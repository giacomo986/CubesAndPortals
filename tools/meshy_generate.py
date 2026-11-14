#!/usr/bin/env python3
"""A model out of Meshy from a line of text, by its API: a preview (the shape),
a refine (its texture), and the .glb put where the scripts that fit it to the
game read it (tools/meshy_prop.py for a rigid thing).

    tools/meshy_generate.py balance
    tools/meshy_generate.py make OUT.glb "a line that says what it is" [--polys 6000] [--model meshy-6]

The key is MESHY_API_KEY, or the one the Meshy MCP server is given in
~/.claude.json. It is never printed. Every task costs credits of that account:
`balance` before and after says how many.
"""
import json
import os
import sys
import time
import urllib.error
import urllib.request

API = "https://api.meshy.ai/openapi"


def key():
    k = os.environ.get("MESHY_API_KEY", "")
    if not k:
        try:
            with open(os.path.expanduser("~/.claude.json")) as f:
                k = json.load(f)["mcpServers"]["meshy"]["env"]["MESHY_API_KEY"]
        except (OSError, KeyError, ValueError):
            pass
    if not k:
        sys.exit("meshy_generate: no MESHY_API_KEY, and none in ~/.claude.json")
    return k


def call(path, body=None):
    req = urllib.request.Request(API + path, data=json.dumps(body).encode() if body is not None else None,
                                 headers={"Authorization": "Bearer " + key(), "Content-Type": "application/json",
                                          "User-Agent": "cubes-and-portals/tools"})
    try:
        with urllib.request.urlopen(req, timeout=60) as r:
            return json.load(r)
    except urllib.error.HTTPError as e:
        sys.exit(f"meshy_generate: {path}: HTTP {e.code}: {e.read().decode(errors='replace')[:400]}")


def wait(task):
    last = -1
    while True:
        t = call(f"/v2/text-to-3d/{task}")
        if t.get("progress", 0) != last:
            last = t.get("progress", 0)
            print(f"  {t.get('status')} {last}%", flush=True)
        if t.get("status") == "SUCCEEDED":
            return t
        if t.get("status") in ("FAILED", "CANCELED", "EXPIRED"):
            sys.exit(f"meshy_generate: task {task} {t.get('status')}: {t.get('task_error')}")
        time.sleep(6)


def fetch(url, path):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    req = urllib.request.Request(url, headers={"User-Agent": "cubes-and-portals/tools"})
    with urllib.request.urlopen(req, timeout=300) as r, open(path, "wb") as f:
        f.write(r.read())


def make(out, prompt, polys, model):
    print(f"preview: {prompt}", flush=True)
    preview = call("/v2/text-to-3d", {"mode": "preview", "prompt": prompt, "ai_model": model,
                                      "should_remesh": True, "topology": "triangle", "target_polycount": polys})["result"]
    wait(preview)
    print("refine", flush=True)
    refine = call("/v2/text-to-3d", {"mode": "refine", "preview_task_id": preview})["result"]
    t = wait(refine)
    fetch(t["model_urls"]["glb"], out)
    if t.get("thumbnail_url"):
        fetch(t["thumbnail_url"], os.path.splitext(out)[0] + "_thumb.png")
    with open(os.path.splitext(out)[0] + ".txt", "w") as f:
        f.write(f"prompt: {prompt}\nmodel: {model}\npolys: {polys}\npreview: {preview}\nrefine: {refine}\n")
    print("wrote", out, os.path.getsize(out), "bytes", flush=True)


if __name__ == "__main__":
    a = sys.argv[1:]
    if a[:1] == ["balance"]:
        print("credits:", call("/v1/balance").get("balance"))
    elif a[:1] == ["make"] and len(a) >= 3:
        polys = int(a[a.index("--polys") + 1]) if "--polys" in a else 6000
        model = a[a.index("--model") + 1] if "--model" in a else "meshy-6"
        make(a[1], a[2], polys, model)
    else:
        sys.exit(__doc__)
