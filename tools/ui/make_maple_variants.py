#!/usr/bin/env python3
# SPDX-FileCopyrightText: Copyright 2026 Erable
# SPDX-License-Identifier: GPL-3.0-or-later
"""Makes the Spring Maple and Winter Maple themes from Maple (dist/qt_themes/maple): its
stylesheet and icons recoloured by one rule. Maple's warm greys take the season's tint and its
maple accent the season's colour; white and the status colours (success, warning, danger) stay.

    python3 tools/ui/make_maple_variants.py

Needs Pillow. Prints the palette colours main_window.cpp's ThemePalette uses for each theme."""
import colorsys
import re
import shutil
from pathlib import Path

from PIL import Image

ROOT = Path(__file__).resolve().parents[2] / "dist" / "qt_themes"
SOURCE = ROOT / "maple"

# grey_hue/accent_hue in degrees; the factors scale saturation and lightness.
SEASONS = {
    "maple_spring": {
        "comment": "Erable, Spring Maple",
        "grey_hue": 100, "grey_sat": 1.15,
        "accent_hue": 100, "accent_sat": 0.7, "accent_light": 0.84,
    },
    "maple_winter": {
        "comment": "Erable, Winter Maple",
        "grey_hue": 212, "grey_sat": 1.6,
        "accent_hue": 202, "accent_sat": 0.82, "accent_light": 1.04,
    },
}

# Colours that keep their meaning in every season
KEEP = {(0x5c, 0xb8, 0x7a), (0xd9, 0xa4, 0x41), (0xc9, 0x5d, 0x5d), (56, 189, 225), (1, 253, 201)}


def recolour(rgb, season):
    if rgb in KEEP:
        return rgb
    r, g, b = (c / 255 for c in rgb)
    h, l, s = colorsys.rgb_to_hls(r, g, b)
    if l > 0.97 or (s < 0.02 and l > 0.9):
        return rgb  # white text and highlights
    degrees = h * 360
    accent = s >= 0.3 and (degrees < 45 or degrees > 340)
    if accent:
        offset = ((degrees + 180) % 360 - 180) - 18  # around Maple's #e8622a
        h = ((season["accent_hue"] + offset * 0.5) % 360) / 360
        s = min(1.0, s * season["accent_sat"])
        l = min(0.86, l * season["accent_light"])
    elif s < 0.3:
        h = season["grey_hue"] / 360
        s = min(0.3, s * season["grey_sat"])
    else:
        return rgb
    r, g, b = colorsys.hls_to_rgb(h, l, s)
    return tuple(round(c * 255) for c in (r, g, b))


def hex_colour(rgb):
    return "#{:02x}{:02x}{:02x}".format(*rgb)


def recolour_stylesheet(text, season):
    def hex_sub(match):
        value = match.group(1)
        rgb = tuple(int(value[i:i + 2], 16) for i in (0, 2, 4))
        return hex_colour(recolour(rgb, season))

    def rgba_sub(match):
        rgb = tuple(int(match.group(i)) for i in (1, 2, 3))
        r, g, b = recolour(rgb, season)
        return f"rgba({r}, {g}, {b},{match.group(4)})"

    text = re.sub(r"#([0-9a-fA-F]{6})\b", hex_sub, text)
    return re.sub(r"rgba\(\s*(\d+),\s*(\d+),\s*(\d+),([^)]*)\)", rgba_sub, text)


def recolour_icon(source, target, season):
    image = Image.open(source).convert("RGBA")
    cache = {}
    pixels = image.load()
    for y in range(image.height):
        for x in range(image.width):
            r, g, b, a = pixels[x, y]
            if a == 0:
                continue
            key = (r, g, b)
            if key not in cache:
                cache[key] = recolour(key, season)
            pixels[x, y] = (*cache[key], a)
    image.save(target, optimize=True)


def main():
    palette_colours = {
        "window": (0x1d, 0x1a, 0x16), "surface": (0x26, 0x23, 0x1f), "text": (0xf1, 0xee, 0xea),
        "disabled": (0x5c, 0x59, 0x55), "accent": (0xe8, 0x62, 0x2a),
        "alternate": (0x21, 0x1e, 0x1a), "tooltip": (0x29, 0x26, 0x22),
        "placeholder": (0x77, 0x74, 0x70), "light": (0x3b, 0x38, 0x34),
        "midlight": (0x31, 0x2e, 0x2a), "mid": (0x29, 0x26, 0x22), "dark": (0x1a, 0x17, 0x13),
        "link": (0xf2, 0x80, 0x3f), "link_visited": (0xc7, 0x46, 0x1f),
    }
    for name, season in SEASONS.items():
        target = ROOT / name
        if target.exists():
            shutil.rmtree(target)
        (target / "icons").mkdir(parents=True)
        index = (SOURCE / "index.theme").read_text()
        index = index.replace("Name=maple", f"Name={name}")
        index = re.sub(r"Comment=.*", f"Comment={season['comment']}", index)
        (target / "index.theme").write_text(index)
        qrc = (SOURCE / "style.qrc").read_text()
        qrc = qrc.replace('prefix="icons/maple"', f'prefix="icons/{name}"')
        qrc = qrc.replace('prefix="maple"', f'prefix="{name}"')
        (target / "style.qrc").write_text(qrc)
        qss = (SOURCE / "style.qss").read_text()
        qss = qss.replace(":/maple/", f":/{name}/")
        qss = qss.replace("/* Erable, from Lichii Dark",
                          f"/* {season['comment']}: Maple recoloured by tools/ui/make_maple_variants.py."
                          "\n   Maple: from Lichii Dark", 1)
        (target / "style.qss").write_text(recolour_stylesheet(qss, season))
        for icon in sorted((SOURCE / "icons").glob("*.png")):
            recolour_icon(icon, target / "icons" / icon.name, season)
        print(name)
        for key, rgb in palette_colours.items():
            print(f"  {key:13s} {hex_colour(recolour(rgb, season))}")


if __name__ == "__main__":
    main()
