#!/usr/bin/env python3
"""The faces of mods/bees' blocks, drawn by rule on the 16 grid and doubled to
32 px into assets/textures, and its two sounds, synthesised into mods/bees.

  nest    a wild nest: rings of papery comb, gold and brown, a dark hole in
          the side for the bees; full, honey seeps from the hole and its rings.
  hive    a box of planks with a band of comb round its middle, an entrance
          slot at the bottom of the sides and a lid on top; full, honey runs
          down from the slot and the band.
  apiary  a kept hive (apiary.luau): pale painted boards, a white roof
          band, three frames' ends showing in the side, a landing board;
          full, honey shining between the frames.
  press   a screw press: iron plate and screw over a wooden tub, honey in
          the tub's top.
  honey_lamp  amber honey in a wax frame, glowing.
  buzz    a swarm, 2 s: a dozen detuned saw buzzes, each wandering in pitch
          and swelling in and out, as bees passing.
  sting   one bee close, 0.4 s: a high whine rising and cut short.
  press   the press screwed down, 0.6 s: a creak falling, a squelch.
  puff    the smoker's bellows, 0.7 s: two soft breaths of noise, low.

The honey and honeycomb items are tools/make_item_textures.py's. Seeded, so
running it again makes the same files.

    python3 tools/make_bees_assets.py
"""
import math
import os
import random
import sys

from PIL import Image

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from make_weather_sounds import ROOT, RATE, write  # noqa: E402

OUT = os.path.join(ROOT, "assets", "textures")
MOD = os.path.join(ROOT, "mods", "bees")

PLANK = (176, 136, 84)
COMB = (215, 160, 50)
COMB_DARK = (150, 100, 35)
HONEY = (235, 150, 20)
HONEY_LIGHT = (255, 205, 90)
HOLE = (40, 26, 16)


def shade(c, k):
    return tuple(max(0, min(255, int(v * k))) for v in c)


def save(img, name):
    img.resize((32, 32), Image.NEAREST).save(os.path.join(OUT, name + ".png"))


