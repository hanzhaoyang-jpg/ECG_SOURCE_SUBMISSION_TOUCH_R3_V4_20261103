//go:build linux

package main

import (
	"fmt"
	"os"
	"os/exec"
)

func openBrowser(url string) error {
	return exec.Command("xdg-open", url).Start()
}

func fatalDialog(msg string) {
	fmt.Fprintln(os.Stderr, msg)
}

func windowSize() (int, int) { return 1600, 1000 }
