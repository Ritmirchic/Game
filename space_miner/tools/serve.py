"""Serve the actual Godot web export with required WASM isolation headers."""
from http.server import ThreadingHTTPServer, SimpleHTTPRequestHandler
from pathlib import Path
import os
os.chdir(Path(__file__).resolve().parents[2])
class Handler(SimpleHTTPRequestHandler):
    def end_headers(self):
        self.send_header('Cross-Origin-Opener-Policy','same-origin')
        self.send_header('Cross-Origin-Embedder-Policy','require-corp')
        self.send_header('Cache-Control','no-store')
        super().end_headers()
    def do_GET(self):
        if self.path=='/':
            self.send_response(302)
            self.send_header('Location','/releases/web/index.html')
            self.end_headers()
            return
        super().do_GET()
ThreadingHTTPServer(('0.0.0.0',8000),Handler).serve_forever()
