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
SHEETS = ['GJ_GameSheet', 'GJ_GameSheet02', 'GJ_GameSheet03', 'GJ_GameSheet04', 'GJ_GameSheetGlow', 'FireSheet_01']


def pair(s):
    a, b = s.strip('{}').split(',')
    return float(a), float(b)


def rect(s):
    s = s.replace('{', '').replace('}', '')
    x, y, w, h = (int(float(v)) for v in s.split(','))
    return x, y, w, h


def cut(sheet_img, d):
    """A frame back at its untrimmed size."""
    x, y, w, h = rect(d['textureRect'])
    if d.get('textureRotated'):
        # stored turned a quarter clockwise: its rectangle is h x w
        piece = sheet_img.crop((x, y, x + h, y + w)).rotate(90, expand=True)
    else:
        piece = sheet_img.crop((x, y, x + w, y + h))
    sw, sh = (int(v) for v in pair(d['spriteSourceSize']))
    ox, oy = pair(d['spriteOffset'])
    full = Image.new('RGBA', (sw, sh), (0, 0, 0, 0))
    # the offset is from the center, y up
    px = int(round((sw - piece.width) / 2 + ox))
    py = int(round((sh - piece.height) / 2 - oy))
    full.paste(piece, (px, py))
    return full


ROBOT_SIZE = 64


def robot_layer(res, robot, layer):
    """The robot standing (Robot_AnimDesc's idle01_001 pose), its parts in one
    picture: layer 1 the main color's, 2 the second color's (_2 parts)."""
    import re
    meta = plistlib.load(open(os.path.join(res, 'icons', '%s.plist' % robot), 'rb'))
    sheet_img = Image.open(os.path.join(res, 'icons', '%s.png' % robot)).convert('RGBA')
    desc = open(os.path.join(res, 'Robot_AnimDesc.plist'), encoding='utf-8', errors='replace').read()
    start = desc.index('<key>Robot_idle01_001.png</key>')
    block = desc[start:desc.index('<key>Robot_idle01_002.png</key>')]
    parts = []
    for sp in re.findall(r'<key>sprite_\d+</key>\s*<dict>(.*?)</dict>', block, re.S):
        val = dict(re.findall(r'<key>(\w+)</key>\s*<string>([^<]*)</string>', sp))
        parts.append(val)
    parts.sort(key=lambda v: int(v.get('zValue', 0)))

    canvas = Image.new('RGBA', (ROBOT_SIZE, ROBOT_SIZE), (0, 0, 0, 0))
    for v in parts:
        name = v['texture'].replace('robot_01', robot)
        if layer == 2:
            name = name.replace('_001.png', '_2_001.png')
        d = meta['frames'].get(name)
        if not d:
            continue
        img = cut(sheet_img, d)
        sx, sy = pair(v.get('scale', '{1,1}'))
        fx, fy = pair(v.get('flipped', '{0,0}'))
        w, h = max(1, int(round(img.width * abs(sx)))), max(1, int(round(img.height * abs(sy))))
        img = img.resize((w, h), Image.BICUBIC)
        if fx:
            img = img.transpose(Image.FLIP_LEFT_RIGHT)
        if fy:
            img = img.transpose(Image.FLIP_TOP_BOTTOM)
        # cocos turns clockwise, PIL the other way
        img = img.rotate(-float(v.get('rotation', 0)), resample=Image.BICUBIC, expand=True)
        px, py = pair(v['position'])
        cx, cy = ROBOT_SIZE / 2 + px, ROBOT_SIZE / 2 - py
        canvas.alpha_composite(img, (int(round(cx - img.width / 2)), int(round(cy - img.height / 2))))
    return canvas


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
        if n.startswith('psp_robot_'):
            # psp_robot_<robot>_<layer>.png: put together from its parts
            robot, layer = n[len('psp_robot_'):-4].rsplit('_', 1)
            robot_layer(res, robot, int(layer)).save(os.path.join(work, n))
            t3s.append(n)
            continue
        png, d = frames[n]
        if png not in sheets:
            sheets[png] = Image.open(png).convert('RGBA')
        cut(sheets[png], d).save(os.path.join(work, n))
        t3s.append(n)

    t3s_path = os.path.join(work, 'sprites_v2.t3s')
    open(t3s_path, 'w').write('\n'.join(t3s) + '\n')
    subprocess.check_call([sys.executable, os.path.join(HERE, 'mkatlas.py'), t3s_path, out])


if __name__ == '__main__':
    main()
