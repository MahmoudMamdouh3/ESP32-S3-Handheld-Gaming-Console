#!/usr/bin/env python3
"""Serve the browser simulator as a read-only loopback static HTTP server."""

from __future__ import annotations

import argparse
import http.server
import json
import re
import socketserver
import sys
import threading
import webbrowser
from pathlib import Path
from typing import Any
from urllib.parse import unquote, urlsplit

try:
    from simulator.local_library import (
        IMAGE_MIME_TYPES,
        MAX_LOCAL_ROM_BYTES,
        LocalLibraryCatalog,
        build_local_library_catalog,
        decode_raw_bgr565_to_bmp,
    )
except ImportError:
    from local_library import (  # type: ignore[no-redef]
        IMAGE_MIME_TYPES,
        MAX_LOCAL_ROM_BYTES,
        LocalLibraryCatalog,
        build_local_library_catalog,
        decode_raw_bgr565_to_bmp,
    )

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
    if ".." in relative.replace("\\", "/").split("/"):
        return None
    candidate = (DIRECTORY / relative).resolve()
    try:
        candidate.relative_to(DIRECTORY.resolve())
    except ValueError:
        return None
    return candidate


_ROM_TOKEN_RE = re.compile(r"^r[0-9a-f]{15}$")
_COVER_TOKEN_RE = re.compile(r"^c[0-9a-f]{15}$")
_CATALOG_LOCK = threading.Lock()
_CACHED_CATALOG: LocalLibraryCatalog | None = None


def get_local_catalog(refresh: bool = False) -> LocalLibraryCatalog:
    """Return cached local ROM & box-art catalog, scanning read-only on first use."""
    global _CACHED_CATALOG
    with _CATALOG_LOCK:
        if _CACHED_CATALOG is None or refresh:
            _CACHED_CATALOG = build_local_library_catalog(DIRECTORY.parent)
        return _CACHED_CATALOG


