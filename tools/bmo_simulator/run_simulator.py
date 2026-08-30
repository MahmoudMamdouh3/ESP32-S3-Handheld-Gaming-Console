#!/usr/bin/env python3
"""
run_simulator.py
Launches the BMO Handheld Console 1:1 Live Simulator in a local web server
and opens the user's default browser for real-time UI/UX design inspection.
"""

import http.server
import socketserver
import webbrowser
import os
from pathlib import Path

PORT = 8080
DIRECTORY = Path(__file__).resolve().parent

class Handler(http.server.SimpleHTTPRequestHandler):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, directory=str(DIRECTORY), **kwargs)

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
