#!/usr/bin/env python3
"""
Turns the client logos in docs/assets/clients/ into terminal art and writes src/ui/LogoArt.cpp.

Run from the repository root:   python3 tools/make_logo_art.py          (needs Pillow: pip install pillow)
Add --preview to also write PNG previews of what each terminal tier will look like to docs/assets/clients/preview_*.png.

How it works (the "half block" trick):
  Every terminal character cell is about twice as tall as it is wide. The character U+2580 (upper half block) fills the top half of a
  cell, so a cell can show TWO pixels: the foreground colour is the top pixel and the background colour is the bottom pixel.

Three variants are generated per logo, the TUI picks one with chooseColorDepth():
  TrueColor  24-bit colour, ESC[38;2;R;G;Bm       (Windows Terminal, VS Code, iTerm2, Kitty, GNOME Terminal, ...)
  Ansi256    the xterm 256 colour palette         (macOS Terminal.app, plain xterm-256color, tmux without truecolor)
  None       no colour at all, only block characters, so it still works with NO_COLOR or when output is piped

The "powered by TIVIDY" line is removed from the picture: it is too small to survive at terminal resolution, so the TUI prints it as
real text (ClientProfile::tagline) underneath instead.
"""

import sys
from PIL import Image, ImageFilter

LOGOS = [
    # (function name in C++, source image, display width in character cells)
    # the last value thickens the mark: TIVIDY's diamond is drawn with hairlines that would fade to nothing when shrunk to terminal size
    ("hudson", "docs/assets/clients/hudson_bank_logo.png", 76, False),
    ("warped", "docs/assets/clients/warped_logo.png", 64, False),
    ("tividy", "docs/assets/tividy_logo.jpg", 76, True),
]

WHITE = (255, 255, 255)
UPPER_HALF = "▀"
LOWER_HALF = "▄"
FULL = "█"


def ink(p):
    """True if the pixel is visibly not the white background."""
    return sum(255 - c for c in p) > 90


def mono_ink(p):
    """Stricter than ink(): with no colour, anti-aliased grey edges would fuse thin letters into blobs, so only dark or saturated pixels count."""
    return (max(p) - min(p) > 80 and ink(p)) or sum(255 - c for c in p) > 330


def runs(flags):
    """Groups consecutive True values into (start, end_exclusive) pairs."""
    out, start = [], None
    for i, f in enumerate(flags):
        if f and start is None:
            start = i
        if not f and start is not None:
            out.append((start, i))
            start = None
    if start is not None:
        out.append((start, len(flags)))
    return out


def mark_columns(img):
    """Column range (start, end) of the logo mark, the first group of ink on the left. None if the logo has no separate mark."""
    w, h = img.size
    px = img.load()
    cols = [any(ink(px[x, y]) for y in range(h)) for x in range(w)]
    col_runs = runs(cols)
    if len(col_runs) < 2:
        return None
    # merge ink runs that are close together (letters of the wordmark), the first remaining run is the mark
    merged = [list(col_runs[0])]
    for s, e in col_runs[1:]:
        if s - merged[-1][1] < 12:
            merged[-1][1] = e
        else:
            merged.append([s, e])
    return merged[0][0], merged[0][1]


def thicken_mark(img):
    """Fattens the strokes of the mark only (darker pixels spread outwards) so hairlines survive the shrinking."""
    columns = mark_columns(img)
    if columns is None:
        return img
    box = (max(columns[0] - 4, 0), 0, columns[1] + 4, img.height)
    img.paste(img.crop(box).filter(ImageFilter.MinFilter(7)), box[:2])
    return img


def remove_tagline(img, bg=WHITE):
    """Erases the small 'powered by TIVIDY' line: the last row-run to the right of the logo mark."""
    w, h = img.size
    px = img.load()
    columns = mark_columns(img)
    if columns is None:
        return img
    x0 = columns[1] + 2
    rows = [any(ink(px[x, y]) for x in range(x0, w)) for y in range(h)]
    row_runs = runs(rows)
    if len(row_runs) < 2:
        return img
    top, bottom = row_runs[-1]
    for y in range(top - 1, min(bottom + 2, h)):
        for x in range(x0, w):
            px[x, y] = bg
    return img


