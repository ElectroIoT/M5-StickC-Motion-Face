#!/usr/bin/env python3
"""Build docs/images/m5burner_cover.png (800x600) from the real screenshots.

  python tools/make_cover.py
"""
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parent.parent
SHOTS = ROOT / "docs" / "screenshots"
OUT = ROOT / "docs" / "images" / "m5burner_cover.png"

BG, INK, SOFT, BLUE = (12, 12, 24), (241, 244, 255), (140, 152, 184), (80, 200, 255)


def font(size, bold=False):
    for name in (("segoeuib.ttf" if bold else "segoeui.ttf"), "arialbd.ttf" if bold else "arial.ttf"):
        try:
            return ImageFont.truetype(name, size)
        except OSError:
            pass
    return ImageFont.load_default()


def main():
    img = Image.new("RGB", (800, 600), BG)
    d = ImageDraw.Draw(img)
    d.text((400, 62), "StickC Motion Face", font=font(54, True), fill=INK, anchor="mm")
    d.text((400, 112), "Shake it, tilt it, leave it alone - it reacts", font=font(24), fill=SOFT, anchor="mm")

    shots = [("cat", "idle"), ("robot", "happy"), ("ghost", "dizzy"), ("cyclops", "scared")]
    w, h, gap = 150, 300, 30
    x0 = (800 - (4 * w + 3 * gap)) // 2
    for col, (c, m) in enumerate(shots):
        p = SHOTS / f"{c}_{m}.png"
        if not p.exists():
            continue
        im = Image.open(p).convert("RGB").resize((w, h), Image.NEAREST)
        x, y = x0 + col * (w + gap), 150
        d.rounded_rectangle((x - 5, y - 5, x + w + 5, y + h + 5), 14, outline=(40, 46, 70), width=3)
        img.paste(im, (x, y))
    d.text((400, 505), "M5StickC  -  4 characters  -  motion reactive", font=font(24), fill=BLUE, anchor="mm")
    d.text((400, 550), "github.com/ElectroIoT/M5-StickC-Motion-Face", font=font(18), fill=SOFT, anchor="mm")
    OUT.parent.mkdir(parents=True, exist_ok=True)
    img.save(OUT)
    print("saved", OUT)


if __name__ == "__main__":
    main()
