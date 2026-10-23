#!/usr/bin/env python3
"""Download the rbx-dom reference binary files used to validate our RBXL writer.

These are only test fixtures (not shipped inside the game)."""
import json, subprocess, os, sys

OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "testdata")
TREE = "/tmp/tree.json"


def main():
    os.makedirs(OUT, exist_ok=True)
    if not os.path.exists(TREE):
        subprocess.run(
            ["curl", "-sS",
             "https://api.github.com/repos/rojo-rbx/rbx-test-files/git/trees/master?recursive=1",
             "-o", TREE], check=True)
    d = json.load(open(TREE))
    files = [t["path"] for t in d["tree"] if t["path"].endswith((".rbxm", ".rbxl"))]
    files += ["places/baseplate-454/xml.rbxlx", "places/baseplate-413/xml.rbxlx"]
    print("downloading", len(files), "files")
    for p in files:
        out = os.path.join(OUT, p.replace("/", "__"))
        url = "https://api.github.com/repos/rojo-rbx/rbx-test-files/contents/" + p
        subprocess.run(["curl", "-sS", "-H", "Accept: application/vnd.github.raw", url, "-o", out])
        if os.path.getsize(out) < 100:
            print("FAIL", p, open(out, "rb").read()[:80])
    print(len(os.listdir(OUT)), "files", sum(os.path.getsize(os.path.join(OUT, f)) for f in os.listdir(OUT)), "bytes")


if __name__ == "__main__":
    main()
