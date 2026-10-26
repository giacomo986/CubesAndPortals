# The track's power-up icons and the ink blot, drawn with PIL into
# games/track/textures (track_core registers them as items, so the server
# serves them and track_game's HUD draws them with core.hud.image), and the
# icons of the timing tools, the item box tool and the track editor.
#
#   python3 tools/make_track_icons.py
import math
import os
import random

from PIL import Image, ImageDraw, ImageFilter

OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "games", "track", "textures")
N = 64
OUTLINE = (20, 16, 24, 255)


def canvas():
    im = Image.new("RGBA", (N, N), (0, 0, 0, 0))
    return im, ImageDraw.Draw(im)


def save(im, name):
    # Drawn at 64 and kept at 32, the size of the other item pictures: the
    # atlas takes one size, and a picture of another is a red square in the bag.
    im.resize((32, 32), Image.LANCZOS).save(os.path.join(OUT, f"track_core_{name}.png"))


def boost():
    im, d = canvas()
    for k, col in enumerate([(255, 210, 60, 255), (255, 120, 40, 255)]):
        y = 14 + k * 18
        pts = [(10, y + 20), (32, y), (54, y + 20), (54, y + 30), (32, y + 12), (10, y + 30)]
        d.polygon(pts, fill=col, outline=OUTLINE, width=2)
    save(im, "item_boost")


def banana():
    im, d = canvas()
    outer = [(32 + 26 * math.cos(a), 18 + 30 * math.sin(a)) for a in [math.radians(t) for t in range(20, 161, 10)]]
    inner = [(32 + 18 * math.cos(a), 14 + 22 * math.sin(a)) for a in [math.radians(t) for t in range(160, 19, -10)]]
    d.polygon(outer + inner, fill=(250, 215, 50, 255), outline=OUTLINE, width=2)
    d.ellipse((4, 22, 13, 31), fill=(90, 60, 25, 255))
    d.ellipse((51, 22, 60, 31), fill=(90, 60, 25, 255))
    save(im, "item_banana")


def shell(name, rgb):
    im, d = canvas()
    d.ellipse((6, 40, 58, 56), fill=(245, 240, 220, 255), outline=OUTLINE, width=2)
    d.pieslice((8, 10, 56, 74), 180, 360, fill=rgb + (255,), outline=OUTLINE, width=2)
    for x, y in [(32, 20), (20, 30), (44, 30), (32, 34)]:
        d.ellipse((x - 5, y - 4, x + 5, y + 4), fill=(245, 240, 220, 255))
    save(im, name)


def star():
    im, d = canvas()
    pts = []
    for i in range(10):
        r = 28 if i % 2 == 0 else 12
        a = -math.pi / 2 + i * math.pi / 5
        pts.append((32 + r * math.cos(a), 34 + r * math.sin(a)))
    d.polygon(pts, fill=(255, 220, 60, 255), outline=OUTLINE, width=2)
    d.rectangle((26, 28, 29, 36), fill=OUTLINE)
    d.rectangle((35, 28, 38, 36), fill=OUTLINE)
    save(im, "item_star")


def bolt():
    im, d = canvas()
    pts = [(36, 4), (14, 36), (30, 36), (24, 60), (50, 24), (34, 24), (42, 4)]
    d.polygon(pts, fill=(255, 235, 80, 255), outline=OUTLINE, width=2)
    save(im, "item_bolt")


def box_tool():
    # The item box tool: the box's coloured faces and its question mark.
    im, d = canvas()
    for (x0, y0), col in zip([(6, 6), (32, 6), (6, 32), (32, 32)],
                             [(255, 70, 60), (255, 215, 40), (70, 220, 90), (70, 160, 255)]):
        d.rectangle((x0, y0, x0 + 26, y0 + 26), fill=col + (255,))
    d.rectangle((6, 6, 58, 58), outline=(245, 245, 255, 255), width=3)
    q = [".###.", "#...#", "....#", "...#.", "..#..", ".....", "..#.."]
    for r, line in enumerate(q):
        for c, ch in enumerate(line):
            if ch == "#":
                d.rectangle((17 + c * 6, 11 + r * 6, 22 + c * 6, 16 + r * 6), fill=(255, 255, 255, 255),
                            outline=OUTLINE)
    save(im, "box_tool")


