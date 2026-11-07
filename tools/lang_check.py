#!/usr/bin/env python3
"""What the mods and worlds show that assets/lang/it.txt or en.txt lack.

The client translates by the text itself (src/client/ui/lang.h): a notice, a
chat line, hud text, a panel's label or button, a page's title and an item's
name (its id made a name, as ui::item_display_name does) are looked up as they
are drawn. A mod writes English, which it.txt puts into Italian, or Italian,
which en.txt puts into English. This reads every .luau under mods/ and
games/*/mods/ (a linked mod once), finds the text of those calls -- a literal,
a `string {interpolated}`, pieces joined with .., string.format, either side of
an if-then-else -- as the {} patterns the files use, tells which language each
is in, and lists those the other file has no line for.

    python3 tools/lang_check.py            # the missing, by file, with where
    python3 tools/lang_check.py --count    # only how many
    python3 tools/lang_check.py --keys     # the missing keys alone, it.txt's then en.txt's

Exit status 1 when something is missing. A text built in a way this does not
follow (from a table, a function's result) is not seen: say it in a comment
by the call, or keep the table's strings as literals.
"""
import os
import re
import sys

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
LANG = os.path.join(ROOT, "assets", "lang")

# The calls whose text is drawn, and which argument it is (0-based).
CALLS = {
    "core.notice": 1, "core.chat": 1, "core.hud.text": 2, "core.ui_label": 3, "core.ui_button": 6,
    "core.page_register": 1,
}
NAMES = ("core.register_node", "core.register_item", "core.register_tool")

IT_WORDS = set("""il lo la gli le un una uno di del della dei degli delle che non per con sono nel nella
nei alla al allo ai dal dalla da su sul sulla tra fra si ti ci mi vi tuo tua tuoi tue suo sua suoi
ha hai ho hanno era piu' gia' ancora questo questa quello quella qui li' dove quando come anche
ogni tutto tutti niente nessuno nessuna dopo prima sempre mai poi se ma o ed e' c'e' porta prendi
chiave chiavi mondo piano trono luce torcia aspetta sigillo cripta chiudi crediti soldati
costruttori negozio cartucce proiettili giri tempo ricompensa cassa accanto circuito""".split())
EN_WORDS = set("""the a an of to in on at is are was be you your it its and or not no for with from by
this that these those there here what which who when where how all any some more less than then
has have had do does did can will would should one two first last back up down out over""".split())


def italian(text: str) -> bool:
    if re.search(r"[àèéìòù]", text):
        return True
    words = re.findall(r"[a-zA-Z']+", text.lower())
    it = sum(w in IT_WORDS for w in words)
    en = sum(w in EN_WORDS for w in words)
    return it > en


def split_args(s: str, i: int):
    """The arguments of a call whose '(' is at s[i-1], and where it ends."""
    args, depth, cur, j = [], 0, [], i
    while j < len(s):
        c = s[j]
        if c in "\"'`":
            k = j + 1
            while k < len(s) and s[k] != c:
                k += 2 if s[k] == "\\" else 1
            cur.append(s[j:k + 1])
            j = k + 1
            continue
        if c == "-" and s.startswith("--", j):
            k = s.find("\n", j)
            j = len(s) if k < 0 else k
            continue
        if c in "([{":
            depth += 1
        elif c in ")]}":
            if depth == 0:
                args.append("".join(cur).strip())
                return args, j
            depth -= 1
        elif c == "," and depth == 0:
            args.append("".join(cur).strip())
            cur = []
            j += 1
            continue
        cur.append(c)
        j += 1
    return args, j


def top_split(expr: str, sep: str):
    """expr split on sep where it is not inside a string or brackets."""
    out, depth, cur, j = [], 0, [], 0
    while j < len(expr):
        c = expr[j]
        if c in "\"'`":
            k = j + 1
            while k < len(expr) and expr[k] != c:
                k += 2 if expr[k] == "\\" else 1
            cur.append(expr[j:k + 1])
            j = k + 1
            continue
        if c in "([{":
            depth += 1
        elif c in ")]}":
            depth -= 1
        if depth == 0 and expr.startswith(sep, j) and (sep != " then " or True):
            out.append("".join(cur))
            cur = []
            j += len(sep)
            continue
        cur.append(c)
        j += 1
    out.append("".join(cur))
    return out


def unquote(lit: str) -> str:
    body = lit[1:-1]
    return body.replace("\\'", "'").replace('\\"', '"').replace("\\n", " ").replace("\\\\", "\\")


def patterns(expr: str):
    """The {} patterns an expression may draw; [] when it cannot be followed."""
    expr = expr.strip()
    while expr.startswith("(") and expr.endswith(")") and split_args(expr, 1)[1] == len(expr) - 1:
        expr = expr[1:-1].strip()
    if expr.startswith("if ") and " then " in expr:
        out = []
        for part in re.split(r"\b(?:then|elseif|else)\b", expr)[1:]:
            part = re.sub(r"^\s*|\s*$", "", part)
            if part and not re.match(r"^[^\"'`]*\bthen\b", part):
                out += patterns(part.split(" elseif ")[0])
        return out
    if expr.startswith("string.format("):
        args, _ = split_args(expr, len("string.format("))
        if args and args[0][:1] in "\"'":
            fmt = unquote(args[0])
            fmt = re.sub(r"%[-+ #0]*\d*(?:\.\d+)?[sdifgxXcq]", "{}", fmt).replace("%%", "%")
            return [fmt]
        return []
    parts = top_split(expr, "..")
    if len(parts) > 1:
        texts = [""]
        for p in parts:
            sub = patterns(p) if p.strip()[:1] in "\"'`" else None
            piece = sub[0] if sub else "{}"
            texts = [t + piece for t in texts]
        t = re.sub(r"(\{\})+", "{}", texts[0])
        return [t]
    if expr[:1] in "\"'" and expr[-1:] == expr[:1]:
        return [unquote(expr)]
    if expr[:1] == "`" and expr[-1:] == "`":
        return [holes(expr[1:-1])]
    return []


