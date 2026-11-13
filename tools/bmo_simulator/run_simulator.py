#!/usr/bin/env python3
"""Run the browser simulator with a loopback-only, token-protected test endpoint."""

from __future__ import annotations

import hmac
import http.server
import json
import secrets
import socketserver
import subprocess
import sys
import threading
import time
import webbrowser
from collections import deque
from pathlib import Path
from urllib.parse import unquote, urlsplit

PORT = 8080
HOST = "127.0.0.1"
ORIGIN = f"http://{HOST}:{PORT}"
DIRECTORY = Path(__file__).resolve().parent
REPO_ROOT = DIRECTORY.parents[1]
SESSION_TOKEN = secrets.token_urlsafe(32)
RATE_LIMIT_COUNT = 3
RATE_LIMIT_WINDOW_S = 60.0

_rate_lock = threading.Lock()
_request_times: dict[str, deque[float]] = {}
_run_lock = threading.Lock()


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


def allow_host_core_request(client_address: str, now: float | None = None) -> bool:
    """Allow at most three host-test launches per client in each minute."""
    current = time.monotonic() if now is None else now
    with _rate_lock:
        events = _request_times.setdefault(client_address, deque())
        while events and current - events[0] >= RATE_LIMIT_WINDOW_S:
            events.popleft()
        if len(events) >= RATE_LIMIT_COUNT:
            return False
        events.append(current)
        return True


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
        if route == "/api/host-cores/run":
            self.send_response(405)
            self.send_header("Allow", "POST")
            self.send_header("Content-Length", "0")
            self.end_headers()
            return
        if route == "/api/host-cores/token":
            origin = self.headers.get("Origin")
            if origin is not None and not valid_origin_header(origin):
                self._write_json(403, {"status": "error", "message": "Invalid Origin header"})
                return
            self._write_json(200, {"token": SESSION_TOKEN})
            return
        super().do_GET()

    def do_POST(self):
        if not self._host_is_allowed():
            return
        if urlsplit(self.path).path != "/api/host-cores/run":
            self._write_json(404, {"status": "error", "message": "Not found"})
            return
        if not valid_origin_header(self.headers.get("Origin")):
            self._write_json(403, {"status": "error", "message": "Invalid Origin header"})
            return
        if not hmac.compare_digest(self.headers.get("X-BMO-Session", ""), SESSION_TOKEN):
            self._write_json(403, {"status": "error", "message": "Invalid session token"})
            return
        content_length = self.headers.get("Content-Length", "0")
        if content_length not in ("", "0"):
            self._write_json(400, {"status": "error", "message": "Request body is not accepted"})
            return
        client = self.client_address[0]
        if not allow_host_core_request(client):
            self._write_json(429, {"status": "error", "message": "Host-test rate limit reached"})
            return
        if not _run_lock.acquire(blocking=False):
            self._write_json(429, {"status": "error", "message": "Host tests are already running"})
            return

        try:
            cmd = [
                sys.executable,
                "-m",
                "unittest",
                "tests.test_native_cores_execution.TestNativeCoresExecutionAndPhaseD.test_01_compile_and_execute_all_11_c_emulator_cores_50_frames",
            ]
            proc = subprocess.run(
                cmd,
                cwd=str(REPO_ROOT),
                capture_output=True,
                text=True,
                timeout=60,
                shell=False,
            )
            passed = proc.returncode == 0
            self._write_json(
                200 if passed else 500,
                {
                    "status": "success" if passed else "failed",
                    "cores_tested": 11,
                    "all_passed": passed,
                    "message": (
                        "All 11 native C cores passed 50-frame execution and visual regression"
                        if passed
                        else "Host runner reported failure"
                    ),
                    "output": proc.stdout or proc.stderr,
                },
            )
        except subprocess.TimeoutExpired:
            self._write_json(504, {"status": "error", "message": "Host tests timed out"})
        except OSError:
            self._write_json(500, {"status": "error", "message": "Could not start host tests"})
        finally:
            _run_lock.release()


class LoopbackThreadingServer(socketserver.ThreadingTCPServer):
    allow_reuse_address = True
    daemon_threads = True
    request_queue_size = 16


def main():
    print("==================================================================")
    print(" BMO HANDHELD CONSOLE SIMULATOR")
    print("==================================================================")
    print(f"[*] Serving simulator at {ORIGIN}")
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
