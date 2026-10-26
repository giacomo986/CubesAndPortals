# The two decks of mods/cards, forty faces and a back each:
#
#   the classic deck   from the art tools/make_card_art.py renders in Blender
#                      (~/kart_assets/cards/art): c<n>.png, back.png
#   the illustrated    from the pictures ElevenLabs draws a few a day
#                      (~/kart_assets/cards/elevenlabs, tools/elevenlabs_deck.json):
#                      ec<n>.png, eback.png; a 1..7 is its own picture
#                      (<value>_<suit>.png) or else the suit's emblem as pips,
#                      and a picture not drawn yet is the Blender one meanwhile
#
#   python3 tools/make_cards.py
#
# n = suit * 10 + value, suits denari, coppe, spade, bastoni, values 1..7,
# fante 8, cavallo 9, re 10. Every face also darkened, d<n>.png and ed<n>.png:
# a card that may not be played now. Drawn four times over and taken down, so
# the thin lines stay smooth.
import os

from PIL import Image, ImageChops, ImageDraw, ImageFont

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
MOD = os.path.join(ROOT, "mods", "cards")
ART = os.path.expanduser("~/kart_assets/cards/art")
DRAWN = os.path.expanduser("~/kart_assets/cards/elevenlabs")
FONT = "/usr/share/fonts/truetype/dejavu/DejaVuSerif-Bold.ttf"

W, H = 200, 320      # what goes in the mod
K = 4                # drawn at K times that
SUITS = ["denari", "coppe", "spade", "bastoni"]
INK = {"denari": (175, 120, 10), "coppe": (175, 30, 25), "spade": (25, 55, 140), "bastoni": (30, 105, 40)}
PAPER = (250, 244, 228, 255)

# Where the pips of 2..7 go, as fractions of the space inside the frame.
PIPS = {
    1: [(0.5, 0.5)],
    2: [(0.5, 0.23), (0.5, 0.77)],
    3: [(0.5, 0.17), (0.5, 0.5), (0.5, 0.83)],
    4: [(0.28, 0.23), (0.72, 0.23), (0.28, 0.77), (0.72, 0.77)],
    5: [(0.28, 0.2), (0.72, 0.2), (0.5, 0.5), (0.28, 0.8), (0.72, 0.8)],
    6: [(0.28, 0.17), (0.72, 0.17), (0.28, 0.5), (0.72, 0.5), (0.28, 0.83), (0.72, 0.83)],
    7: [(0.28, 0.15), (0.72, 0.15), (0.5, 0.33), (0.28, 0.52), (0.72, 0.52), (0.28, 0.85), (0.72, 0.85)],
}


