#!/usr/bin/env python3
"""
ESL Zap - icon generator.
Copyright (c) 2026 SangueFreddo. Licensed under the MIT License.

Draws every ESL Zap icon as 1-bit pixel art and writes the PNG files
used by the Flipper build (images/) plus the app icon (esl_zap.png).
"""
import math
import os

from PIL import Image, ImageDraw

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, ".."))
OUT = os.path.join(ROOT, "images")

INK = 0  # black pixel = lit on the Flipper screen
PAPER = 1


def canvas(w, h):
    img = Image.new("1", (w, h), PAPER)
    return img, ImageDraw.Draw(img)


def save(img, name):
    os.makedirs(OUT, exist_ok=True)
    img.save(os.path.join(OUT, name))


def icon_image():
    img, d = canvas(32, 32)
    d.rounded_rectangle([1, 4, 30, 27], radius=3, outline=INK, width=2)
    d.ellipse([20, 8, 25, 13], fill=INK)
    d.polygon([(4, 24), (12, 13), (17, 19), (20, 16), (27, 24)], fill=INK)
    return img


def icon_text():
    img, d = canvas(32, 32)
    d.rectangle([5, 1, 26, 30], outline=INK, width=2)
    # big letter T
    d.rectangle([9, 5, 22, 7], fill=INK)
    d.rectangle([14, 5, 17, 15], fill=INK)
    for y in (19, 23, 27):
        d.line([9, y, 22 if y != 27 else 17, y], fill=INK, width=2)
    return img


def icon_test():
    img, d = canvas(32, 32)
    d.rectangle([1, 3, 30, 28], outline=INK, width=2)
    sq = 4
    for row in range(3):
        for col in range(6):
            if (row + col) % 2 == 0:
                x = 4 + col * sq
                y = 6 + row * sq
                d.rectangle([x, y, x + sq - 1, y + sq - 1], fill=INK)
    # grey ramp
    for x in range(4, 28):
        level = (x - 4) / 24.0
        for y in range(20, 26):
            if ((x + y) % 2 == 0 and level < 0.66) or level < 0.33:
                d.point((x, y), fill=INK)
    return img


def icon_tags():
    # An ESL shelf label: left price text, right dark promo block, plus an
    # "add" badge in the corner so it reads as "add your tags here".
    img, d = canvas(32, 32)
    # label body (landscape e-paper screen)
    d.rounded_rectangle([0, 7, 25, 27], radius=2, outline=INK)
    d.line([3, 10, 3, 24], fill=INK)  # inner left margin tick
    # left side: price lines (a big one + two thin ones)
    d.rectangle([5, 11, 14, 14], fill=INK)
    d.line([5, 17, 13, 17], fill=INK)
    d.line([5, 20, 11, 20], fill=INK)
    d.line([5, 23, 15, 23], fill=INK)
    # right side: dark promo block (like "BUY ONE GET ONE FREE")
    d.rectangle([17, 10, 23, 24], fill=INK)
    # "add" badge: white disc with a black plus, overlapping the corner
    d.ellipse([20, 1, 31, 12], fill=PAPER, outline=INK, width=2)
    d.rectangle([25, 4, 26, 9], fill=INK)
    d.rectangle([23, 6, 28, 7], fill=INK)
    return img


def icon_page():
    img, d = canvas(32, 32)
    d.rectangle([11, 2, 29, 21], outline=INK)
    d.rectangle([7, 6, 25, 25], fill=PAPER, outline=INK)
    d.rectangle([3, 10, 21, 29], fill=PAPER, outline=INK, width=2)
    d.text((8, 13), "2", fill=INK)
    d.line([8, 25, 16, 25], fill=INK)
    return img


def icon_settings():
    img, d = canvas(32, 32)
    cx, cy = 15.5, 15.5
    teeth = 8
    pts = []
    for i in range(teeth * 2):
        a = math.pi * i / teeth
        r = 14 if i % 2 == 0 else 10.5
        a0 = a - math.pi / (teeth * 2) * 0.55
        a1 = a + math.pi / (teeth * 2) * 0.55
        pts.append((cx + r * math.cos(a0), cy + r * math.sin(a0)))
        pts.append((cx + r * math.cos(a1), cy + r * math.sin(a1)))
    d.polygon(pts, fill=INK)
    d.ellipse([10, 10, 21, 21], fill=PAPER)
    d.ellipse([13, 13, 18, 18], fill=INK)
    return img


def icon_help():
    img, d = canvas(32, 32)
    d.ellipse([1, 1, 30, 30], outline=INK, width=2)
    d.arc([10, 6, 21, 17], start=180, end=40, fill=INK, width=3)
    d.line([19, 15, 16, 18], fill=INK, width=3)
    d.rectangle([14, 18, 17, 20], fill=INK)
    d.rectangle([14, 23, 17, 26], fill=INK)
    return img


def icon_tag_screen():
    """e-paper tag used on the transmission screen (24x18)."""
    img, d = canvas(24, 18)
    d.rounded_rectangle([0, 0, 23, 17], radius=2, outline=INK)
    d.rectangle([3, 3, 20, 14], outline=INK)
    d.rectangle([5, 5, 11, 8], fill=INK)
    d.line([5, 11, 18, 11], fill=INK)
    return img


def icon_flipper():
    """Flipper seen from the top with its IR window (20x14)."""
    img, d = canvas(20, 14)
    d.rounded_rectangle([0, 1, 19, 12], radius=4, outline=INK)
    d.rectangle([4, 4, 11, 9], outline=INK)
    d.ellipse([13, 5, 16, 8], outline=INK)
    d.rectangle([19, 5, 19, 8], fill=INK)
    return img


def icon_ok():
    img, d = canvas(15, 15)
    d.ellipse([0, 0, 14, 14], fill=INK)
    d.line([3, 7, 6, 10, 11, 4], fill=PAPER, width=2)
    return img


def icon_fail():
    img, d = canvas(15, 15)
    d.ellipse([0, 0, 14, 14], fill=INK)
    d.line([4, 4, 10, 10], fill=PAPER, width=2)
    d.line([10, 4, 4, 10], fill=PAPER, width=2)
    return img


APP_ICON = [
    "..........",
    "....######",
    ".#..#....#",
    "#.#.#.##.#",
    "#.#.#.##.#",
    "#.#.#....#",
    ".#..#.##.#",
    "....#....#",
    "....######",
    "..........",
]


def app_icon():
    img, _ = canvas(10, 10)
    for y, row in enumerate(APP_ICON):
        for x, c in enumerate(row):
            if c == "#":
                img.putpixel((x, y), INK)
    return img


def main():
    save(icon_image(), "home_image_32x32.png")
    save(icon_text(), "home_text_32x32.png")
    save(icon_test(), "home_test_32x32.png")
    save(icon_tags(), "home_tags_32x32.png")
    save(icon_page(), "home_page_32x32.png")
    save(icon_settings(), "home_settings_32x32.png")
    save(icon_help(), "home_help_32x32.png")
    save(icon_tag_screen(), "tx_tag_24x18.png")
    save(icon_flipper(), "tx_flipper_20x14.png")
    save(icon_ok(), "tx_ok_15x15.png")
    save(icon_fail(), "tx_fail_15x15.png")
    app_icon().save(os.path.join(ROOT, "esl_zap.png"))
    print("icons written to", OUT)


if __name__ == "__main__":
    main()