def nest_side(rng, full):
    img = Image.new("RGB", (16, 16))
    for y in range(16):
        for x in range(16):
            ring = (y + (x * 7 // 16) % 2) % 3 == 0
            c = COMB_DARK if ring else COMB
            img.putpixel((x, y), shade(c, rng.uniform(0.88, 1.08)))
    # The bees' way in, low on the side.
    for y in range(9, 12):
        for x in range(6, 10):
            img.putpixel((x, y), HOLE)
    if full:
        # Honey out of the hole and along two rings, running down.
        for x in range(6, 10):
            for y in range(12, 12 + 1 + (x * 5) % 4):
                img.putpixel((x, y), shade(HONEY, rng.uniform(0.9, 1.1)))
        for x in (2, 3, 12, 13):
            for y in range(3, 3 + 2 + x % 3):
                img.putpixel((x, y), HONEY_LIGHT if y == 3 else HONEY)
    return img


def nest_top(rng, full):
    img = Image.new("RGB", (16, 16))
    for y in range(16):
        for x in range(16):
            d = max(abs(x - 7.5), abs(y - 7.5))
            c = COMB_DARK if int(d) % 3 == 0 else COMB
            if full and d < 2.5:
                c = HONEY
            img.putpixel((x, y), shade(c, rng.uniform(0.88, 1.08)))
    return img


def hive_side(rng, full):
    img = Image.new("RGB", (16, 16))
    for y in range(16):
        for x in range(16):
            if 6 <= y <= 9:
                c = COMB if (x + y) % 3 else COMB_DARK      # the band of comb
            else:
                c = shade(PLANK, 0.8) if y % 4 == 0 else PLANK
            img.putpixel((x, y), shade(c, rng.uniform(0.9, 1.06)))
    for x in range(5, 11):
        img.putpixel((x, 13), HOLE)                         # the entrance
    if full:
        for x in range(5, 11):
            for y in range(14, 14 + 1 + (x * 3) % 2):
                if y < 16:
                    img.putpixel((x, y), HONEY)
        for x in (1, 4, 9, 13):
            for y in range(10, 10 + 1 + x % 3):
                img.putpixel((x, y), HONEY_LIGHT if y == 10 else HONEY)
    return img


def hive_top(rng):
    img = Image.new("RGB", (16, 16))
    for y in range(16):
        for x in range(16):
            edge = x in (0, 15) or y in (0, 15)
            c = shade(PLANK, 0.7) if edge else (shade(PLANK, 0.85) if x % 5 == 0 else PLANK)
            img.putpixel((x, y), shade(c, rng.uniform(0.9, 1.06)))
    return img


PAINT = (226, 214, 180)
IRON = (120, 124, 132)


def apiary_side(rng, full):
    img = Image.new("RGB", (16, 16))
    for y in range(16):
        for x in range(16):
            c = shade(PAINT, 0.82) if y % 5 == 0 else PAINT
            if y <= 1:
                c = (245, 245, 240)                          # the roof band
            img.putpixel((x, y), shade(c, rng.uniform(0.93, 1.05)))
    for fx in (3, 7, 11):                                    # the frames' ends
        for y in range(4, 11):
            img.putpixel((fx, y), shade(PLANK, 0.75))
            img.putpixel((fx + 1, y), PLANK)
        if full:
            for y in range(5, 10):
                img.putpixel((fx + 2, y), HONEY if y % 2 else HONEY_LIGHT)
    for x in range(4, 12):
        img.putpixel((x, 13), HOLE)                          # the entrance
        img.putpixel((x, 14), shade(PLANK, 0.9))             # the landing board
    return img


def apiary_top(rng):
    img = Image.new("RGB", (16, 16))
    for y in range(16):
        for x in range(16):
            edge = x in (0, 15) or y in (0, 15)
            c = (200, 200, 196) if edge else (240, 240, 236)
            img.putpixel((x, y), shade(c, rng.uniform(0.94, 1.04)))
    return img


def press_side(rng):
    img = Image.new("RGB", (16, 16))
    for y in range(16):
        for x in range(16):
            if y <= 4:
                c = IRON if y in (3, 4) else (shade(IRON, 0.8) if x in (7, 8) else shade(PLANK, 0.55))
            else:
                c = shade(PLANK, 0.75) if x % 4 == 0 else PLANK  # the tub's staves
                if y in (7, 13):
                    c = shade(IRON, 0.9)                     # its hoops
            img.putpixel((x, y), shade(c, rng.uniform(0.92, 1.06)))
    return img


def press_top(rng):
    img = Image.new("RGB", (16, 16))
    for y in range(16):
        for x in range(16):
            d = max(abs(x - 7.5), abs(y - 7.5))
            c = shade(PLANK, 0.8) if d > 6 else (IRON if d < 2 else HONEY)
            img.putpixel((x, y), shade(c, rng.uniform(0.92, 1.06)))
    return img


def honey_lamp(rng):
    img = Image.new("RGB", (16, 16))
    for y in range(16):
        for x in range(16):
            edge = x in (0, 15) or y in (0, 15)
            hexa = (x + (y // 3) % 2 * 2) % 4 == 0 or y % 3 == 0
            c = COMB_DARK if edge else (HONEY if hexa else HONEY_LIGHT)
            img.putpixel((x, y), shade(c, rng.uniform(0.95, 1.05)))
    return img


def squelch(rng, length):
    n = int(length * RATE)
    out = []
    ph = 0.0
    for i in range(n):
        t = i / RATE
        f = 140 - 70 * t / length
        ph = (ph + f / RATE) % 1.0
        creak = (1 if ph < 0.15 else -0.2) * (1 - t / length) * 0.6
        wet = rng.uniform(-1, 1) * max(0.0, math.sin(math.pi * t / length)) ** 3 * 0.5
        out.append((creak + wet) * min(1.0, t / 0.01))
    return out


def puff(rng, length):
    n = int(length * RATE)
    out = []
    lp = 0.0
    for i in range(n):
        t = i / RATE
        k = (t % (length / 2)) / (length / 2)                # two breaths
        env = math.sin(math.pi * k) ** 2
        lp += (rng.uniform(-1, 1) - lp) * 0.08               # noise, its highs taken off
        out.append(lp * env * 2.2)
    return out


def buzz(rng, length):
    n = int(length * RATE)
    out = [0.0] * n
    for _ in range(12):
        f0 = rng.uniform(170, 260)
        wander = rng.uniform(4, 9)
        phase_w = rng.uniform(0, 2 * math.pi)
        swell = rng.uniform(0.5, 1.6)
        phase_s = rng.uniform(0, 2 * math.pi)
        ph = 0.0
        for i in range(n):
            t = i / RATE
            f = f0 * (1 + 0.04 * math.sin(2 * math.pi * wander * t + phase_w))
            ph = (ph + f / RATE) % 1.0
            saw = 2 * ph - 1
            amp = 0.5 + 0.5 * math.sin(2 * math.pi * swell * t + phase_s)
            out[i] += saw * amp
    # In and out at the ends, so it never clicks.
    for i in range(n):
        t = i / RATE
        out[i] *= min(1.0, t / 0.25, (length - t) / 0.4)
    return out


def sting(rng, length):
    n = int(length * RATE)
    out = []
    ph = 0.0
    for i in range(n):
        t = i / RATE
        f = 600 + 900 * t / length
        ph = (ph + f / RATE) % 1.0
        out.append((2 * ph - 1) * min(1.0, t / 0.02) * (1 - t / length) ** 0.5 + rng.uniform(-0.05, 0.05))
    return out


def main():
    rng = random.Random(4242)
    for full in (False, True):
        tail = "_full" if full else ""
        save(nest_side(rng, full), f"bees_nest{tail}_side")
        save(nest_top(rng, full), f"bees_nest{tail}_top")
        save(nest_top(rng, False), f"bees_nest{tail}_bottom")
        save(hive_side(rng, full), f"bees_hive{tail}_side")
        save(hive_top(rng), f"bees_hive{tail}_top")
        save(hive_top(rng), f"bees_hive{tail}_bottom")
    for full in (False, True):
        tail = "_full" if full else ""
        save(apiary_side(rng, full), f"bees_apiary{tail}_side")
        save(apiary_top(rng), f"bees_apiary{tail}_top")
        save(apiary_top(rng), f"bees_apiary{tail}_bottom")
    save(press_side(rng), "bees_press_side")
    save(press_top(rng), "bees_press_top")
    save(hive_top(rng), "bees_press_bottom")
    save(honey_lamp(rng), "bees_honey_lamp")
    os.makedirs(MOD, exist_ok=True)
    write(squelch(random.Random(9), 0.6), os.path.join(MOD, "press.ogg"))
    write(puff(random.Random(10), 0.7), os.path.join(MOD, "puff.ogg"))
    write(buzz(random.Random(7), 2.0), os.path.join(MOD, "buzz.ogg"))
    write(sting(random.Random(8), 0.4), os.path.join(MOD, "sting.ogg"))


if __name__ == "__main__":
    main()