def load(path, cells_wide, thicken):
    img = Image.open(path).convert("RGB")
    bg = img.getpixel((1, 1))   # the logo's own background: white for the client logos, cream for TIVIDY
    img = remove_tagline(img, bg)
    if thicken:
        img = thicken_mark(img)
    # crop to the ink, then add a small white margin so the logo sits on a white card in any terminal theme
    px = img.load()
    xs = [x for x in range(img.width) for y in range(img.height) if ink(px[x, y])]
    ys = [y for x in range(img.width) for y in range(img.height) if ink(px[x, y])]
    img = img.crop((min(xs), min(ys), max(xs) + 1, max(ys) + 1))
    margin_x, margin_y = 4, 2   # in cells and pixel rows
    scale = (cells_wide - 2 * margin_x) / img.width
    inner_h = max(2, round(img.height * scale))
    img = img.resize((cells_wide - 2 * margin_x, inner_h), Image.LANCZOS)
    card_h = inner_h + 2 * margin_y
    if card_h % 2:
        card_h += 1
    card = Image.new("RGB", (cells_wide, card_h), bg)
    card.paste(img, (margin_x, (card_h - inner_h) // 2))
    return card


def cube_index(v):
    levels = [0, 95, 135, 175, 215, 255]
    return min(range(6), key=lambda i: abs(levels[i] - v)), levels


def to_256(p):
    """Nearest colour in the xterm palette (indexes 16..255: the 6x6x6 cube and the 24 greys)."""
    ri, levels = cube_index(p[0])
    gi, _ = cube_index(p[1])
    bi, _ = cube_index(p[2])
    cube = (levels[ri], levels[gi], levels[bi])
    cube_idx = 16 + 36 * ri + 6 * gi + bi
    grey_i = min(range(24), key=lambda i: abs((8 + 10 * i) - sum(p) / 3))
    grey = (8 + 10 * grey_i,) * 3
    dist = lambda a, b: sum((x - y) ** 2 for x, y in zip(a, b))
    return cube_idx if dist(p, cube) <= dist(p, grey) else 232 + grey_i


def color_lines(card, mode):
    """One string per text row; consecutive cells with the same colours share one escape sequence."""
    px = card.load()
    lines = []
    for row in range(card.height // 2):
        out, last = [], None
        for x in range(card.width):
            top, bottom = px[x, 2 * row], px[x, 2 * row + 1]
            if mode == "truecolor":
                fg, bg = top, bottom
                seq = "\x1b[38;2;%d;%d;%dm\x1b[48;2;%d;%d;%dm" % (fg + bg)
            else:
                fg, bg = to_256(top), to_256(bottom)
                seq = "\x1b[38;5;%dm\x1b[48;5;%dm" % (fg, bg)
            if (fg, bg) != last:
                out.append(seq)
                last = (fg, bg)
            out.append(UPPER_HALF)
        out.append("\x1b[0m")
        lines.append("".join(out))
    return lines


def mono_lines(card):
    px = card.load()
    lines = []
    for row in range(card.height // 2):
        s = ""
        for x in range(card.width):
            t, b = mono_ink(px[x, 2 * row]), mono_ink(px[x, 2 * row + 1])
            s += FULL if t and b else UPPER_HALF if t else LOWER_HALF if b else " "
        lines.append(s.rstrip())
    return lines


def cpp_literal(text):
    out = ""
    for ch in text:
        if ch == "\x1b":
            out += "\\x1b"
        elif ch in '"\\':
            out += "\\" + ch
        else:
            out += ch
    # a hex escape swallows following hex digits, so end the literal after every ESC ... safe because ESC is always followed by '['
    return '"' + out + '"'


def array(name, lines):
    body = ",\n".join("\t\t\t" + cpp_literal(l) for l in lines)
    return "\t\tconst char* const %s[] =\n\t\t{\n%s\n\t\t};\n\t\tconst std::size_t %s_ROWS = %d;\n" % (name, body, name, len(lines))


def preview(card, mode, path):
    """Draws what the terminal will show: every cell is two stacked pixels, enlarged so it is easy to look at."""
    scale = 6
    img = Image.new("RGB", (card.width * scale, card.height * scale), WHITE)
    src = card.load()
    dst = img.load()
    for y in range(card.height):
        for x in range(card.width):
            p = src[x, y]
            if mode == "ansi256":
                idx = to_256(p)
                if idx >= 232:
                    p = (8 + 10 * (idx - 232),) * 3
                else:
                    i = idx - 16
                    lv = [0, 95, 135, 175, 215, 255]
                    p = (lv[i // 36], lv[(i // 6) % 6], lv[i % 6])
            elif mode == "none":
                p = (0, 0, 0) if mono_ink(p) else WHITE
            for dy in range(scale):
                for dx in range(scale):
                    dst[x * scale + dx, y * scale + dy] = p
    img.save(path)


def main():
    want_preview = "--preview" in sys.argv
    parts = []
    for name, path, cells, thicken in LOGOS:
        card = load(path, cells, thicken)
        parts.append((name, card))
        if want_preview:
            for mode in ("truecolor", "ansi256", "none"):
                preview(card, mode, "docs/assets/clients/preview_%s_%s.png" % (name, mode))

    arrays, accessors = [], []
    for name, card in parts:
        up = name.upper()
        arrays.append(array(up + "_TRUECOLOR", color_lines(card, "truecolor")))
        arrays.append(array(up + "_ANSI256", color_lines(card, "256")))
        arrays.append(array(up + "_PLAIN", mono_lines(card)))
        accessors.append(
            "\tstd::string %sLogo(ColorDepth depth)\n\t{\n"
            "\t\tif (depth == ColorDepth::TrueColor) return join(%s_TRUECOLOR, %s_TRUECOLOR_ROWS);\n"
            "\t\tif (depth == ColorDepth::Ansi256) return join(%s_ANSI256, %s_ANSI256_ROWS);\n"
            "\t\treturn join(%s_PLAIN, %s_PLAIN_ROWS);\n\t}\n" % (name, up, up, up, up, up, up))

    src = '''/*
Aidan du Bruyn (25040104)
Connor Human (22752154)
Ishmael Ngwasheng (24642542)
Jay Macaskill (25198387)
Keval Govind (24904407)
Ruan van Tonder (25284607)
Thabo Ngwabi (24742512)

COS 214 (Software Modelling) TIVIDY
Last Modified: 27 October 2026

LogoArt.cpp

GENERATED by tools/make_logo_art.py from the images in docs/assets/clients/. Do not edit by hand: change the script or the images and run it again.
*/

#ifndef TIVIDY_UI_LOGOART_CPP
#define TIVIDY_UI_LOGOART_CPP

#include <cstddef>

#include "ui/LogoArt.h"

namespace tividy
{
	namespace
	{
		// every row ends with a reset (ESC[0m) so a colour can never leak into the text printed after the logo
		std::string join(const char* const* rows, std::size_t count)
		{
			std::string out;
			for (std::size_t i = 0; i < count; ++i)
			{
				out += rows[i];
				out += "\\n";
			}
			return out;
		}

%s
	} // anonymous namespace

%s} // namespace tividy

#endif // TIVIDY_UI_LOGOART_CPP
''' % ("\n".join(arrays), "\n".join(accessors))
    with open("src/ui/LogoArt.cpp", "w", encoding="utf-8") as f:
        f.write(src)
    for name, card in parts:
        print("%s: %d columns x %d rows" % (name, card.width, card.height // 2))


if __name__ == "__main__":
    main()