def flag(name, cells):
    # A flag on a pole, its cloth `cells`: rows of colours across it.
    im, d = canvas()
    d.rectangle((10, 4, 15, 60), fill=(200, 200, 210, 255), outline=OUTLINE)
    rows, cols = len(cells), len(cells[0])
    w, h = 42 // cols, 30 // rows
    for r, line in enumerate(cells):
        for c, col in enumerate(line):
            d.rectangle((16 + c * w, 6 + r * h, 16 + (c + 1) * w, 6 + (r + 1) * h), fill=col + (255,))
    d.rectangle((16, 6, 16 + cols * w, 6 + rows * h), outline=OUTLINE, width=2)
    save(im, name)


def offroad():
    # The off-road area: grass between two opposite corners.
    im, d = canvas()
    d.rectangle((10, 10, 54, 54), fill=(90, 170, 70, 255))
    for x in range(14, 54, 8):
        for y in range(16, 54, 10):
            d.line((x, y, x + 2, y - 5), fill=(50, 120, 40, 255), width=2)
    for x0, y0, sx, sy in [(6, 6, 1, 1), (58, 58, -1, -1)]:
        d.line((x0, y0, x0 + 16 * sx, y0), fill=(255, 255, 255, 255), width=5)
        d.line((x0, y0, x0, y0 + 16 * sy), fill=(255, 255, 255, 255), width=5)
    save(im, "offroad")


def editor():
    # The track editor: a bend of road and a pencil drawing it on.
    im, d = canvas()
    d.pieslice((-40, 14, 48, 102), 270, 360, fill=(70, 72, 84, 255), outline=OUTLINE, width=2)
    d.pieslice((-14, 40, 22, 76), 270, 360, fill=(0, 0, 0, 0))
    for a in range(272, 360, 12):
        x = 4 + 38 * math.cos(math.radians(a))
        y = 58 + 38 * math.sin(math.radians(a))
        d.rectangle((x - 2, y - 2, x + 2, y + 2), fill=(230, 60, 50, 255))
    d.polygon([(40, 4), (60, 24), (34, 50), (14, 30)], fill=(250, 200, 60, 255), outline=OUTLINE)
    d.polygon([(14, 30), (34, 50), (8, 56)], fill=(240, 220, 180, 255), outline=OUTLINE)
    save(im, "editor")


def blot(size, seed):
    random.seed(seed)
    s = size / 128
    im = Image.new("L", (size, size), 0)
    d = ImageDraw.Draw(im)

    def disc(x, y, r):
        d.ellipse(((x - r) * s, (y - r) * s, (x + r) * s, (y + r) * s), fill=255)
    disc(64, 64, 34)
    for _ in range(9):
        a, r = random.uniform(0, 2 * math.pi), random.uniform(30, 44)
        disc(64 + math.cos(a) * r, 64 + math.sin(a) * r, random.uniform(7, 14))
    for _ in range(7):
        a, r = random.uniform(0, 2 * math.pi), random.uniform(50, 58)
        disc(64 + math.cos(a) * r, 64 + math.sin(a) * r, random.uniform(3, 6))
    out = Image.new("RGBA", (size, size), (18, 12, 30, 0))
    out.putalpha(im.filter(ImageFilter.GaussianBlur(1.2 * s)))
    return out


if __name__ == "__main__":
    boost()
    banana()
    shell("item_shell", (60, 190, 80))
    shell("item_homing", (225, 50, 40))
    star()
    bolt()
    k, w = (20, 20, 24), (245, 245, 245)
    flag("start_line", [[k, w, k, w], [w, k, w, k], [k, w, k, w]])
    flag("checkpoint", [[(250, 210, 40)], [(60, 130, 240)]])
    offroad()
    box_tool()
    editor()
    save(blot(N, 3), "item_ink")
    save(blot(N, 7), "ink_blot")
    print("track icons written to", os.path.normpath(OUT))
