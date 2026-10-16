#!/usr/bin/env python3
"""
gen2objects.py - the objects GD 2.0 added (ids 746 and up), for the PSP build

The game's table (source/objects.c) stops at id 745. The rest comes from
OpenGD's object data (Content/Custom/object.json: sprite, details, layer,
colors, object type) and hitboxes (Source/LongData.cpp), translated to the
table's format the way the existing objects show it:

  color_channel base/detail/black/none -> COLOR_TYPE_BASE/DETAIL(+swap)/BLACK/WHITE
  details at (x, y) from the parent's corner -> from its center
  hitbox {h, w, left, bottom} -> {center x, center y, w, h}
  object type 0 solid, 2 hazard, 25 slope, 21 breakable, 7 decoration, else special

Writes, in source/ (committed: building needs neither OpenGD nor GD):
  objects_v2_defs.inc   the new objects' details and hitboxes
  objects_v2_rows.inc   their rows of game_objects[]
  objects_v2.h          the new count, where their textures start
and psp/gfx2/sprites_v2.list: the sprites they use that the game's sheets
don't have, in texture order from V2_TEXTURE_START. Their pictures come
from each user's own copy of GD when building (mkv2sheet.py).

Usage: gen2objects.py <GD Resources folder> <object.json> <LongData.cpp> [last id]
"""
import json
import math
import os
import plistlib
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import gdobjects as g

FIRST_ID = 746
V2_TEXTURE_START = 2000

SHEETS = ['GJ_GameSheet', 'GJ_GameSheet02', 'GJ_GameSheet03', 'GJ_GameSheet04', 'GJ_GameSheetGlow', 'FireSheet_01']

COLOR_TYPES = {'base': ('COLOR_TYPE_BASE', 0), 'detail': ('COLOR_TYPE_DETAIL', 1),
               'black': ('COLOR_TYPE_BLACK', 0), None: ('COLOR_TYPE_WHITE', 0)}

# gameplay objects OpenGD has no data for: like a 1.x object (its row, hitbox
# and all), with their own sprite. The gameplay code handles them by id
LIKE = {
    747: 10,    # teleport portal (in): a portal
    749: 10,    # teleport portal (out)
    1022: 84,   # green orb: the blue orb
}

# objects the game's table has but without a sprite: a row like a 1.x
# object's, as a V2_ROW_<id> define objects.c uses. id -> (like, sprite)
PATCH = {
    745: (13, 'portal_17_front_001.png'),   # robot portal: like the ship's
}

# decorations OpenGD has no data for (monsters, sludge, the key): drawn
# still, with the editor's sprite. id -> (z layer, z order, main channel)
DECO = {
    918: (3, 0, 1010),      # monster 1 (animated in GD): black
    919: (3, 0, 1010),      # black sludge
    1327: (3, 0, 1010),     # monster 2
    1328: (3, 0, 1010),     # monster 3
    1275: (3, 0, 1011),     # key
}


def stem(name):
    return name[:-8] if name.endswith('_001.png') else name.rsplit('.', 1)[0]


def size_of(frame):
    w, h = frame['spriteSourceSize'].strip('{}').split(',')
    return int(w), int(h)


def load_hitboxes(path):
    src = open(path).read()
    part = src[src.index('_pHitboxes'):src.index('_pHitboxRadius')]
    hb = {int(m.group(1)): tuple(float(x) for x in m.group(2).split(','))
          for m in re.finditer(r'\{\s*(\d+)\s*,\s*\{([^}]*)\}\s*\}', part)}
    rad = src[src.index('_pHitboxRadius'):]
    rad = rad[:rad.index('};')]
    radius = {int(m.group(1)): float(m.group(2))
              for m in re.finditer(r'\{\s*(\d+)\s*,\s*([-\d.]+)\s*\}', rad)}
    return hb, radius


