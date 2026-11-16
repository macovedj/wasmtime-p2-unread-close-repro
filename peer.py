#!/usr/bin/env python3
"""Send a tiny request and an extra byte to the native or WASI P2 server."""

import errno
import os
import socket
import subprocess
import sys


def main():
    if len(sys.argv) != 2 or sys.argv[1] not in ("native", "p2"):
        raise SystemExit("usage: python3 peer.py native|p2")

    command = (["./server-native"] if sys.argv[1] == "native" else
               [os.environ.get("WASMTIME", "wasmtime"), "run",
                "-S", "inherit-network=y", "-S", "tcp=y", "server.wasm"])
    server = subprocess.Popen(command, stdout=subprocess.PIPE,
                              stderr=subprocess.PIPE, text=True)
    try:
        ready = server.stdout.readline().strip()
        if not ready.startswith("PORT "):
            raise RuntimeError(f"server did not announce a port: {ready!r}")
        port = int(ready.removeprefix("PORT "))

        with socket.create_connection(("127.0.0.1", port), timeout=10) as peer:
            peer.sendall(b"q")
            if peer.recv(1) != b"r":
                raise RuntimeError("did not receive the reply")
            peer.sendall(b"x")

            # Observe the first close event, before waiting for process exit.
            try:
                data = peer.recv(1)
                result = "EOF" if data == b"" else f"unexpected data: {data!r}"
            except OSError as exc:
                result = ("RST (ECONNRESET)" if exc.errno == errno.ECONNRESET
                          else f"socket error: {exc}")

            stdout, stderr = server.communicate(timeout=10)
            print(stdout, end="")
            if stderr:
                print(stderr, file=sys.stderr, end="")
            if server.returncode != 0:
                raise SystemExit(f"server exited with {server.returncode}")

            print(f"peer observed after the reply: {result}")
    finally:
        if server.poll() is None:
            server.kill()
            server.wait()


if __name__ == "__main__":
    main()
