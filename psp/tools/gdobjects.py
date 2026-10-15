#!/usr/bin/env python3
"""
gdobjects.py - reads the game's object table (source/objects.c) with names

The table holds texture indices: < 819 into gfx/sprites.t3s, from 819 into
gfx/portals/portals.t3s; glow frames into gfx/glow/glow.t3s. GD's own names
for each object id come from the decompilation's ObjectToolbox.cpp. Shared by
the tools that add the 2.0 objects (gen2objects.py).
"""
import os
import re

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..')
SPRITESHEET2_START = 819


def t3s_names(path):
    lines = [l.strip() for l in open(os.path.join(ROOT, path)) if l.strip()]
    return [os.path.basename(l) for l in lines[1:]]


def texture_names():
    return t3s_names('gfx/sprites.t3s') + t3s_names('gfx/portals/portals.t3s')


def glow_names():
    return t3s_names('gfx/glow/glow.t3s')


def toolbox(path=None):
    path = path or os.path.join(ROOT, 'gd211-decomp/GD/code/src/ObjectToolbox.cpp')
    out = {}
    for m in re.finditer(r'ADD_TO_TOOLBOX\((\d+),\s*"([^"]+)"\)', open(path).read()):
        out.setdefault(int(m.group(1)), m.group(2))
    return out


def _split_fields(s):
    return [f.strip() for f in s.split(',')]


def parse_objects_c(path=None):
    """{id: dict} for the table's entries, children and hitboxes resolved."""
    src = open(path or os.path.join(ROOT, 'source/objects.c')).read()

    hitboxes = {}
    for m in re.finditer(r'const ObjectHitbox (\w+)\[\] = \{(.*?)\};', src, re.S):
        rows = re.findall(r'\{([^{}]+)\}', m.group(2))
        hitboxes[m.group(1)] = [_split_fields(r) for r in rows]

    children = {}
    for m in re.finditer(r'const ChildSprite (\w+)\[\] = \{(.*?)\};', src, re.S):
        rows = re.findall(r'\{([^{}]+)\}', m.group(2))
        children[m.group(1)] = [_split_fields(r) for r in rows]

    table = src[src.index('game_objects[GAME_OBJECT_COUNT]'):]
    table = table[table.index('{') + 1:]
    objs = {}
    for i, row in enumerate(re.findall(r'\{([^{}]+)\}', table.split('};')[0])):
        f = _split_fields(row)
        objs[i] = {
            'texture': int(f[0]), 'glow': int(f[1]), 'x': float(f[2].rstrip('f')), 'y': float(f[3].rstrip('f')),
            'z_layer': int(f[4]), 'z_order': int(f[5]), 'base_color': int(f[6]),
            'color_type': f[7], 'swap': int(f[8]), 'opacity': float(f[9].rstrip('f')),
            'children': children.get(f[11], []), 'children_name': f[11],
            'hitbox': hitboxes.get(f[12]), 'hitbox_name': f[12],
        }
    return objs


if __name__ == '__main__':
    import sys
    tex = texture_names()
    glow = glow_names()
    tb = toolbox()
    objs = parse_objects_c()
    print(len(objs), 'objects,', len(tex), 'textures,', len(glow), 'glow frames,', len(tb), 'toolbox ids')
    for i in (int(a) for a in sys.argv[1:]):
        o = objs.get(i)
        name = tb.get(i)
        if not o:
            print(i, name, 'not in the table')
            continue
        tn = tex[o['texture']] if 0 <= o['texture'] < len(tex) else None
        gn = glow[o['glow']] if 0 <= o['glow'] < len(glow) else None
        print(f"{i} {name}: tex {o['texture']} {tn} glow {gn} z {o['z_layer']}/{o['z_order']} "
              f"col {o['base_color']} {o['color_type']} swap {o['swap']} hitbox {o['hitbox']}")
        for c in o['children']:
            ct = int(c[0])
            print('   child', ct, tex[ct] if 0 <= ct < len(tex) else None, c[1:])