def main():
    res, json_path, longdata = sys.argv[1:4]
    last_id = int(sys.argv[4]) if len(sys.argv) > 4 else 1329

    frames = {}
    for s in SHEETS:
        p = os.path.join(res, s + '.plist')
        if os.path.exists(p):
            for n, d in plistlib.load(open(p, 'rb'))['frames'].items():
                frames.setdefault(n, d)

    data = json.load(open(json_path))
    hitboxes, radii = load_hitboxes(longdata)

    tex = g.texture_names()
    glow_list = g.glow_names()
    tb = g.toolbox()
    objs = g.parse_objects_c()

    tex_index = {}
    for i, n in enumerate(tex):
        tex_index.setdefault(n, i)
    glow_index = {n: i for i, n in enumerate(glow_list)}
    new_sprites = []

    def texture_for(name):
        if name in tex_index:
            return tex_index[name]
        if name not in frames:
            return -1
        tex_index[name] = V2_TEXTURE_START + len(new_sprites)
        new_sprites.append(name)
        return tex_index[name]

    def glow_for(name):
        gname = stem(name) + '_glow_001.png'
        if gname in glow_index:
            return glow_index[gname]
        if gname not in frames:
            return -1
        return texture_for(gname)

    def flatten(children, pw, ph, ox=0.0, oy=0.0, prot=0.0, psx=1.0, psy=1.0):
        """Details (and theirs) as offsets from the object's center."""
        out = []
        for c in children or []:
            cw = c.get('content_x', pw)
            ch = c.get('content_y', ph)
            # from the parent's corner to its center, then through the parent's transform
            lx = (c['x'] - cw / 2) * psx
            ly = (c['y'] - ch / 2) * psy
            r = math.radians(prot)
            x = ox + lx * math.cos(r) + ly * math.sin(r)
            y = oy - lx * math.sin(r) + ly * math.cos(r)
            sx = c['scale_x'] * psx
            sy = c['scale_y'] * psy
            rot = prot + c['rot']
            out.append((c, x, y, sx, sy, rot))
            if c.get('children'):
                f = frames.get(c['texture_name'])
                w, h = size_of(f) if f else (cw, ch)
                out += flatten(c['children'], w, h, x, y, rot, sx, sy)
        return out

    defs = ['/* generated by psp/tools/gen2objects.py from OpenGD\'s object data: the objects GD 2.0 added */', '']
    rows = []
    special = []
    for oid in list(PATCH) + list(range(FIRST_ID, last_id + 1)):
        if oid in PATCH:
            j = None
            like = objs.get(PATCH[oid][0])
            name = PATCH[oid][1]
        else:
            j = data.get(str(oid))
            like = objs.get(LIKE.get(oid))
            name = j['texture_name'] if j else tb.get(oid)

        if not j and not like and oid in DECO:
            z, zo, chan = DECO[oid]
            j = {'default_z_layer': z, 'default_z_order': zo, 'default_primary_channel': chan,
                        'color_channel': 'base', 'object_type': 7, 'texture_name': name}

        if not name or (not j and not like) or name not in frames:
            what = 'trigger/none' if not j or j.get('object_type') == 25 else name
            rows.append('    { -1, -1, 0.00, 0.00, 0, 0, 0, COLOR_TYPE_WHITE, 0, 1.00, 0, NULL, NULL }, /* %d %s */'
                        % (oid, name or '-'))
            continue

        w, h = size_of(frames[name])

        if like:
            z_layer, z_order, base_color = like['z_layer'], like['z_order'], like['base_color']
            color_type, swap = like['color_type'], like['swap']
            otype = None
        else:
            z_layer, z_order = j['default_z_layer'], j['default_z_order']
            base_color = j['default_primary_channel']
            color_type, swap = COLOR_TYPES.get(j.get('color_channel'), COLOR_TYPES[None])
            otype = j['object_type']

        # hitbox
        hitbox = None
        if like:
            hitbox = [tuple(like['hitbox'][0])] if like['hitbox'] else None
        elif otype in (0, 2, 21, 25) or (otype != 7 and oid in hitboxes):
            hb = hitboxes.get(oid, (h, w, -w / 2, -h / 2))
            hh, hw, left, bottom = hb
            kind = {0: 'HITBOX_SOLID', 21: 'HITBOX_SOLID', 25: 'HITBOX_SOLID', 2: 'HITBOX_HAZARD'}.get(otype, 'HITBOX_SPECIAL')
            shape = 'COLLISION_SLOPE' if otype == 25 else ('COLLISION_CIRCLE' if oid in radii else 'COLLISION_BOX')
            if oid in radii:
                hw = hh = radii[oid] * 2
                left, bottom = -hw / 2, -hh / 2
            hitbox = [(kind, '%.2f' % (left + hw / 2), '%.2f' % (bottom + hh / 2), '%.2f' % hw, '%.2f' % hh, shape)]
            if kind == 'HITBOX_SPECIAL':
                special.append('%d %s type %s' % (oid, name, otype))

        hitbox_ref = 'NULL'
        if hitbox:
            hitbox_ref = 'hitbox_v2_%d' % oid
            defs.append('const ObjectHitbox %s[] = {' % hitbox_ref)
            for hb in hitbox:
                defs.append('    { %s, %s, %s, %s, %s, %s },' % tuple(hb))
            defs.append('};')

        # details
        children = []
        if like:
            # the 1.x object's details, as this object's sprites when GD has
            # them (portal_01_back -> portal_15_back)
            old = '_'.join(stem(tex[like['texture']]).split('_')[:2])
            new = '_'.join(stem(name).split('_')[:2])
            for c in like['children']:
                c = list(c)
                ct = int(c[0])
                if 0 <= ct < len(tex):
                    renamed = tex[ct].replace(old, new, 1)
                    if renamed != tex[ct] and renamed in frames:
                        c[0] = str(texture_for(renamed))
                children.append(c)
        elif j:
            for c, x, y, sx, sy, rot in flatten(j.get('children'), w, h):
                ci = texture_for(c['texture_name'])
                if ci < 0:
                    continue
                ctype = COLOR_TYPES.get(c.get('color_channel'), COLOR_TYPES[None])[0]
                children.append([str(ci), '%.2f' % x, '%.2f' % y, '%.2f' % abs(sx), '%.2f' % abs(sy),
                                 str(int(bool(c.get('flip_x')) ^ (sx < 0))), str(int(bool(c.get('flip_y')) ^ (sy < 0))),
                                 str(int(c.get('z', -1))), '0', '%.2f' % rot, ctype, '1.00'])

        children_ref = 'NULL'
        if children:
            children_ref = 'children_v2_%d' % oid
            defs.append('const ChildSprite %s[] = {' % children_ref)
            for c in children:
                defs.append('    { %s },' % ', '.join(c))
            defs.append('};')
        if hitbox or children:
            defs.append('')

        row = '{ %d, %d, 0.00, 0.00, %d, %d, %d, %s, %d, 1.00, %d, %s, %s }' % (
            texture_for(name), glow_for(name), z_layer, z_order, base_color,
            color_type, swap, len(children), children_ref, hitbox_ref)
        if oid in PATCH:
            defs += ['#define V2_ROW_%d %s /* %s */' % (oid, row, name), '']
        else:
            rows.append('    %s, /* %d %s */' % (row, oid, name))

    # the robot (the player's), put together from its parts by mkv2sheet.py
    robot_tex = V2_TEXTURE_START + len(new_sprites)
    new_sprites += ['psp_robot_robot_01_1.png', 'psp_robot_robot_01_2.png']

    src = os.path.join(g.ROOT, 'source')
    open(os.path.join(src, 'objects_v2_defs.inc'), 'w').write('\n'.join(defs) + '\n')
    open(os.path.join(src, 'objects_v2_rows.inc'), 'w').write(
        '/* generated by psp/tools/gen2objects.py: game_objects[] from id %d */\n' % FIRST_ID
        + '\n'.join(rows) + '\n')
    open(os.path.join(src, 'objects_v2.h'), 'w').write(
        '/* generated by psp/tools/gen2objects.py */\n#pragma once\n'
        '#define GAME_OBJECT_COUNT_V2 %d\n#define V2_TEXTURE_START %d\n#define V2_TEXTURE_COUNT %d\n'
        '#define V2_TEX_ROBOT %d /* main color, then the second color */\n'
        % (last_id + 1, V2_TEXTURE_START, len(new_sprites), robot_tex))
    os.makedirs(os.path.join(g.ROOT, 'psp/gfx2'), exist_ok=True)
    open(os.path.join(g.ROOT, 'psp/gfx2/sprites_v2.list'), 'w').write('\n'.join(new_sprites) + '\n')
    print('%d ids, %d new sprites; special objects (gameplay code needed):' % (last_id + 1 - FIRST_ID, len(new_sprites)))
    print('\n'.join(special))


if __name__ == '__main__':
    main()
