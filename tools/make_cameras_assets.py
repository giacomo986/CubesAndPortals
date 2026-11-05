#!/usr/bin/env python3
"""The faces of mods/cameras' blocks, on the 16 grid doubled to 32 px into
assets/textures: the camera's dark grey casing, its lens on the front and a
light by it -- dark, and red on the one that sees someone -- and the screen's
frame, its glass dark (the picture is drawn over it, core.render.screen).
Seeded.

    python3 tools/make_cameras_assets.py
"""
import os
import random

from PIL import Image

OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "assets", "textures")
CASE = (70, 72, 78)


def shade(c, k):
    return tuple(max(0, min(255, int(v * k))) for v in c)


def save(img, name):
    img.resize((32, 32), Image.NEAREST).save(os.path.join(OUT, name + ".png"))


def casing(rng):
    img = Image.new("RGB", (16, 16))
    for y in range(16):
        for x in range(16):
            edge = x in (0, 15) or y in (0, 15)
            img.putpixel((x, y), shade(CASE, (0.75 if edge else 1.0) * rng.uniform(0.92, 1.06)))
    return img


def front(rng, on):
    img = casing(rng)
    for y in range(16):
        for x in range(16):
            d = ((x - 7.5) ** 2 + (y - 7.5) ** 2) ** 0.5
            if d < 5.5:
                img.putpixel((x, y), (20, 22, 28) if d > 4.5 else (35, 45, 70) if d > 2 else (90, 120, 170))
    img.putpixel((5, 5), (240, 230, 255))
    for x, y in ((13, 2), (13, 3), (12, 2), (12, 3)):
        img.putpixel((x, y), (255, 40, 30) if on else (60, 20, 20))
    return img


def screen(rng, part):
    img = Image.new("RGB", (16, 16))
    for y in range(16):
        for x in range(16):
            frame = x in (0, 1, 14, 15) or y in (0, 1, 14, 15)
            if part == "front" and not frame:
                c = (12, 14, 18)
            else:
                c = shade((45, 46, 50), rng.uniform(0.9, 1.08))
            img.putpixel((x, y), c)
    return img


def main():
    rng = random.Random(5150)
    # The faces by their groups (side, top, bottom) and not the plain name:
    # that one is the bag's picture, rendered from the model by
    # tools/make_held_models.py. A face asks for its own file first
    # (src/core/media/face_tiles.h). The camera is drawn as its models
    # (tools/make_camera_models.py); these are what a client without them draws.
    for name, on in (("cameras_camera", False), ("cameras_camera_on", True)):
        side = casing(rng)
        for group in ("side", "top", "bottom"):
            save(side, f"{name}_{group}")
        save(front(rng, on), name + "_front")
    side = screen(rng, "side")
    for group in ("side", "top", "bottom"):
        save(side, f"cameras_screen_{group}")
    save(screen(rng, "front"), "cameras_screen_front")


if __name__ == "__main__":
    main()
