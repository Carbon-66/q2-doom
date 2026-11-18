#!/usr/bin/env python3
"""card/icon.png (52 px) and card/icon-40.png (40 px): an original drawing, a pale skull on a
flame-coloured disc. Drawn at 8x and shrunk. Usage: tools/icon.py"""
import pathlib
from PIL import Image, ImageDraw

def icon(size):
    s = 8
    n = size * s
    im = Image.new('RGBA', (n, n), (0, 0, 0, 0))
    d = ImageDraw.Draw(im)
    # disc: a vertical flame gradient, dark red to orange
    disc = Image.new('RGBA', (n, n), (0, 0, 0, 0))
    g = ImageDraw.Draw(disc)
    for y in range(n):
        t = y / n
        g.line([(0, y), (n, y)], fill=(int(120 + 120 * t), int(20 + 90 * t), 10, 255))
    mask = Image.new('L', (n, n), 0)
    ImageDraw.Draw(mask).ellipse([s, s, n - s, n - s], fill=255)
    im.paste(disc, (0, 0), mask)
    # skull: cranium, jaw, eye sockets, nose, teeth gaps
    bone, dark = (245, 235, 215, 255), (60, 12, 6, 255)
    c = n / 2
    w = n * 0.50
    d.ellipse([c - w / 2, n * 0.20, c + w / 2, n * 0.20 + w * 0.95], fill=bone)
    d.rounded_rectangle([c - w * 0.30, n * 0.56, c + w * 0.30, n * 0.80], radius=n * 0.04, fill=bone)
    e = w * 0.15
    for dx in (-w * 0.21, w * 0.21):
        d.ellipse([c + dx - e, n * 0.43 - e * 0.9, c + dx + e, n * 0.43 + e * 0.9], fill=dark)
    d.polygon([(c, n * 0.53), (c - w * 0.06, n * 0.61), (c + w * 0.06, n * 0.61)], fill=dark)
    for k in (-1, 0, 1):
        x = c + k * w * 0.13
        d.line([(x, n * 0.68), (x, n * 0.80)], fill=dark, width=max(1, int(n * 0.022)))
    return im.resize((size, size), Image.Resampling.LANCZOS)

here = pathlib.Path(__file__).resolve().parent.parent / 'card'
icon(52).save(here / 'icon.png')
icon(40).save(here / 'icon-40.png')
