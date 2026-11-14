#!/usr/bin/env python3
"""Serve the browser simulator as a read-only loopback static HTTP server."""

from __future__ import annotations

import argparse
import http.server
import json
import socketserver
import sys
import webbrowser
from pathlib import Path
from urllib.parse import unquote, urlsplit

PORT = 8080
HOST = "127.0.0.1"
ORIGIN = f"http://{HOST}:{PORT}"
DIRECTORY = Path(__file__).resolve().parent
CSP_POLICY = (
    "default-src 'self'; "
    "object-src 'none'; "
    "base-uri 'none'; "
    "form-action 'none'; "
    "script-src 'self' 'wasm-unsafe-eval'; "
    "style-src 'self' 'unsafe-inline'; "
    "img-src 'self' data: blob:; "
    "connect-src 'self'"
)


def valid_host_header(value: str | None, port: int = PORT) -> bool:
    """Reject DNS-rebinding hostnames; this server is bound to one loopback host."""
    return value == f"{HOST}:{port}"


def valid_origin_header(value: str | None, port: int = PORT) -> bool:
    return value == f"http://{HOST}:{port}"


def resolve_static_path(url_path: str) -> Path | None:
    """Resolve a URL path only when it stays inside the simulator directory."""
    relative = unquote(urlsplit(url_path).path).lstrip("/")
    candidate = (DIRECTORY / relative).resolve()
    try:
        candidate.relative_to(DIRECTORY.resolve())
    except ValueError:
        return None
    return candidate


class Handler(http.server.SimpleHTTPRequestHandler):
    server_version = "BmoSimulator/1"
    sys_version = ""
    extensions_map = {
        **http.server.SimpleHTTPRequestHandler.extensions_map,
        ".wasm": "application/wasm",
        ".js": "application/javascript; charset=utf-8",
    }

    def __init__(self, *args, **kwargs):
        super().__init__(*args, directory=str(DIRECTORY), **kwargs)

    def end_headers(self):
        self.send_header("Content-Security-Policy", CSP_POLICY)
        self.send_header("X-Content-Type-Options", "nosniff")
        self.send_header("Referrer-Policy", "no-referrer")
        self.send_header("Cross-Origin-Resource-Policy", "same-origin")
        super().end_headers()

    def _write_json(self, status: int, payload: dict):
        body = json.dumps(payload).encode("utf-8")
        self.send_response(status)
        self.send_header("Content-Type", "application/json; charset=utf-8")
        self.send_header("Content-Length", str(len(body)))
        self.send_header("Cache-Control", "no-store")
        self.end_headers()
        self.wfile.write(body)

    def _host_is_allowed(self) -> bool:
        bound_addr = getattr(getattr(self, "server", None), "server_address", None)
        port = bound_addr[1] if isinstance(bound_addr, tuple) and len(bound_addr) >= 2 else PORT
        if valid_host_header(self.headers.get("Host"), port=port):
            return True
        self._write_json(403, {"status": "error", "message": "Invalid Host header"})
        return False

    def translate_path(self, path: str) -> str:
        resolved = resolve_static_path(path)
        if resolved is None:
            return str(DIRECTORY / ".blocked-path")
        return str(resolved)

    def do_GET(self):
        if not self._host_is_allowed():
            return
        route = urlsplit(self.path).path
        if route.startswith("/api/"):
            self._write_json(404, {"status": "error", "message": "Not found"})
            return
        super().do_GET()

    def do_POST(self):
        if not self._host_is_allowed():
            return
        self.send_response(405)
        self.send_header("Allow", "GET, HEAD")
        self.send_header("Content-Length", "0")
        self.end_headers()


class LoopbackThreadingServer(socketserver.ThreadingTCPServer):
    allow_reuse_address = True
    daemon_threads = True
    request_queue_size = 16


def ensure_wasm_artifacts() -> None:
    """Build untracked browser WebAssembly cores on demand when missing."""
    wasm_path = DIRECTORY / "bmo_playtest_cores.wasm"
    b64_path = DIRECTORY / "bmo_playtest_cores.b64.js"
    if wasm_path.is_file() and b64_path.is_file():
        return
    repo_root = DIRECTORY.parent
    if str(repo_root) not in sys.path:
        sys.path.insert(0, str(repo_root))
    print("[*] Building browser WASM cores (bmo_playtest_cores.wasm)...")
    try:
        from scripts.build_wasm_cores import build_wasm_module

        wasm_bytes = build_wasm_module(output_path=wasm_path)
        print(f"[+] Built {wasm_path.name} ({len(wasm_bytes)} bytes)")
    except Exception as exc:  # noqa: BLE001
        print(
            f"[!] Could not auto-build WASM cores: {exc}\n"
            "    Install WASI SDK or set WASI_SDK_PATH, then run: "
            "python scripts/build_wasm_cores.py"
        )


def main(argv=None):
    parser = argparse.ArgumentParser(description="Run the BMO Handheld loopback browser simulator.")
    parser.add_argument("--no-open", action="store_true", help="Start HTTP server without opening a browser tab.")
    parser.add_argument("--port", type=int, default=PORT, help="Loopback TCP port (default: 8080).")
    args = parser.parse_args(argv)

    ensure_wasm_artifacts()
    serve_origin = f"http://{HOST}:{args.port}"
    print("==================================================================")
    print(" BMO HANDHELD CONSOLE SIMULATOR")
    print("==================================================================")
    print(f"[*] Serving simulator at {serve_origin}")
    print("    Run native C core tests from a terminal with unittest.")
    print("    Press Ctrl+C to stop the local simulator server.")
    print("==================================================================")

    with LoopbackThreadingServer((HOST, args.port), Handler) as httpd:
        if not args.no_open:
            webbrowser.open(serve_origin)
        try:
            httpd.serve_forever()
        except KeyboardInterrupt:
            print("\nSimulator stopped.")


if __name__ == "__main__":
    main()
