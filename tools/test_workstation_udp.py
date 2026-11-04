"""Smoke-test the supplied Windows workstation with synthetic v1 samples."""
import argparse
import http.client
import json
from pathlib import Path
import socket
import struct
import subprocess
import time
import urllib.request


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("exe", type=Path)
    args = parser.parse_args()
    opener = urllib.request.build_opener(urllib.request.ProxyHandler({}))
    with socket.socket() as probe:
        probe.bind(("127.0.0.1", 0))
        http_port = probe.getsockname()[1]
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as sender:
        sender.bind(("127.0.0.1", 0))
        with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as probe:
            probe.bind(("0.0.0.0", 0))
            udp_port = probe.getsockname()[1]
        base = f"http://127.0.0.1:{http_port}"
        exe = args.exe.resolve(strict=True)
        process = subprocess.Popen(
            [str(exe), "-open=false", f"-port={http_port}", f"-udp={udp_port}"],
            cwd=exe.parent, creationflags=subprocess.CREATE_NO_WINDOW,
            stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
        )
        connection = http.client.HTTPConnection("127.0.0.1", http_port, timeout=3)
        try:
            for _ in range(40):
                if process.poll() is not None:
                    raise RuntimeError("Workstation exited before accepting HTTP")
                try:
                    with opener.open(base + "/api/status", timeout=0.3) as response:
                        before = json.load(response)
                    break
                except OSError as exc:
                    startup_error = exc
                    time.sleep(0.1)
            else:
                raise RuntimeError("Workstation HTTP startup timed out") from startup_error
            assert before["app"] == "ecg-workstation" and before["udp"] == udp_port
            assert before["fs"] == 360 and not before["live"]
            connection.request("GET", "/api/stream")
            stream = connection.getresponse()
            assert stream.status == 200
            for _ in range(20):
                if b'"t":"hello"' in stream.readline():
                    break
            else:
                raise RuntimeError("No SSE hello")
            wire = struct.pack("<IHHIQIhBB", 0x31474345, 1, 1, 8, 1234, 3427, -321, 2, 3)
            assert len(wire) == 28
            sender.sendto(wire, ("127.0.0.1", udp_port))
            for _ in range(20):
                line = stream.readline()
                if line.startswith(b"data: "):
                    event = json.loads(line[6:])
                    if event["t"] == "w":
                        assert event == {"t": "w", "i0": 1234, "v": [-321], "c": 2, "f": 3}
                        break
            else:
                raise RuntimeError("No waveform SSE event")
            with opener.open(base + "/api/status", timeout=2) as response:
                after = json.load(response)
            assert after["live"] and after["pkts"] == 1
            print("PASS Windows workstation UDP v1 -> SSE: signed waveform, sample index, class, flags, live status")
        finally:
            connection.close()
            if process.poll() is None:
                try:
                    req = urllib.request.Request(base + "/api/quit", method="POST")
                    with opener.open(req, timeout=1):
                        pass
                    process.wait(timeout=3)
                except (OSError, subprocess.TimeoutExpired):
                    process.terminate()
                    process.wait(timeout=3)


if __name__ == "__main__":
    main()