class Handler(http.server.SimpleHTTPRequestHandler):
    server_version = "BmoSimulator/1"
    sys_version = ""
    extensions_map = {  # noqa: RUF012
        **http.server.SimpleHTTPRequestHandler.extensions_map,
        ".wasm": "application/wasm",
        ".js": "application/javascript; charset=utf-8",
    }

    def __init__(self, *args: Any, **kwargs: Any) -> None:
        super().__init__(*args, directory=str(DIRECTORY), **kwargs)

    def end_headers(self) -> None:
        self.send_header("Content-Security-Policy", CSP_POLICY)
        self.send_header("X-Content-Type-Options", "nosniff")
        self.send_header("Referrer-Policy", "no-referrer")
        self.send_header("Cross-Origin-Resource-Policy", "same-origin")
        super().end_headers()

    def _write_bytes(
        self,
        body: bytes,
        content_type: str,
        cache_control: str = "no-store",
        head_only: bool = False,
    ) -> None:
        self.send_response(200)
        self.send_header("Content-Type", content_type)
        self.send_header("Content-Length", str(len(body)))
        self.send_header("Cache-Control", cache_control)
        self.end_headers()
        if not head_only:
            self.wfile.write(body)

    def _write_json(self, status: int, payload: dict[str, Any], head_only: bool = False) -> None:
        body = json.dumps(payload).encode("utf-8")
        self.send_response(status)
        self.send_header("Content-Type", "application/json; charset=utf-8")
        self.send_header("Content-Length", str(len(body)))
        self.send_header("Cache-Control", "no-store")
        self.end_headers()
        if not head_only:
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

    def _serve_local_library(self, route: str, query: str, head_only: bool = False) -> bool:
        if route == "/local-library/manifest.json":
            refresh = "refresh=1" in query
            catalog = get_local_catalog(refresh=refresh)
            self._write_bytes(catalog.manifest_bytes, "application/json; charset=utf-8", head_only=head_only)
            return True
        if route.startswith("/local-library/rom/"):
            self._serve_local_rom(route.removeprefix("/local-library/rom/"), head_only=head_only)
            return True
        if route.startswith("/local-library/cover/"):
            self._serve_local_cover(route.removeprefix("/local-library/cover/"), head_only=head_only)
            return True
        return False

    def _serve_local_rom(self, token: str, head_only: bool = False) -> None:
        if not _ROM_TOKEN_RE.fullmatch(token):
            self._write_json(404, {"status": "error", "message": "Not found"}, head_only=head_only)
            return
        rom_path = get_local_catalog().rom_paths.get(token)
        if rom_path is None or rom_path.is_symlink() or not rom_path.is_file():
            self._write_json(404, {"status": "error", "message": "Not found"}, head_only=head_only)
            return
        try:
            size = rom_path.stat().st_size
            if size > MAX_LOCAL_ROM_BYTES:
                self._write_json(413, {"status": "error", "message": "ROM exceeds 32 MiB limit"}, head_only=head_only)
                return
            body = b"" if head_only else rom_path.read_bytes()
        except OSError:
            self._write_json(404, {"status": "error", "message": "Not found"}, head_only=head_only)
            return
        if head_only:
            self.send_response(200)
            self.send_header("Content-Type", "application/octet-stream")
            self.send_header("Content-Length", str(size))
            self.send_header("Cache-Control", "no-store")
            self.end_headers()
            return
        self._write_bytes(body, "application/octet-stream")

    def _serve_local_cover(self, token: str, head_only: bool = False) -> None:
        if not _COVER_TOKEN_RE.fullmatch(token):
            self._write_json(404, {"status": "error", "message": "Not found"}, head_only=head_only)
            return
        cover_path = get_local_catalog().cover_paths.get(token)
        if cover_path is None or cover_path.is_symlink() or not cover_path.is_file():
            self._write_json(404, {"status": "error", "message": "Not found"}, head_only=head_only)
            return
        ext = cover_path.suffix.lower()
        try:
            if ext == ".raw":
                body = decode_raw_bgr565_to_bmp(cover_path.read_bytes())
                content_type = "image/bmp"
            else:
                content_type = IMAGE_MIME_TYPES.get(ext, "")
                if not content_type:
                    self._write_json(404, {"status": "error", "message": "Not found"}, head_only=head_only)
                    return
                body = cover_path.read_bytes()
        except (OSError, ValueError):
            self._write_json(404, {"status": "error", "message": "Not found"}, head_only=head_only)
            return
        self._write_bytes(body, content_type, cache_control="max-age=300", head_only=head_only)

    def _handle_read_request(self, head_only: bool) -> bool:
        if not self._host_is_allowed():
            return True
        parsed = urlsplit(self.path)
        route = parsed.path
        if route == "/hardware/layout.json":
            layout_path = DIRECTORY.parent / "hardware" / "layout.json"
            if not layout_path.is_file():
                self._write_json(404, {"status": "error", "message": "Not found"}, head_only=head_only)
                return True
            self._write_bytes(layout_path.read_bytes(), "application/json; charset=utf-8", head_only=head_only)
            return True
        if route.startswith("/local-library/"):
            if self._serve_local_library(route, parsed.query, head_only=head_only):
                return True
            self._write_json(404, {"status": "error", "message": "Not found"}, head_only=head_only)
            return True
        if route.startswith("/api/"):
            self._write_json(404, {"status": "error", "message": "Not found"}, head_only=head_only)
            return True
        return False

    def do_HEAD(self) -> None:
        if not self._handle_read_request(head_only=True):
            super().do_HEAD()

    def do_GET(self) -> None:
        if not self._handle_read_request(head_only=False):
            super().do_GET()

    def do_POST(self) -> None:
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
    except Exception as exc:
        print(
            f"[!] Could not auto-build WASM cores: {exc}\n"
            "    Install WASI SDK or set WASI_SDK_PATH, then run: "
            "python scripts/build_wasm_cores.py"
        )


def main(argv: list[str] | None = None) -> int:
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
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
