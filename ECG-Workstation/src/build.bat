@echo off
rem 需要先安装 Go 1.21+（https://go.dev/dl/）
setlocal
set GOOS=windows
set GOARCH=amd64
set CGO_ENABLED=0
go build -trimpath -ldflags "-s -w -H windowsgui" -o ..\ECG-Workstation.exe .
if errorlevel 1 ( echo build failed ) else ( echo built: ..\ECG-Workstation.exe )
pause
