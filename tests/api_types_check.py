#!/usr/bin/env python3
"""The hand-written part of core and its types say the same thing.

Most of core is generated: one line in src/script/api.luau makes the C++ glue,
the type in docs/api.d.luau and the documentation, and they cannot disagree.
core.ecs, craft, net, render, hud, event, register_mapgen/abm/lbm are written
by hand -- their C++ in src/script/script_luau.cpp (register_api), their types
in the .d.luau part of tools/api_gen/gen.luau -- and nothing tied the two: from
0.9.0 to 0.18 the types lacked whole tables, and every mod that used them was
an error in an editor and nowhere else.

This reads every `lua_pushcfunction(L, fn, "core.a.b")` in register_api and
every function declared after the "Hand-written" line of docs/api.d.luau, and
fails if a name is on one side only. It checks names, not argument lists:
those are the part a reader of the two files can see side by side.

    python3 tests/api_types_check.py [source dir]
"""
import os
import re
import sys

ROOT = sys.argv[1] if len(sys.argv) > 1 else os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")

cpp = open(os.path.join(ROOT, "src", "script", "script_luau.cpp"), encoding="utf-8").read()
start = cpp.index("void register_api(lua_State *L)")
body = cpp[start:cpp.index("\n}\n", start)]
registered = set(re.findall(r'lua_pushcfunction\(L, \w+, "core\.([\w.]+)"\)', body))

dluau = open(os.path.join(ROOT, "docs", "api.d.luau"), encoding="utf-8").read()
hand = dluau[dluau.index("Hand-written, not generated"):]
declared = set()
stack = []
for line in hand.split("\n"):
    indent = len(line) - len(line.lstrip(" "))
    want = 4 * (len(stack) + 1)   # a field of the table being read, not a continuation line
    m = re.match(r"(\w+): \{\s*$", line.strip())
    if m and indent == want:
        stack.append(m.group(1))
        continue
    if re.match(r"\},?\s*$", line.strip()) and stack and indent == 4 * len(stack):
        stack.pop()
        continue
    m = re.match(r"(\w+): \(", line.strip())
    if m and indent == want:
        declared.add(".".join(stack + [m.group(1)]))

untyped = sorted(registered - declared)
unregistered = sorted(declared - registered)
if untyped:
    print("in register_api but without a type in docs/api.d.luau (add it to tools/api_gen/gen.luau):")
    for n in untyped:
        print(f"  core.{n}")
if unregistered:
    print("typed in docs/api.d.luau but not registered by register_api:")
    for n in unregistered:
        print(f"  core.{n}")
if untyped or unregistered:
    sys.exit(1)
print(f"{len(registered)} hand-written functions, each with its type")