def holes(body: str) -> str:
    """A `string`'s body with each {expression} as {}, nested strings and all."""
    out, j = [], 0
    while j < len(body):
        if body[j] == "{":
            depth, k = 1, j + 1
            while k < len(body) and depth > 0:
                c = body[k]
                if c == "`":
                    k = body.find("`", k + 1)
                    if k < 0:
                        k = len(body)
                elif c in "\"'":
                    k = body.find(c, k + 1)
                    if k < 0:
                        k = len(body)
                elif c == "{":
                    depth += 1
                elif c == "}":
                    depth -= 1
                k += 1
            out.append("{}")
            j = k
        else:
            out.append(body[j])
            j += 1
    return "".join(out)


def display_name(item_id: str) -> str:
    out = item_id.split(":", 1)[-1].replace("_", " ")
    return out[:1].upper() + out[1:]


def keys(path: str):
    out = []
    if os.path.exists(path):
        for line in open(path, encoding="utf-8"):
            line = line.rstrip("\n")
            if not line or line.startswith("#") or line.startswith("@") or " = " not in line:
                continue
            out.append(line.split(" = ", 1)[0])
    return out


def matcher(key: str):
    return re.compile("^" + ".*".join(re.escape(p) for p in key.split("{}")) + "$", re.S)


def found(text: str, have, rx) -> bool:
    if text in have:
        return True
    # A hole that is a plural's "s" (thing{}) is two lines in a file, so try
    # it as a number, as nothing and as an s.
    samples = {text.replace("{}", v) for v in ("7", "", "s")}
    samples.add(re.sub(r"(?<=[a-z])\{\}", "s", text).replace("{}", "7"))
    samples.add(re.sub(r"(?<=[a-z])\{\}", "", text).replace("{}", "7"))
    return any(t in have for t in samples) or any(r.match(t) for r in rx for t in samples)


def main():
    seen_dirs, texts = set(), {}
    roots = [os.path.join(ROOT, "mods")] + [os.path.join(ROOT, "games", g, "mods")
                                             for g in sorted(os.listdir(os.path.join(ROOT, "games")))]
    for root in roots:
        if not os.path.isdir(root):
            continue
        for mod in sorted(os.listdir(root)):
            real = os.path.realpath(os.path.join(root, mod))
            if real in seen_dirs or not os.path.isdir(real):
                continue
            seen_dirs.add(real)
            for dirpath, _, files in os.walk(real):
                for f in files:
                    if not f.endswith(".luau"):
                        continue
                    path = os.path.join(dirpath, f)
                    src = open(path, encoding="utf-8").read()
                    rel = os.path.relpath(path, ROOT)
                    for m in re.finditer(r"\b(core\.[a-z_.]+)\(", src):
                        name = m.group(1)
                        if name not in CALLS and name not in NAMES:
                            continue
                        args, _ = split_args(src, m.end())
                        line = src.count("\n", 0, m.start()) + 1
                        if name in NAMES:
                            if args and re.fullmatch(r'"[^"\\]*"|\'[^\'\\]*\'', args[0]):
                                t = display_name(unquote(args[0]))
                                texts.setdefault(t, f"{rel}:{line}")
                            continue
                        k = CALLS[name]
                        if len(args) > k:
                            for t in patterns(args[k]):
                                t = t.strip()
                                if re.search(r"[A-Za-z]{2}", t) and t != "{}":
                                    texts.setdefault(t, f"{rel}:{line}")
    it_keys, en_keys = keys(os.path.join(LANG, "it.txt")), keys(os.path.join(LANG, "en.txt"))
    it_set, en_set = set(it_keys), set(en_keys)
    it_rx = [matcher(k) for k in it_keys if "{}" in k]
    en_rx = [matcher(k) for k in en_keys if "{}" in k]
    miss_it, miss_en = [], []
    for t, where in sorted(texts.items()):
        # In either file already: its language is the one that file is from.
        if found(t, it_set, it_rx) or found(t, en_set, en_rx):
            continue
        if italian(t):
            if not found(t, en_set, en_rx):
                miss_en.append((t, where))
        elif not found(t, it_set, it_rx):
            miss_it.append((t, where))
    if "--count" in sys.argv:
        print(f"{len(texts)} texts; missing: {len(miss_it)} in it.txt, {len(miss_en)} in en.txt")
    elif "--keys" in sys.argv:
        for t, _ in miss_it:
            print(t)
        print("----")
        for t, _ in miss_en:
            print(t)
    else:
        for title, miss in (("it.txt (English text)", miss_it), ("en.txt (Italian text)", miss_en)):
            print(f"# missing in {title}: {len(miss)}")
            for t, where in miss:
                print(f"{t}    <- {where}")
    return 1 if miss_it or miss_en else 0


if __name__ == "__main__":
    sys.exit(main())
