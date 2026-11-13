#!/usr/bin/env python3
"""Serve the browser simulator as a read-only loopback static HTTP server."""

from __future__ import annotations

import http.server
import json
import socketserver
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


def valid_host_header(value: str | None) -> bool:
    """Reject DNS-rebinding hostnames; this server is bound to one loopback host."""
    return value == f"{HOST}:{PORT}"


def valid_origin_header(value: str | None) -> bool:
    return value == ORIGIN


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
        if valid_host_header(self.headers.get("Host")):
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


def main():
    print("==================================================================")
    print(" BMO HANDHELD CONSOLE SIMULATOR")
    print("==================================================================")
    print(f"[*] Serving simulator at {ORIGIN}")
    print("    Run native C core tests from a terminal with unittest.")
    print("    Press Ctrl+C to stop the local simulator server.")
    print("==================================================================")

    with LoopbackThreadingServer((HOST, PORT), Handler) as httpd:
        webbrowser.open(ORIGIN)
        try:
            httpd.serve_forever()
        except KeyboardInterrupt:
            print("\nSimulator stopped.")


if __name__ == "__main__":
    main()
