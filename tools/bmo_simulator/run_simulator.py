#!/usr/bin/env python3
"""
run_simulator.py
Launches the BMO Handheld Console 1:1 Live Simulator in a local web server
and opens the user's default browser for real-time UI/UX design inspection.
"""

import http.server
import json
import os
import socketserver
import subprocess
import sys
import webbrowser
from pathlib import Path

PORT = 8080
DIRECTORY = Path(__file__).resolve().parent
REPO_ROOT = DIRECTORY.parents[1]


class Handler(http.server.SimpleHTTPRequestHandler):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, directory=str(DIRECTORY), **kwargs)

    def do_GET(self):
        if self.path == "/api/host-cores/run":
            self.handle_host_cores_run()
            return
        super().do_GET()

    def handle_host_cores_run(self):
        try:
            cmd = [
                sys.executable,
                "-m",
                "unittest",
                "tests.test_native_cores_execution.TestNativeCoresExecutionAndPhaseD.test_01_compile_and_execute_all_10_c_emulator_cores_50_frames",
            ]
            proc = subprocess.run(
                cmd,
                cwd=str(REPO_ROOT),
                capture_output=True,
                text=True,
                timeout=60,
            )
            passed = proc.returncode == 0
            payload = {
                "status": "success" if passed else "failed",
                "cores_tested": 10,
                "all_passed": passed,
                "message": (
                    "All 10 native C cores passed 50-frame execution and visual regression"
                    if passed
                    else "Host runner reported failure"
                ),
                "output": proc.stdout or proc.stderr,
            }
            body = json.dumps(payload).encode("utf-8")
            self.send_response(200 if passed else 500)
            self.send_header("Content-Type", "application/json")
            self.send_header("Content-Length", str(len(body)))
            self.end_headers()
            self.wfile.write(body)
        except Exception as e:
            err = json.dumps({"status": "error", "message": str(e)}).encode("utf-8")
            self.send_response(500)
            self.send_header("Content-Type", "application/json")
            self.send_header("Content-Length", str(len(err)))
            self.end_headers()
            self.wfile.write(err)

def main():
    os.chdir(DIRECTORY)
    print("==================================================================")
    print(" BMO HANDHELD CONSOLE 1:1 LIVE SIMULATOR & UI/UX DESIGN LAB")
    print("==================================================================")
    print(f"[*] Serving simulator at http://localhost:{PORT}")
    print("    Press Ctrl+C to stop the local simulator server.")
    print("==================================================================")
    
    webbrowser.open(f"http://localhost:{PORT}")
    
    with socketserver.TCPServer(("", PORT), Handler) as httpd:
        try:
            httpd.serve_forever()
        except KeyboardInterrupt:
            print("\nSimulator stopped.")

if __name__ == "__main__":
    main()
