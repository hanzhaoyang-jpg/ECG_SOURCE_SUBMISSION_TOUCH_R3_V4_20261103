package main

import (
	"bufio"
	"embed"
	"encoding/json"
	"fmt"
	"io/fs"
	"log"
	"net"
	"net/http"
	"os"
	"path/filepath"
	"strconv"
	"sync"
	"sync/atomic"
	"time"
)

//go:embed web
var webFS embed.FS

const (
	fsHz        = 360
	ringSeconds = 60
	ringN       = fsHz * ringSeconds
	snapSeconds = 10
)

// ---------------------------------------------------------------- messages

type sample struct {
	idx int64
	v   int16
	cls uint8
	fl  uint8
}

type helloMsg struct {
	T       string   `json:"t"`
	Version string   `json:"version"`
	UDP     int      `json:"udp"`
	FS      int      `json:"fs"`
	IPs     []string `json:"ips"`
}

type snapMsg struct {
	T  string  `json:"t"`
	I0 int64   `json:"i0"`
	V  []int16 `json:"v"`
}

type waveMsg struct {
	T  string  `json:"t"`
	I0 int64   `json:"i0"`
	V  []int16 `json:"v"`
	C  int     `json:"c"`
	F  int     `json:"f"` // 最近一个样本的 flags：bit0 LOD, bit1 OTR
}

type statMsg struct {
	T    string `json:"t"`
	Live bool   `json:"live"`
	Pkts uint64 `json:"pkts"`
	Lost uint64 `json:"lost"`
}

// -------------------------------------------------------------------- hub

type Hub struct {
	mu      sync.RWMutex
	clients map[chan []byte]struct{}

	ring  [ringN]int16
	start int64 // 最早有效样本
	end   int64 // 下一个待写 index
}

func newHub() *Hub {
	return &Hub{clients: make(map[chan []byte]struct{})}
}

func (h *Hub) subscribe() chan []byte {
	ch := make(chan []byte, 32)
	h.mu.Lock()
	h.clients[ch] = struct{}{}
	h.mu.Unlock()
	return ch
}

func (h *Hub) unsubscribe(ch chan []byte) {
	h.mu.Lock()
	if _, ok := h.clients[ch]; ok {
		delete(h.clients, ch)
		close(ch)
	}
	h.mu.Unlock()
}

func (h *Hub) append(idx int64, v int16) {
	h.mu.Lock()
	if h.end == 0 {
		h.start, h.end = idx, idx
	}
	if idx >= h.end {
		h.end = idx + 1
	}
	if idx < h.start {
		h.start = idx
	}
	if h.end-h.start > ringN {
		h.start = h.end - ringN
	}
	h.ring[mod(idx, ringN)] = v
	h.mu.Unlock()
}

func (h *Hub) snapshot() (int64, []int16) {
	h.mu.RLock()
	defer h.mu.RUnlock()

	start := h.end - snapSeconds*fsHz
	if start < h.start {
		start = h.start
	}
	if start < 0 || h.end <= start {
		return 0, nil
	}
	out := make([]int16, h.end-start)
	for i := range out {
		out[i] = h.ring[mod(start+int64(i), ringN)]
	}
	return start, out
}

func (h *Hub) broadcast(b []byte) {
	h.mu.RLock()
	defer h.mu.RUnlock()
	for ch := range h.clients {
		select {
		case ch <- b:
		default: // 慢客户端丢帧，绝不阻塞采集侧
		}
	}
}

func mod(a, n int64) int64 {
	m := a % n
	if m < 0 {
		m += n
	}
	return m
}

// -------------------------------------------------------------------- app

type App struct {
	port    int
	udpPort int
	log     *log.Logger

	mu      sync.Mutex
	pending []sample

	hub *Hub
	rec *recorder

	pkts uint64
	lost uint64

	lastPkt   atomic.Int64
	connected atomic.Bool
	lastHB    atomic.Int64

	started  time.Time
	done     chan struct{}
	doneOnce sync.Once
}

func newApp(udpPort int, logger *log.Logger) *App {
	return &App{
		udpPort: udpPort,
		log:     logger,
		hub:     newHub(),
		started: time.Now(),
		done:    make(chan struct{}),
	}
}

func (a *App) shutdown(reason string) {
	a.doneOnce.Do(func() {
		a.log.Printf("shutdown: %s", reason)
		if a.rec != nil {
			a.rec.stop()
		}
		close(a.done)
	})
}

