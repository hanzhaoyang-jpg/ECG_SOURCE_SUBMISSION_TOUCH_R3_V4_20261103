package main

import (
	"encoding/binary"
	"fmt"
	"net"
	"time"
)

const (
	magicV1 uint32 = 0x31474345 // "ECG1"（当前固件 ecg_net.h）
	magicV2 uint16 = 0xEC61     // 方案 v2
)

// runUDP 监听板卡推流；收到包即进入实时模式。
func (a *App) runUDP() {
	addr, err := net.ResolveUDPAddr("udp4", fmt.Sprintf("0.0.0.0:%d", a.udpPort))
	if err != nil {
		a.log.Printf("udp resolve: %v", err)
		return
	}
	conn, err := net.ListenUDP("udp4", addr)
	if err != nil {
		a.log.Printf("udp listen :%d failed (%v); live mode disabled", a.udpPort, err)
		return
	}
	defer conn.Close()
	a.log.Printf("udp listening on :%d", a.udpPort)

	buf := make([]byte, 4096)
	for {
		n, _, err := conn.ReadFromUDP(buf)
		if err != nil {
			select {
			case <-a.done:
				return
			default:
			}
			a.log.Printf("udp read: %v", err)
			time.Sleep(200 * time.Millisecond)
			continue
		}
		if ss, ok := parsePacket(buf[:n]); ok {
			a.addSamples(ss)
		}
	}
}

func parsePacket(b []byte) ([]sample, bool) {
	if ss, ok := parseV1(b); ok {
		return ss, true
	}
	return parseV2(b)
}

// 当前固件：28 字节 packed 结构，一发一个样本。
func parseV1(b []byte) ([]sample, bool) {
	if len(b) < 28 || binary.LittleEndian.Uint32(b[0:4]) != magicV1 {
		return nil, false
	}
	if binary.LittleEndian.Uint16(b[4:6]) != 1 {
		return nil, false
	}
	if binary.LittleEndian.Uint16(b[6:8]) != 1 { // 2 = event，界面暂不使用
		return nil, false
	}
	return []sample{{
		idx: int64(binary.LittleEndian.Uint64(b[12:20])),
		v:   int16(binary.LittleEndian.Uint16(b[24:26])),
		cls: b[26],
		fl:  b[27],
	}}, true
}

// 方案 v2：8 样本一包，一次解出 n 个点。
func parseV2(b []byte) ([]sample, bool) {
	if len(b) < 24 || binary.LittleEndian.Uint16(b[0:2]) != magicV2 || b[2] != 2 {
		return nil, false
	}
	if b[3] != 0x02 { // WAVE
		return nil, false
	}
	n := int(b[20])
	if n <= 0 || n > 32 || len(b) < 24+2*n {
		return nil, false
	}
	base := int64(binary.LittleEndian.Uint64(b[8:16]))
	cls := b[22]
	flagsAt := 24 + 2*n
	out := make([]sample, n)
	for k := 0; k < n; k++ {
		var fl uint8
		if flagsAt+k < len(b) {
			fl = b[flagsAt+k]
		}
		out[k] = sample{
			idx: base + int64(k),
			v:   int16(binary.LittleEndian.Uint16(b[24+2*k : 26+2*k])),
			cls: cls,
			fl:  fl,
		}
	}
	return out, true
}
