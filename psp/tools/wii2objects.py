#!/usr/bin/env python3
"""
wii2objects.py - the GD 2.0 objects' table from wiidash's (AleFunky's Wii port)

wiidash (github.com/AleFunky/wiidash) defines every object as layers drawn in
order (source/object_definition.c): a whole sprite each (data/*/*.png, at
twice GD's standard size), an offset, a z layer offset, a color type (main,
detail, unmodified, glow) and the channel it takes by default. gen2objects.py
put the same objects together out of OpenGD's parts, and got many wrong
(block009d out of block009c pieces, ...); this one rebuilds the table's rows
from wiidash's layers:

  the first layer at the object's own z layer   -> the row's sprite
  the layers before it / after it               -> details behind / in front
  a *_glow_* layer                              -> the row's glow
  main / detail / unmodified / glow             -> COLOR_TYPE_BASE / DETAIL / WHITE / GLOW
  a main layer's channel                        -> base_color (GD's numbers)
  a detail layer's channel                      -> detail_color (else 1)
  y offset (down, on wiidash's screen)          -> up, as this table has it

The hitboxes stay the ones the table already has (GD 2.113's data, through
OpenGD); an object that has none takes wiidash's. Objects wiidash draws with
no layers (triggers, monsters it animates, ...) keep their current rows.

The sprites go in psp/gfx2/sprites_v2.list as wii:<name> (mkv2sheet.py takes
them from wiidash's PNGs, at half size); the kept rows' sprites stay names
from GD's own sheets.

Usage: wii2objects.py <wiidash folder>
"""
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, '..', '..'))
SRC = os.path.join(ROOT, 'source')
LIST = os.path.join(ROOT, 'psp', 'gfx2', 'sprites_v2.list')

FIRST_ID, LAST_ID = 746, 1329
V2_TEXTURE_START = 2000
PATCH = (745,)          # in the game's table, with a 2.0 sprite (V2_ROW_745)
SKIP = {914}            # the text object: wiidash's layer is a placeholder

CHANNELS = {'BG': 1000, 'GROUND': 1001, 'LINE': 1002, 'THREEDL': 1003, 'OBJ': 1004,
            'P1': 1005, 'P2': 1006, 'LBG': 1007, 'OBJ_BLENDING': 1004, 'G2': 1009,
            'BLACK': 1010, 'WHITE': 1011, 'LIGHTER': 1012, 'LBG_NO_LERP': 1007}
HITBOX = {'HITBOX_SOLID': 'HITBOX_SOLID', 'HITBOX_SPIKE': 'HITBOX_HAZARD',
          'HITBOX_SPECIAL': 'HITBOX_SPECIAL', 'HITBOX_BREAKABLE_BLOCK': 'HITBOX_SOLID'}


# ---- wiidash's C ---------------------------------------------------------

def strip_comments(s):
    s = re.sub(r'/\*.*?\*/', '', s, flags=re.S)
    return re.sub(r'//[^\n]*', '', s)


def enum_values(h):
    ids = {}
    for body in re.findall(r'enum\s*\w*\s*\{(.*?)\}', h, re.S):
        val = -1
        for item in body.split(','):
            item = item.strip()
            if not item:
                continue
            if '=' in item:
                name, v = (x.strip() for x in item.split('=', 1))
                try:
                    val = int(v, 0)
                except ValueError:
                    val = ids.get(v, val + 1)
            else:
                name, val = item, val + 1
            if re.match(r'^\w+$', name):
                ids.setdefault(name, val)
    return ids


def split_top(s):
    out, depth, cur = [], 0, ''
    for ch in s:
        if ch in '{(':
            depth += 1
        elif ch in '})':
            depth -= 1
        if ch == ',' and depth == 0:
            out.append(cur)
            cur = ''
        else:
            cur += ch
    out.append(cur)
    return [x.strip() for x in out if x.strip()]


def inside(s, start=0):
    depth = 0
    for i in range(start, len(s)):
        if s[i] == '{':
            depth += 1
        elif s[i] == '}':
            depth -= 1
            if depth == 0:
                return s[start + 1:i]
    raise ValueError('unbalanced braces')


def fields(entry):
    d = {}
    for f in split_top(entry):
        m = re.match(r'\.(\w+)\s*=\s*(.*)$', f, re.S)
        if m:
            d[m.group(1)] = m.group(2).strip()
    return d