func (a *App) runTicker() {
	flush := time.NewTicker(50 * time.Millisecond)
	stat := time.NewTicker(time.Second)
	hb := time.NewTicker(10 * time.Second)
	defer flush.Stop()
	defer stat.Stop()
	defer hb.Stop()

	for {
		select {
		case <-a.done:
			return
		case <-flush.C:
			a.flush()
		case <-stat.C:
			a.broadcastJSON(statMsg{
				T:    "st",
				Live: time.Since(time.UnixMilli(a.lastPkt.Load())) < 2*time.Second,
				Pkts: atomic.LoadUint64(&a.pkts),
				Lost: atomic.LoadUint64(&a.lost),
			})
		case <-hb.C:
			// 页面关掉后（没有心跳）自动退出，避免留下后台进程
			if !a.connected.Load() {
				if time.Since(a.started) > 5*time.Minute {
					a.shutdown("no UI connected")
				}
				continue
			}
			if time.Since(time.UnixMilli(a.lastHB.Load())) > 120*time.Second {
				a.shutdown("no heartbeat from UI")
			}
		}
	}
}

func (a *App) flush() {
	a.mu.Lock()
	ps := a.pending
	a.pending = nil
	rec := a.rec
	a.mu.Unlock()
	if len(ps) == 0 {
		return
	}

	last := ps[len(ps)-1]
	m := waveMsg{T: "w", I0: ps[0].idx, C: int(last.cls), F: int(last.fl)}
	m.V = make([]int16, len(ps))
	for i, s := range ps {
		m.V[i] = s.v
		a.hub.append(s.idx, s.v)
	}
	a.broadcastJSON(m)

	if rec != nil {
		for _, s := range ps {
			rec.write(s)
		}
	}
}

func (a *App) broadcastJSON(v any) {
	b, err := json.Marshal(v)
	if err != nil {
		return
	}
	a.hub.broadcast(b)
}

func (a *App) addSamples(ss []sample) {
	if len(ss) == 0 {
		return
	}
	atomic.AddUint64(&a.pkts, uint64(len(ss)))
	a.lastPkt.Store(time.Now().UnixMilli())

	a.mu.Lock()
	for _, s := range ss {
		if len(a.pending) >= 4096 {
			atomic.AddUint64(&a.lost, 1)
			continue
		}
		a.pending = append(a.pending, s)
	}
	a.mu.Unlock()
}

// ------------------------------------------------------------------ routes

func (a *App) routes() http.Handler {
	mux := http.NewServeMux()
	mux.HandleFunc("/api/ping", a.handlePing)
	mux.HandleFunc("/api/status", a.handleStatus)
	mux.HandleFunc("/api/hb", a.handleHeartbeat)
	mux.HandleFunc("/api/quit", a.handleQuit)
	mux.HandleFunc("/api/record/start", a.handleRecordStart)
	mux.HandleFunc("/api/record/stop", a.handleRecordStop)
	mux.HandleFunc("/api/stream", a.handleStream)

	sub, err := fs.Sub(webFS, "web")
	if err != nil {
		panic(err)
	}
	files := http.FileServer(http.FS(sub))
	mux.Handle("/", http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		w.Header().Set("Cache-Control", "no-store")
		files.ServeHTTP(w, r)
	}))
	return mux
}

func (a *App) handlePing(w http.ResponseWriter, r *http.Request) {
	writeJSON(w, 200, map[string]any{"app": "ecg-workstation", "version": Version})
}

func (a *App) handleStatus(w http.ResponseWriter, r *http.Request) {
	writeJSON(w, 200, map[string]any{
		"app":     "ecg-workstation",
		"version": Version,
		"live":    time.Since(time.UnixMilli(a.lastPkt.Load())) < 2*time.Second,
		"pkts":    atomic.LoadUint64(&a.pkts),
		"lost":    atomic.LoadUint64(&a.lost),
		"udp":     a.udpPort,
		"fs":      fsHz,
	})
}

func (a *App) handleHeartbeat(w http.ResponseWriter, r *http.Request) {
	a.connected.Store(true)
	a.lastHB.Store(time.Now().UnixMilli())
	w.WriteHeader(http.StatusNoContent)
}

func (a *App) handleQuit(w http.ResponseWriter, r *http.Request) {
	writeJSON(w, 200, map[string]any{"ok": true})
	go func() {
		time.Sleep(120 * time.Millisecond)
		a.shutdown("quit from UI")
	}()
}

func (a *App) handleRecordStart(w http.ResponseWriter, r *http.Request) {
	a.mu.Lock()
	defer a.mu.Unlock()
	if a.rec != nil {
		n, path := a.rec.stats()
		writeJSON(w, 200, map[string]any{"ok": true, "file": path, "lines": n})
		return
	}
	rec, err := newRecorder(dataDir())
	if err != nil {
		a.log.Printf("record start: %v", err)
		writeJSON(w, 500, map[string]any{"ok": false, "error": err.Error()})
		return
	}
	a.rec = rec
	a.log.Printf("recording to %s", rec.path)
	writeJSON(w, 200, map[string]any{"ok": true, "file": rec.path, "lines": 0})
}

