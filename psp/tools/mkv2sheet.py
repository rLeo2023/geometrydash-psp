#!/usr/bin/env python3
"""
mkv2sheet.py - the 2.0 objects' sprite sheet, from the user's own copy of GD

psp/gfx2/sprites_v2.list names the sprites (gen2objects.py writes it, in
texture order). Each one is cut from GD's sheets (GJ_GameSheet*.plist/png,
the standard resolution: the game's sprites are at that size), back to its
untrimmed size so it centers like GD draws it, and the lot packed like any
other sheet (mkatlas.py).

Usage: mkv2sheet.py <GD Resources folder> <work folder> <out.psheet>
"""
import os
import plistlib
import subprocess
import sys

from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
SHEETS = ['GJ_GameSheet', 'GJ_GameSheet02', 'GJ_GameSheet03', 'GJ_GameSheet04', 'GJ_GameSheetGlow']


def pair(s):
    a, b = s.strip('{}').split(',')
    return float(a), float(b)


def rect(s):
    s = s.replace('{', '').replace('}', '')
    x, y, w, h = (int(float(v)) for v in s.split(','))
    return x, y, w, h


def main():
    res, work, out = sys.argv[1:4]
    names = [l.strip() for l in open(os.path.join(HERE, '..', 'gfx2', 'sprites_v2.list')) if l.strip()]

    frames = {}
    for s in SHEETS:
        p = os.path.join(res, s + '.plist')
        if not os.path.exists(p):
            continue
        meta = plistlib.load(open(p, 'rb'))
        png = os.path.join(res, meta['metadata']['textureFileName'])
        for n, d in meta['frames'].items():
            frames.setdefault(n, (png, d))

    os.makedirs(work, exist_ok=True)
    sheets = {}
    t3s = ['--atlas --border transparent -f rgba8888 -z auto']
    for n in names:
        png, d = frames[n]
        if png not in sheets:
            sheets[png] = Image.open(png).convert('RGBA')
        x, y, w, h = rect(d['textureRect'])
        if d.get('textureRotated'):
            # stored turned a quarter clockwise: its rectangle is h x w
            piece = sheets[png].crop((x, y, x + h, y + w)).rotate(90, expand=True)
        else:
            piece = sheets[png].crop((x, y, x + w, y + h))
        sw, sh = (int(v) for v in pair(d['spriteSourceSize']))
        ox, oy = pair(d['spriteOffset'])
        full = Image.new('RGBA', (sw, sh), (0, 0, 0, 0))
        # the offset is from the center, y up
        px = int(round((sw - piece.width) / 2 + ox))
        py = int(round((sh - piece.height) / 2 - oy))
        full.paste(piece, (px, py))
        full.save(os.path.join(work, n))
        t3s.append(n)

    t3s_path = os.path.join(work, 'sprites_v2.t3s')
    open(t3s_path, 'w').write('\n'.join(t3s) + '\n')
    subprocess.check_call([sys.executable, os.path.join(HERE, 'mkatlas.py'), t3s_path, out])


if __name__ == '__main__':
    main()