def wiidash_table(wii):
    h = strip_comments(open(os.path.join(wii, 'source', 'objects.h')).read())
    c = strip_comments(open(os.path.join(wii, 'source', 'object_definition.c')).read())
    ids = enum_values(h)
    named_hb = {}
    for m in re.finditer(r'const ObjectHitbox (\w+)\s*=\s*\{', c):
        named_hb[m.group(1)] = fields(inside(c, m.end() - 1))
    m = re.search(r'const ObjectDefinition objects\[[^\]]*\]\s*=\s*\{', c)
    table, idx = {}, -1
    for item in split_top(inside(c, m.end() - 1)):
        mm = re.match(r'\[(\w+)\]\s*=\s*(.*)$', item, re.S)
        if mm:
            idx = ids[mm.group(1)] if mm.group(1) in ids else int(mm.group(1), 0)
            item = mm.group(2).strip()
        else:
            idx += 1
        if not item.startswith('{'):
            table[idx] = {'layers': [], 'hitbox': {}}   # trigger, unknown, ...
            continue
        f = fields(inside(item))
        layers = []
        if 'layers' in f and f['layers'].startswith('{'):
            for l in split_top(inside(f['layers'])):
                if l.startswith('{'):
                    layers.append(fields(inside(l)))
        hb = f.get('hitbox', '')
        f['hitbox'] = fields(inside(hb)) if hb.startswith('{') else named_hb.get(hb, {})
        f['layers'] = [l for l in layers if l.get('texture')]
        table[idx] = f
    return table, ids


# ---- the table as it is ----------------------------------------------------

def current():
    rows_src = open(os.path.join(SRC, 'objects_v2_rows.inc')).read()
    defs_src = open(os.path.join(SRC, 'objects_v2_defs.inc')).read()
    names = [l.strip() for l in open(LIST) if l.strip()]
    rows = {}
    for m in re.finditer(r'^\s*(\{.*\}),\s*/\*\s*(\d+)\s+(.*?)\s*\*/', rows_src, re.M):
        rows[int(m.group(2))] = (m.group(1), m.group(3))
    m = re.search(r'#define V2_ROW_745 (\{.*\}) /\* (.*?) \*/', defs_src)
    rows[745] = (m.group(1), m.group(2))
    blocks = {}
    for m in re.finditer(r'const (ObjectHitbox|ChildSprite) (hitbox|children)_v2_(\d+)\[\] = \{(.*?)\};', defs_src, re.S):
        blocks[(m.group(2), int(m.group(3)))] = m.group(4).strip('\n')
    return rows, blocks, names


# ---- the new one -------------------------------------------------------------

def main():
    wii = sys.argv[1]
    table, ids = wiidash_table(wii)
    rows, blocks, old_names = current()

    sprites = []
    index = {}

    def sprite(name):
        if name not in index:
            index[name] = V2_TEXTURE_START + len(sprites)
            sprites.append(name)
        return index[name]

    def remap(text):
        """A kept row or block: its 2.0 sprites at their new places."""
        def one(m):
            v = int(m.group(1))
            if v >= V2_TEXTURE_START and v - V2_TEXTURE_START < len(old_names):
                return m.group(0).replace(m.group(1), str(sprite(old_names[v - V2_TEXTURE_START])), 1)
            return m.group(0)
        # the texture is the first field, the glow the second (rows only)
        text = re.sub(r'\{ (\d+),', one, text)
        text = re.sub(r'^(\{ -?\d+, )(\d+),', lambda m: m.group(1) + (
            str(sprite(old_names[int(m.group(2)) - V2_TEXTURE_START]))
            if int(m.group(2)) >= V2_TEXTURE_START else m.group(2)) + ',', text)
        return text

    defs = ['/* generated by psp/tools/wii2objects.py from wiidash\'s object definitions'
            ' (the rest from gen2objects.py\'s table): the objects GD 2.0 added */', '']
    out_rows = []
    from_wii = kept = 0

    for oid in list(PATCH) + list(range(FIRST_ID, LAST_ID + 1)):
        w = table.get(oid)
        old = rows.get(oid)
        if not w or not w['layers'] or oid in SKIP:
            # as it was
            if not old:
                out_rows.append('    { -1, -1, 0.00, 0.00, 0, 0, 0, COLOR_TYPE_WHITE, 0, 1.00, 0, NULL, NULL }, /* %d - */' % oid)
                continue
            row = remap(old[0])
            for kind in ('hitbox', 'children'):
                b = blocks.get((kind, oid))
                if b is not None:
                    typ = 'ObjectHitbox' if kind == 'hitbox' else 'ChildSprite'
                    defs += ['const %s %s_v2_%d[] = {' % (typ, kind, oid), remap(b), '};']
            defs.append('')
            kept += 1
        else:
            row = wii_row(oid, w, blocks, sprite, defs)
            from_wii += 1
            old = (None, w['layers'][0]['texture'][:-4] + '.png')
        if oid in PATCH:
            defs += ['#define V2_ROW_%d %s /* %s */' % (oid, row, old[1]), '']
        else:
            out_rows.append('    %s, /* %d %s */' % (row, oid, old[1]))

    # the player's robot (mkv2sheet.py puts it together), last as before
    robot = sprite('psp_robot_robot_01_1.png')
    sprite('psp_robot_robot_01_2.png')

    open(os.path.join(SRC, 'objects_v2_defs.inc'), 'w').write('\n'.join(defs) + '\n')
    open(os.path.join(SRC, 'objects_v2_rows.inc'), 'w').write(
        '/* generated by psp/tools/wii2objects.py: game_objects[] from id %d */\n' % FIRST_ID
        + '\n'.join(out_rows) + '\n')
    open(os.path.join(SRC, 'objects_v2.h'), 'w').write(
        '/* generated by psp/tools/wii2objects.py */\n#pragma once\n'
        '#define GAME_OBJECT_COUNT_V2 %d\n#define V2_TEXTURE_START %d\n#define V2_TEXTURE_COUNT %d\n'
        '#define V2_TEX_ROBOT %d /* main color, then the second color */\n'
        % (LAST_ID + 1, V2_TEXTURE_START, len(sprites), robot))
    open(LIST, 'w').write('\n'.join(sprites) + '\n')
    print('%d objects from wiidash, %d kept, %d sprites (%d from wiidash)'
          % (from_wii, kept, len(sprites), sum(1 for s in sprites if s.startswith('wii:'))))


