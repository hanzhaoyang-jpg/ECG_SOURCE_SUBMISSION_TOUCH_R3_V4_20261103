#!/bin/sh
set -e
cd "$(dirname "$0")"
GOOS=windows GOARCH=amd64 CGO_ENABLED=0 \
  go build -trimpath -ldflags "-s -w -H windowsgui" -o ../ECG-Workstation.exe .
echo "built: ../ECG-Workstation.exe"
