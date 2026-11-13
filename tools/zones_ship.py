#!/usr/bin/env python3
"""Zones out of a world file into the file the game ships them in.

mods/zones keeps the zones players save in the server's shared storage, which
is the first room's world file. This reads them out of such a file and writes
mods/zones/shipped.luau, which the mod loads on every server: the zones in it
are approved and nobody's.

    python3 tools/zones_ship.py WORLD_FILE              # every approved zone
    python3 tools/zones_ship.py WORLD_FILE --all        # the private ones too
    python3 tools/zones_ship.py WORLD_FILE --world chambers
    python3 tools/zones_ship.py WORLD_FILE chambers.hall arpg.lair
    python3 tools/zones_ship.py WORLD_FILE --list       # what is there, nothing written
    python3 tools/zones_ship.py WORLD_FILE --keep       # add to the file's zones, not replace them
    python3 tools/zones_ship.py WORLD_FILE -o FILE      # another file than mods/zones/shipped.luau

Run it on a world no server has open, or on a copy.
"""
import argparse
import os
import re
import sqlite3
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "..", "mods", "zones", "shipped.luau")
HEADER = ("--!strict\n"
          "-- The zones that come with the game. Written by tools/zones_ship.py: not by hand.\n")


def get_long(store, key):
    try:
        parts = int(store.get(key, "") or 0)
    except ValueError:
        return ""
    return "".join(store.get(f"{key} {k}", "") for k in range(1, parts + 1))


def quote(text):
    """A Luau long string no text of a zone can close."""
    level = 1
    while "]" + "=" * level + "]" in text:
        level += 1
    eq = "=" * level
    # A long string drops a newline right after its opening bracket.
    return f"[{eq}[\n{text}]{eq}]"


def read_shipped(path):
    """id -> (head, body) of a file this tool wrote."""
    if not os.path.exists(path):
        return {}
    text = open(path, encoding="utf-8").read()
    out = {}
    for m in re.finditer(r'\["([^"]+)"\] = \{ \[(=+)\[\n(.*?)\]\2\], \[(=+)\[\n(.*?)\]\4\] \},\n', text, re.S):
        out[m.group(1)] = (m.group(3), m.group(5))
    return out


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("world_file")
    ap.add_argument("ids", nargs="*")
    ap.add_argument("--all", action="store_true")
    ap.add_argument("--world")
    ap.add_argument("--list", action="store_true")
    ap.add_argument("--keep", action="store_true")
    ap.add_argument("-o", "--out", default=OUT)
    a = ap.parse_args()

    if not os.path.exists(a.world_file):
        sys.exit(f"zones_ship: no file {a.world_file}")
    db = sqlite3.connect(f"file:{a.world_file}?mode=ro", uri=True)
    try:
        store = dict(db.execute("SELECT key, value FROM mod_store WHERE mod = 'zones'"))
    except sqlite3.Error as e:
        sys.exit(f"zones_ship: {a.world_file}: {e}")

    index = get_long(store, "index")
    found = {}
    for zid in (index.split(";") if index else []):
        head, body = get_long(store, f"z {zid}"), get_long(store, f"c {zid}")
        f = head.split("|")
        if len(f) != 8 or f[0] != "1" or "\n" not in body:
            print(f"zones_ship: {zid}: not a zone this tool knows, left out", file=sys.stderr)
            continue
        found[zid] = (f, body)

    if a.list:
        for zid, (f, _) in sorted(found.items()):
            print(f"{zid}: {f[4].replace(' ', ' x ')}, {f[5].split()[-1]}, {f[2]}, world {f[3]}")
        if not found:
            print("no zones in this world file")
        return

    for zid in a.ids:
        if zid not in found:
            sys.exit(f"zones_ship: no zone called {zid} in {a.world_file} (--list shows them)")

    out = read_shipped(a.out) if a.keep else {}
    took = []
    for zid, (f, body) in sorted(found.items()):
        if a.ids and zid not in a.ids:
            continue
        if a.world and f[3] != a.world:
            continue
        if not a.ids and not a.all and f[2] != "approved":
            continue
        # Shipped, a zone is nobody's and approved, whatever it was.
        f[1], f[2] = "-", "approved"
        out[zid] = ("|".join(f), body)
        took.append(zid)

    with open(a.out, "w", encoding="utf-8") as fh:
        fh.write(HEADER)
        fh.write("return {\n")
        for zid in sorted(out):
            head, body = out[zid]
            fh.write(f'    ["{zid}"] = {{ {quote(head)}, {quote(body)} }},\n')
        fh.write("} :: { [string]: { string } }\n")
    print(f"zones_ship: {len(took)} zone(s) taken ({', '.join(took) or 'none'}), "
          f"{len(out)} in {os.path.relpath(a.out)}")


if __name__ == "__main__":
    main()