def channel(v):
    v = (v or 'OBJ').strip()
    return CHANNELS[v] if v in CHANNELS else int(v, 0)


def num(v, default=0.0):
    try:
        return float(str(v).rstrip('f'))
    except (TypeError, ValueError):
        return default


def wii_row(oid, w, blocks, sprite, defs):
    layers = w['layers']
    names = [l['texture'][:-4] + '.png' for l in layers]
    glow = [i for i, n in enumerate(names) if '_glow_' in n or layers[i].get('color_type') == 'COLOR_GLOW']
    body = [i for i in range(len(layers)) if i not in glow]
    if not body:
        body, glow = glow, []
    main = next((i for i in body if int(num(layers[i].get('zlayer_offset'))) == 0), body[0])

    def color(l):
        ct = l.get('color_type', 'COLOR_MAIN')
        return {'COLOR_DETAIL': 'COLOR_TYPE_DETAIL', 'COLOR_UNMOD': 'COLOR_TYPE_WHITE',
                'COLOR_GLOW': 'COLOR_TYPE_GLOW'}.get(ct, 'COLOR_TYPE_BASE')

    base_color = detail_color = 0
    for i in body:
        c = color(layers[i])
        if c == 'COLOR_TYPE_BASE' and not base_color:
            base_color = channel(layers[i].get('col_channel'))
        if c == 'COLOR_TYPE_DETAIL' and not detail_color:
            detail_color = channel(layers[i].get('col_channel'))

    children = []
    before = [i for i in body if i < main]
    after = [i for i in body if i > main]
    for k, i in enumerate(before + after):
        l = layers[i]
        z = -(len(before) - k) if i < main else 2 + (k - len(before))
        children.append('    { %d, %.2f, %.2f, 1.00, 1.00, 0, 0, %d, %d, 0.00, %s, 1.00 },' % (
            sprite('wii:' + names[i]), num(l.get('x_offset')), -num(l.get('y_offset')),
            z, int(num(l.get('zlayer_offset'))), color(l)))

    hitbox_ref = 'NULL'
    if ('hitbox', oid) in blocks:
        defs += ['const ObjectHitbox hitbox_v2_%d[] = {' % oid, blocks[('hitbox', oid)], '};']
        hitbox_ref = 'hitbox_v2_%d' % oid
    else:
        hb = w['hitbox']
        kind = HITBOX.get(hb.get('type'))
        if kind:
            r = num(hb.get('radius'))
            circle = hb.get('is_circular', '').upper() in ('TRUE', '1', 'true') and r > 0
            ww, hh = (2 * r, 2 * r) if circle else (num(hb.get('width')), num(hb.get('height')))
            shape = 'COLLISION_SLOPE' if w.get('is_slope', '').upper() in ('TRUE', '1') else (
                'COLLISION_CIRCLE' if circle else 'COLLISION_BOX')
            if ww > 0 and hh > 0:
                defs += ['const ObjectHitbox hitbox_v2_%d[] = {' % oid,
                         '    { %s, %.2f, %.2f, %.2f, %.2f, %s },' % (
                             kind, num(hb.get('x_off')), num(hb.get('y_off')), ww, hh, shape), '};']
                hitbox_ref = 'hitbox_v2_%d' % oid

    children_ref = 'NULL'
    if children:
        children_ref = 'children_v2_%d' % oid
        defs += ['const ChildSprite %s[] = {' % children_ref] + children + ['};']
    defs.append('')

    z_layer = int(num(w.get('def_zlayer'), 0)) if re.match(r'^-?\d', w.get('def_zlayer', '')) else {
        'LAYER_B4': -3, 'LAYER_B3': -1, 'LAYER_B2': 1, 'LAYER_B1': 3, 'LAYER_T1': 5,
        'LAYER_T2': 7, 'LAYER_T3': 9, 'LAYER_T4': 11}.get(w.get('def_zlayer'), 3)
    z_order = int(num(w.get('def_zorder'), 0))
    glow_ref = sprite('wii:' + names[glow[0]]) if glow else -1
    return '{ %d, %d, %.2f, %.2f, %d, %d, %d, %s, 0, 1.00, %d, %s, %s, %d }' % (
        sprite('wii:' + names[main]), glow_ref, num(layers[main].get('x_offset')), -num(layers[main].get('y_offset')),
        z_layer, z_order, base_color, color(layers[main]), len(children), children_ref, hitbox_ref,
        detail_color if detail_color != 1 else 0)


if __name__ == '__main__':
    main()
