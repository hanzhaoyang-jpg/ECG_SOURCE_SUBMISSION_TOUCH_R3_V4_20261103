//go:build windows

package main

import (
	"fmt"
	"os"
	"os/exec"
	"path/filepath"
	"syscall"
	"unsafe"
)

// openBrowser 优先用 Edge/Chrome 的 app 模式打开：无地址栏、独立窗口，最接近"软件"。
func openBrowser(url string) error {
	w, h := windowSize()
	size := fmt.Sprintf("--window-size=%d,%d", w, h)
	candidates := []string{
		filepath.Join(os.Getenv("ProgramFiles(x86)"), `Microsoft\Edge\Application\msedge.exe`),
		filepath.Join(os.Getenv("ProgramFiles"), `Microsoft\Edge\Application\msedge.exe`),
		filepath.Join(os.Getenv("ProgramFiles"), `Google\Chrome\Application\chrome.exe`),
		filepath.Join(os.Getenv("ProgramFiles(x86)"), `Google\Chrome\Application\chrome.exe`),
		filepath.Join(os.Getenv("LOCALAPPDATA"), `Google\Chrome\Application\chrome.exe`),
	}
	for _, p := range candidates {
		if p == "" {
			continue
		}
		st, err := os.Stat(p)
		if err != nil || st.IsDir() {
			continue
		}
		cmd := exec.Command(p, "--app="+url, size,
			"--no-first-run", "--no-default-browser-check")
		if err := cmd.Start(); err == nil {
			go func() { _ = cmd.Wait() }()
			return nil
		}
	}
	return exec.Command("rundll32", "url.dll,FileProtocolHandler", url).Start()
}

// windowSize 按屏幕大小收缩窗口，避免在小屏笔记本上开出一个超出屏幕的窗口。
func windowSize() (int, int) {
	const wantW, wantH = 1600, 1000
	user32 := syscall.NewLazyDLL("user32.dll")
	get := user32.NewProc("GetSystemMetrics")
	sw, _, _ := get.Call(0)
	sh, _, _ := get.Call(1)
	scrW, scrH := int(sw), int(sh)
	if scrW < 800 || scrH < 600 {
		return wantW, wantH
	}
	w, h := wantW, wantH
	if w > scrW-40 {
		w = scrW - 40
	}
	if h > scrH-80 {
		h = scrH - 80
	}
	return w, h
}

func fatalDialog(msg string) {
	user32 := syscall.NewLazyDLL("user32.dll")
	mb := user32.NewProc("MessageBoxW")
	text, err1 := syscall.UTF16PtrFromString(msg)
	title, err2 := syscall.UTF16PtrFromString("ECG 工作站")
	if err1 != nil || err2 != nil {
		return
	}
	const mbIconError = 0x10
	_, _, _ = mb.Call(0,
		uintptr(unsafe.Pointer(text)),
		uintptr(unsafe.Pointer(title)),
		uintptr(mbIconError))
}
