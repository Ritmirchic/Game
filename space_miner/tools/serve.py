"""Run the exported Godot game; unpack the delivered Web ZIP automatically."""
from http.server import ThreadingHTTPServer, SimpleHTTPRequestHandler
from pathlib import Path
from functools import partial
from zipfile import ZipFile
ROOT=Path(__file__).resolve().parents[2]
ARCHIVE=ROOT/'delivery/SPACE_MINER_WEB.zip'
WEB=ROOT/'.cache/space-miner-web'
if ARCHIVE.exists():
    WEB.mkdir(parents=True,exist_ok=True)
    with ZipFile(ARCHIVE) as z:
        z.extractall(WEB)
else:
    WEB=ROOT/'releases/web'
if not (WEB/'index.html').exists():
    raise SystemExit('Export the Web preset or place SPACE_MINER_WEB.zip in delivery/ first.')
class Handler(SimpleHTTPRequestHandler):
    def end_headers(self):
        self.send_header('Cross-Origin-Opener-Policy','same-origin')
        self.send_header('Cross-Origin-Embedder-Policy','require-corp')
        self.send_header('Cache-Control','no-store')
        super().end_headers()
    def do_GET(self):
        if self.path=='/':
            self.send_response(302)
            self.send_header('Location','/play/index.html')
            self.end_headers()
            return
        if self.path.startswith('/play/'):
            self.path=self.path[5:]
        super().do_GET()
print('SPACE MINER ready on port 8000',flush=True)
ThreadingHTTPServer(('0.0.0.0',8000),partial(Handler,directory=str(WEB))).serve_forever()
