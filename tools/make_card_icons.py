# The bag's pictures of mods/cards' forty cards as items: each classic face
# (mods/cards/c<n>.png, from tools/make_cards.py) taken down to a card 20 by
# 32 in the middle of a clear 32 square, its corners cut round and framed
# dark, as assets/textures/cards_<value>_of_<suit>.png -- the name the bag asks for.
#
#   python3 tools/make_card_icons.py
#
# n = suit * 10 + value, suits coins, cups, swords, clubs (denari, coppe,
# spade, bastoni), values 1..7, jack 8, knight 9, king 10: the item names
# mods/cards registers.
import os

from PIL import Image, ImageDraw

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
MOD = os.path.join(ROOT, "mods", "cards")
OUT = os.path.join(ROOT, "assets", "textures")

SUITS = ["coins", "cups", "swords", "clubs"]
VALUES = ["ace", "two", "three", "four", "five", "six", "seven", "jack", "knight", "king"]
S, W, H = 32, 20, 32   # the tile, and the card in it
K = 8                  # the rounded corners drawn at K times, then taken down


def icon(n):
    face = Image.open(os.path.join(MOD, f"c{n}.png")).convert("RGBA").resize((W, H), Image.LANCZOS)
    mask = Image.new("L", (W * K, H * K), 0)
    ImageDraw.Draw(mask).rounded_rectangle((0, 0, W * K - 1, H * K - 1), radius=2 * K, fill=255)
    face.putalpha(mask.resize((W, H), Image.LANCZOS))
    # the face's own dark frame is too thin to last the taking down
    ImageDraw.Draw(face).rounded_rectangle((0, 0, W - 1, H - 1), radius=2, outline=(70, 50, 30, 255))
    out = Image.new("RGBA", (S, S), (0, 0, 0, 0))
    out.paste(face, ((S - W) // 2, (S - H) // 2), face)
    return out


if __name__ == "__main__":
    for s, suit in enumerate(SUITS):
        for v, value in enumerate(VALUES):
            icon(s * 10 + v + 1).save(os.path.join(OUT, f"cards_{value}_of_{suit}.png"), optimize=True)