func (a *App) handleRecordStop(w http.ResponseWriter, r *http.Request) {
	a.mu.Lock()
	rec := a.rec
	a.rec = nil
	a.mu.Unlock()
	if rec == nil {
		writeJSON(w, 200, map[string]any{"ok": true, "file": "", "lines": 0})
		return
	}
	n, path := rec.stats()
	rec.stop()
	a.log.Printf("recording stopped: %s (%d lines)", path, n)
	writeJSON(w, 200, map[string]any{"ok": true, "file": path, "lines": n})
}

func (a *App) handleStream(w http.ResponseWriter, r *http.Request) {
	flusher, ok := w.(http.Flusher)
	if !ok {
		http.Error(w, "streaming unsupported", http.StatusInternalServerError)
		return
	}
	w.Header().Set("Content-Type", "text/event-stream")
	w.Header().Set("Cache-Control", "no-cache")
	w.Header().Set("Connection", "keep-alive")
	w.Header().Set("X-Accel-Buffering", "no")
	w.WriteHeader(http.StatusOK)
	fmt.Fprint(w, "retry: 2000\n\n")
	flusher.Flush()

	a.connected.Store(true)
	a.lastHB.Store(time.Now().UnixMilli())

	ch := a.hub.subscribe()
	defer a.hub.unsubscribe(ch)

	writeEvent(w, helloMsg{T: "hello", Version: Version, UDP: a.udpPort, FS: fsHz, IPs: localIPv4s()})
	if i0, v := a.hub.snapshot(); len(v) > 0 {
		writeEvent(w, snapMsg{T: "snap", I0: i0, V: v})
	}
	flusher.Flush()

	keep := time.NewTicker(15 * time.Second)
	defer keep.Stop()

	for {
		select {
		case <-r.Context().Done():
			return
		case <-a.done:
			return
		case b, ok := <-ch:
			if !ok {
				return
			}
			fmt.Fprintf(w, "data: %s\n\n", b)
			flusher.Flush()
		case <-keep.C:
			fmt.Fprint(w, ": keepalive\n\n")
			flusher.Flush()
		}
	}
}

// localIPv4s 列出本机网卡地址，方便把板卡的 PEER_IP 配成这台电脑。
func localIPv4s() []string {
	var out []string
	ifaces, err := net.Interfaces()
	if err != nil {
		return out
	}
	for _, ifc := range ifaces {
		if ifc.Flags&net.FlagUp == 0 || ifc.Flags&net.FlagLoopback != 0 {
			continue
		}
		addrs, err := ifc.Addrs()
		if err != nil {
			continue
		}
		for _, a := range addrs {
			ipnet, ok := a.(*net.IPNet)
			if !ok {
				continue
			}
			if v4 := ipnet.IP.To4(); v4 != nil {
				out = append(out, v4.String())
			}
		}
	}
	return out
}

func writeEvent(w http.ResponseWriter, v any) {
	b, err := json.Marshal(v)
	if err != nil {
		return
	}
	fmt.Fprintf(w, "data: %s\n\n", b)
}

func writeJSON(w http.ResponseWriter, code int, v any) {
	w.Header().Set("Content-Type", "application/json; charset=utf-8")
	w.WriteHeader(code)
	_ = json.NewEncoder(w).Encode(v)
}

// --------------------------------------------------------------- recorder

type recorder struct {
	mu   sync.Mutex
	f    *os.File
	w    *bufio.Writer
	path string
	n    int
}

func newRecorder(base string) (*recorder, error) {
	dir := filepath.Join(base, "records")
	if err := os.MkdirAll(dir, 0o755); err != nil {
		return nil, err
	}
	name := "ECG_" + time.Now().Format("20060102_150405") + ".csv"
	path := filepath.Join(dir, name)
	f, err := os.Create(path)
	if err != nil {
		return nil, err
	}
	r := &recorder{f: f, w: bufio.NewWriterSize(f, 64*1024), path: path}
	r.w.WriteString("sample_index,timestamp_ms,filtered_mV,class_id,flags\n")
	return r, nil
}

func (r *recorder) write(s sample) {
	if r == nil {
		return
	}
	r.mu.Lock()
	defer r.mu.Unlock()
	if r.f == nil {
		return
	}
	fmt.Fprintf(r.w, "%d,%d,%s,%d,%d\n",
		s.idx, s.idx*1000/fsHz,
		strconv.FormatFloat(float64(s.v)/1000, 'f', 3, 64),
		s.cls, s.fl)
	r.n++
	if r.n%720 == 0 {
		_ = r.w.Flush()
	}
}

func (r *recorder) stats() (int, string) {
	if r == nil {
		return 0, ""
	}
	r.mu.Lock()
	defer r.mu.Unlock()
	return r.n, r.path
}

func (r *recorder) stop() {
	if r == nil {
		return
	}
	r.mu.Lock()
	defer r.mu.Unlock()
	if r.f == nil {
		return
	}
	_ = r.w.Flush()
	_ = r.f.Close()
	r.f = nil
}
