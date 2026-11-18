#!/usr/bin/env python3
"""A raw portrait page from fbshim (320 x 375 BGRA) as the Q2 shows it: the landscape view
(375 x 320), with the glass's 80 px rounded corners drawn in. Usage: view.py IN.raw OUT.png"""
import sys
from PIL import Image, ImageDraw

raw = open(sys.argv[1], 'rb').read()
page = Image.frombytes('RGBA', (320, 375), raw, 'raw', 'BGRA')
# view (X, Y) is page row X, column 319 - Y
src = page.convert('RGB').load()
view = Image.new('RGB', (375, 320))
dst = view.load()
for X in range(375):
    for Y in range(320):
        dst[X, Y] = src[319 - Y, X]
mask = Image.new('L', view.size, 0)
ImageDraw.Draw(mask).rounded_rectangle([0, 0, 374, 319], radius=80, fill=255)
glass = Image.new('RGB', view.size, (60, 60, 60))
glass.paste(view, (0, 0), mask)
glass.resize((750, 640), Image.Resampling.NEAREST).save(sys.argv[2])
