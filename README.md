# WASI Preview 2 TCP close with unread data

This isolates the socket-close behavior discussed in
[Wasmtime PR #14518](https://github.com/bytecodealliance/wasmtime/pull/14518).
The same C server is compiled for the native host and for WASI Preview 2.
A Python TCP peer sends a one-byte request (`q`) and receives a one-byte reply
(`r`). It then sends an extra byte (`x`). The server waits until `x` is readable,
does **not** read it, and closes the connection without calling `shutdown`.
The peer reports whether its next read sees a reset or EOF.

## Run

```sh
cc -O0 -Wall -Wextra server.c -o server-native
/path/to/wasi-sdk/bin/wasm32-wasip2-clang -O0 -Wall -Wextra server.c -o server.wasm
python3 peer.py native
WASMTIME=/path/to/wasmtime python3 peer.py p2
```

The `WASMTIME` variable is optional if the desired `wasmtime` executable is
already on `PATH`. The P2 run grants loopback TCP access with
`-S inherit-network=y -S tcp=y`.

## Observed on macOS arm64

With Wasmtime 48.0.1:

```text
$ python3 peer.py native
closing with the extra byte unread
peer observed after the reply: RST (ECONNRESET)

$ WASMTIME=/path/to/wasmtime-48.0.1 python3 peer.py p2
closing with the extra byte unread
peer observed after the reply: EOF
```

The readiness wait makes the unread extra byte explicit. The server then waits
100 ms so the peer can block in its final read before the close; it never reads
the extra byte. The request/reply exchange shows that the peer received the
complete reply before the close.
It demonstrates a difference under these conditions; it does not establish
that every close sends a reset, or that an application depends on one.
Results can differ by Wasmtime version and operating system. For example,
Wasmtime 47.0.2 produced a reset in this same setup on macOS arm64.