def keyed(path):
    """An ElevenLabs picture with its plain background taken out: the colour of
    its corners, gone where a pixel is near it and faded in a little further."""
    im = Image.open(path).convert("RGB")
    corners = [im.getpixel(p) for p in ((2, 2), (im.width - 3, 2), (2, im.height - 3), (im.width - 3, im.height - 3))]
    bg = tuple(sorted(c[i] for c in corners)[1] for i in range(3))
    diff = ImageChops.difference(im, Image.new("RGB", im.size, bg)).convert("L")
    alpha = diff.point(lambda v: 0 if v < 18 else 255 if v > 48 else (v - 18) * 255 // 30)
    out = im.convert("RGBA")
    out.putalpha(alpha)
    return out


def art(name, deck=""):
    path = os.path.join(DRAWN, name + ".png")
    if deck == "e" and os.path.exists(path):
        im = keyed(path)
    else:
        im = Image.open(os.path.join(ART, name + ".png")).convert("RGBA")
    return im.crop(im.getbbox())


def fit(im, w, h):
    s = min(w / im.width, h / im.height)
    return im.resize((max(1, int(im.width * s)), max(1, int(im.height * s))), Image.LANCZOS)


def paste_mid(card, im, cx, cy):
    card.alpha_composite(im, (int(cx - im.width / 2), int(cy - im.height / 2)))


def blank():
    card = Image.new("RGBA", (W * K, H * K), PAPER)
    d = ImageDraw.Draw(card)
    d.rounded_rectangle((3 * K, 3 * K, (W - 3) * K, (H - 3) * K), radius=10 * K, outline=(60, 40, 30), width=2 * K)
    return card, d


def index(card, d, text, colour):
    """The value at two corners, the second upside down: small and tight in
    the corner, so it still shows on a card behind another in a fanned hand."""
    font = ImageFont.truetype(FONT, 24 * K)
    for flip in (False, True):
        tile = Image.new("RGBA", (32 * K, 32 * K), (0, 0, 0, 0))
        td = ImageDraw.Draw(tile)
        td.text((16 * K, 15 * K), text, font=font, fill=colour, anchor="mm")
        if text in ("6", "9"):   # underlined, or upside down one is the other
            td.line((10 * K, 28 * K, 22 * K, 28 * K), fill=colour, width=2 * K)
        if flip:
            card.alpha_composite(tile.rotate(180), ((W - 37) * K, (H - 37) * K))
        else:
            card.alpha_composite(tile, (5 * K, 5 * K))


def face(suit_i, value, deck=""):
    suit = SUITS[suit_i]
    card, d = blank()
    colour = INK[suit]
    # the space for the picture, inside the indices
    x0, y0, x1, y1 = 40 * K, 46 * K, (W - 40) * K, (H - 46) * K
    if value <= 7 and deck == "e" and os.path.exists(os.path.join(DRAWN, f"{value}_{suit}.png")):
        # the whole card drawn, its pips and all
        paste_mid(card, fit(art(f"{value}_{suit}", deck), x1 - x0, y1 - y0), (x0 + x1) / 2, (y0 + y1) / 2)
    elif value <= 7:
        sym = art(suit, deck)
        big = value == 1
        size = (x1 - x0) * (0.95 if big else 0.42)
        tall = 1.0 if big else {2: 0.36, 3: 0.29, 4: 0.36, 5: 0.29, 6: 0.27, 7: 0.24}[value]
        pip = fit(sym, size, (y1 - y0) * tall)
        for fx, fy in PIPS[value]:
            paste_mid(card, pip, x0 + (x1 - x0) * fx, y0 + (y1 - y0) * fy)
    else:
        kind = {8: "fante", 9: "cavallo", 10: "re"}[value]
        fig = fit(art(f"{kind}_{suit}", deck), (W - 36) * K, (y1 - y0) + 24 * K)
        paste_mid(card, fig, W * K / 2, (y0 + y1) / 2 + 6 * K)
        d.rounded_rectangle((14 * K, 40 * K, (W - 14) * K, (H - 40) * K), radius=6 * K, outline=colour, width=K)
    index(card, d, str(value), colour)
    return card.resize((W, H), Image.LANCZOS)


def back(deck=""):
    drawn = os.path.join(DRAWN, "back.png")
    if deck == "e" and os.path.exists(drawn):
        # the picture's middle, cut to the card, inside a gold edge
        im = Image.open(drawn).convert("RGBA")
        w = int(im.height * W / H)
        im = im.crop(((im.width - w) // 2, 0, (im.width + w) // 2, im.height)).resize((W * K, H * K), Image.LANCZOS)
        d = ImageDraw.Draw(im)
        d.rounded_rectangle((3 * K, 3 * K, (W - 3) * K, (H - 3) * K), radius=10 * K, outline=(215, 170, 60), width=4 * K)
        return im.resize((W, H), Image.LANCZOS)
    card = Image.new("RGBA", (W * K, H * K), PAPER)
    d = ImageDraw.Draw(card)
    # the illustrated deck, until its back is drawn, in blue
    red, dark, gold = ((150, 25, 30), (105, 15, 20), (215, 170, 60)) if deck == "" else \
        ((25, 55, 130), (15, 30, 85), (215, 170, 60))
    d.rounded_rectangle((3 * K, 3 * K, (W - 3) * K, (H - 3) * K), radius=10 * K, fill=red, outline=dark, width=2 * K)
    m = 14 * K
    d.rectangle((m, m, W * K - m, H * K - m), outline=gold, width=2 * K)
    step = 20 * K
    for i in range(-H * K, W * K + H * K, step):   # a lattice of diamonds
        d.line((i, m, i + (H * K - 2 * m), H * K - m), fill=dark, width=2 * K)
        d.line((i, H * K - m, i + (H * K - 2 * m), m), fill=dark, width=2 * K)
    d.rectangle((0, 0, W * K, m - 1), fill=red)
    d.rectangle((0, H * K - m + 1, W * K, H * K), fill=red)
    d.rectangle((0, 0, m - 1, H * K), fill=red)
    d.rectangle((W * K - m + 1, 0, W * K, H * K), fill=red)
    d.rounded_rectangle((3 * K, 3 * K, (W - 3) * K, (H - 3) * K), radius=10 * K, outline=dark, width=2 * K)
    d.rectangle((m, m, W * K - m, H * K - m), outline=gold, width=2 * K)
    cx, cy, rx, ry = W * K / 2, H * K / 2, 46 * K, 66 * K
    d.ellipse((cx - rx, cy - ry, cx + rx, cy + ry), fill=PAPER, outline=gold, width=3 * K)
    paste_mid(card, fit(art("denari", deck), 70 * K, 70 * K), cx, cy)
    out = card.resize((W, H), Image.LANCZOS)
    # the paper's corners, outside the rounded edge, as the back's red
    return out


if __name__ == "__main__":
    os.makedirs(MOD, exist_ok=True)
    for deck in ("", "e"):
        for s in range(4):
            for v in range(1, 11):
                im = face(s, v, deck).convert("RGB")
                im.save(os.path.join(MOD, f"{deck}c{s * 10 + v}.png"), optimize=True)
                Image.blend(im, Image.new("RGB", im.size, (40, 40, 45)), 0.5).save(
                    os.path.join(MOD, f"{deck}d{s * 10 + v}.png"), optimize=True)
        back(deck).convert("RGB").save(os.path.join(MOD, f"{deck}back.png"), optimize=True)
    have = [f[:-4] for f in os.listdir(DRAWN) if f.endswith(".png")] if os.path.isdir(DRAWN) else []
    print("cards written to", MOD, "-- illustrated from ElevenLabs:", ", ".join(sorted(have)) or "none yet")
