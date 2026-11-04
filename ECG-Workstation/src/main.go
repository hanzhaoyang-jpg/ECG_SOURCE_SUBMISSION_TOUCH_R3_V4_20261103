// ECG Workstation - 单文件桌面版
//
// 一个 exe 内嵌前端页面，本地起 HTTP/SSE 服务，同时监听板卡 UDP 推流。
// 没有板卡数据时前端自动进入演示（模拟）模式。
package main

import (
	"context"
	"encoding/json"
	"errors"
	"flag"
	"fmt"
	"io"
	"log"
	"net"
	"net/http"
	"os"
	"path/filepath"
	"runtime"
	"time"
)

const Version = "1.1.0"

var (
	flagPort = flag.Int("port", 8765, "本地 HTTP 端口")
	flagUDP  = flag.Int("udp", 5005, "板卡 UDP 监听端口")
	flagOpen = flag.Bool("open", true, "启动后自动打开界面")
)

var errRunning = errors.New("instance already running")

func main() {
	flag.Parse()

	logger, closeLog := initLog()
	defer closeLog()

	ln, port, err := acquirePort(*flagPort)
	if errors.Is(err, errRunning) {
		// 已经有一个实例在跑，直接把它叫到前台
		openBrowser(appURL(port))
		return
	}
	if err != nil {
		logger.Printf("listen failed: %v", err)
		fatalDialog("ECG 工作站启动失败\n\n无法监听本地端口：" + err.Error())
		return
	}

	app := newApp(*flagUDP, logger)
	app.port = port

	srv := &http.Server{Handler: app.routes()}
	go func() {
		if err := srv.Serve(ln); err != nil && !errors.Is(err, http.ErrServerClosed) {
			logger.Printf("http server: %v", err)
		}
	}()

	go app.runUDP()
	go app.runTicker()

	url := appURL(port)
	logger.Printf("ECG Workstation %s started  http=%s  udp=%d  data=%s",
		Version, url, *flagUDP, dataDir())

	if *flagOpen {
		go func() {
			time.Sleep(250 * time.Millisecond)
			if err := openBrowser(url); err != nil {
				logger.Printf("open browser: %v", err)
				fatalDialog("无法自动打开浏览器。\n\n请手动访问：\n" + url)
			}
		}()
	}

	<-app.done
	logger.Println("shutting down")
	ctx, cancel := context.WithTimeout(context.Background(), 1500*time.Millisecond)
	defer cancel()
	_ = srv.Shutdown(ctx)
}

func appURL(port int) string {
	return fmt.Sprintf("http://127.0.0.1:%d/", port)
}

// acquirePort 依次尝试端口；若发现同程序的实例则返回 errRunning。
func acquirePort(preferred int) (net.Listener, int, error) {
	for p := preferred; p < preferred+10; p++ {
		ln, err := net.Listen("tcp", fmt.Sprintf("127.0.0.1:%d", p))
		if err == nil {
			return ln, p, nil
		}
		if pingIsOurs(p) {
			return nil, p, errRunning
		}
	}
	return nil, preferred, fmt.Errorf("ports %d-%d are busy", preferred, preferred+9)
}

func pingIsOurs(port int) bool {
	client := &http.Client{Timeout: 500 * time.Millisecond}
	resp, err := client.Get(fmt.Sprintf("http://127.0.0.1:%d/api/ping", port))
	if err != nil {
		return false
	}
	defer resp.Body.Close()
	var v struct {
		App string `json:"app"`
	}
	if err := json.NewDecoder(io.LimitReader(resp.Body, 512)).Decode(&v); err != nil {
		return false
	}
	return v.App == "ecg-workstation"
}

// dataDir 优先使用 exe 所在目录，不可写时退回用户目录。
func dataDir() string {
	if exe, err := os.Executable(); err == nil {
		dir := filepath.Dir(exe)
		probe := filepath.Join(dir, ".write-test")
		if err := os.WriteFile(probe, []byte("ok"), 0o644); err == nil {
			_ = os.Remove(probe)
			return dir
		}
	}
	if v := os.Getenv("LOCALAPPDATA"); v != "" {
		return filepath.Join(v, "ECG-Workstation")
	}
	if home, err := os.UserHomeDir(); err == nil {
		return filepath.Join(home, ".ecg-workstation")
	}
	return os.TempDir()
}

func initLog() (*log.Logger, func()) {
	dir := dataDir()
	_ = os.MkdirAll(dir, 0o755)
	f, err := os.OpenFile(filepath.Join(dir, "ecg-workstation.log"),
		os.O_CREATE|os.O_APPEND|os.O_WRONLY, 0o644)
	if err != nil {
		return log.New(io.Discard, "", 0), func() {}
	}
	var w io.Writer = f
	if runtime.GOOS != "windows" {
		w = io.MultiWriter(f, os.Stderr)
	}
	return log.New(w, "", log.LstdFlags), func() { _ = f.Close() }
}
