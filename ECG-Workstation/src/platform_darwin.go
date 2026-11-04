//go:build darwin

package main

import (
	"fmt"
	"os"
	"os/exec"
)

func openBrowser(url string) error {
	for _, app := range []string{"Google Chrome", "Microsoft Edge", "Brave Browser"} {
		cmd := exec.Command("open", "-na", app, "--args", "--app="+url, "--window-size=1600,1000")
		if err := cmd.Run(); err == nil {
			return nil
		}
	}
	return exec.Command("open", url).Run()
}

func fatalDialog(msg string) {
	fmt.Fprintln(os.Stderr, msg)
}

func windowSize() (int, int) { return 1600, 1000 }
