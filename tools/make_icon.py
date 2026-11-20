#!/usr/bin/env python3
"""Draws pkgfiles/ICON0.PNG (320x176): MS5 blue waves + title."""
import math, os
from PIL import Image, ImageDraw, ImageFont
W, H = 320, 176
im = Image.new("RGB", (W, H))
px = im.load()
for y in range(H):
    t = y / H
    for x in range(W):
        px[x, y] = (int(13 * (1 - t) + 2 * t), int(42 * (1 - t) + 6 * t), int(122 * (1 - t) + 23 * t))
ov = Image.new("RGBA", (W, H), (0, 0, 0, 0))
d = ImageDraw.Draw(ov)
for k, (base, a) in enumerate([(100, 70), (124, 60), (146, 50)]):
    pts = [(x, base + 10 * math.sin(x * 0.03 + k * 1.7)) for x in range(0, W + 1, 4)] + [(W, H), (0, H)]
    d.polygon(pts, fill=(106, 160, 255, a))
im = Image.alpha_composite(im.convert("RGBA"), ov)
d = ImageDraw.Draw(im)
bold = "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf"
for p in ("fonts/Tajawal-Bold.ttf",):
    if os.path.exists(p): bold = p
d.text((24, 40), "MS5", font=ImageFont.truetype(bold, 54), fill=(255, 255, 255))
d.text((26, 98), "LUNCHER", font=ImageFont.truetype(bold, 26), fill=(106, 163, 255))
os.makedirs("pkgfiles", exist_ok=True)
im.convert("RGB").save("pkgfiles/ICON0.PNG")
print("wrote pkgfiles/ICON0.PNG")
