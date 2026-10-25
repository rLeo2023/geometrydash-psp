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
  main / detail / glow                          -> COLOR_TYPE_BASE / DETAIL / GLOW
  unmodified (a fixed black or white)           -> COLOR_TYPE_BLACK / WHITE
  a main layer's channel                        -> base_color (GD's numbers)
  a detail layer's channel                      -> detail_color (else 1)
  y offset (down, on wiidash's screen)          -> up, as this table has it

The hitboxes stay the ones gen2objects.py's table has (GD 2.113's data,
through OpenGD; kept in opengd_base/); an object that has none takes
wiidash's. Objects wiidash draws with no layers (triggers, ...) keep that
table's rows.

The sprites go in psp/gfx2/sprites_v2.list as wii:<name> (mkv2sheet.py takes
them from wiidash's PNGs, at half size); a kept row's sprite would stay a
name from GD's own sheets.

Animations (source/objects_v2_anim.inc, drawn by graphics.c) as wiidash's
animation.c has them: fire, water and loading circles swap sprites; the
monsters, the sludge and the player's robot move their parts (GD's plists).

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

# the gamemode portals' badge (GD 2.2's half disc with the icon): wiidash draws
# the robot portal with only its two halves, the badge sprites are in the
# portals sheet (gfx/portals/portals.t3s), where the rows of GD's own table
# take theirs. id -> (the half disc, the icon)
PORTAL_BADGE = {745: ('portal_14_extra_001.png', 'portal_14_extra_2_001.png')}
PORTAL_SHEET_START = 819


def portal_sheet_texture(name):
    lines = [l.strip() for l in open(os.path.join(ROOT, 'gfx', 'portals', 'portals.t3s'))
             if l.strip()]
    return PORTAL_SHEET_START + lines.index(name) - 1  # the first line is the .t3s options
# wiidash's hitbox over gen2objects' (which had a portal's): the teleports'
# sits 12 to the front, 90 long. Levels count on it (Deadlocked puts a
# gravity portal on the way in, to be touched first)
WII_HITBOX = {747, 749}

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

BASE = os.path.join(HERE, 'opengd_base')  # gen2objects.py's table, as it made it


def current():
    rows_src = open(os.path.join(BASE, 'objects_v2_rows.inc')).read()
    defs_src = open(os.path.join(BASE, 'objects_v2_defs.inc')).read()
    names = [l.strip() for l in open(os.path.join(BASE, 'sprites_v2.list')) if l.strip()]
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

    skel = {}
    for oid in list(PATCH) + list(range(FIRST_ID, LAST_ID + 1)):
        w = table.get(oid)
        old = rows.get(oid)
        if oid in SKEL_ANIMS:
            row, skel[oid] = skel_row(oid, SKEL_ANIMS[oid], wii, w or {}, blocks, sprite, defs)
            out_rows.append('    %s, /* %d %s (animated) */' % (row, oid, SKEL_ANIMS[oid][3][0][0]))
            from_wii += 1
            continue
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
            # the half disc's straight edge sits on the portal's centre and the
            # icon centres on it (the game's own rows have the same 12 and 10)
            badge = [(portal_sheet_texture(n), x, 0.0, z, 0)
                     for z, x, n in zip((2, 3), (12.0, 10.0), PORTAL_BADGE.get(oid, ()))]
            row = wii_row(oid, w, blocks, sprite, defs, extras=badge)
            from_wii += 1
            old = (None, w['layers'][0]['texture'][:-4] + '.png')
        if oid in PATCH:
            defs += ['#define V2_ROW_%d %s /* %s */' % (oid, row, old[1]), '']
        else:
            out_rows.append('    %s, /* %d %s */' % (row, oid, old[1]))

    anim = anim_source(sprite, None, skel) + robot_source(wii, sprite) + spin_source(wii, ids)
    open(os.path.join(SRC, 'objects_v2_anim.inc'), 'w').write(anim)

    open(os.path.join(SRC, 'objects_v2_defs.inc'), 'w').write('\n'.join(defs) + '\n')
    open(os.path.join(SRC, 'objects_v2_rows.inc'), 'w').write(
        '/* generated by psp/tools/wii2objects.py: game_objects[] from id %d */\n' % FIRST_ID
        + '\n'.join(out_rows) + '\n')
    open(os.path.join(SRC, 'objects_v2.h'), 'w').write(
        '/* generated by psp/tools/wii2objects.py */\n#pragma once\n'
        '#define GAME_OBJECT_COUNT_V2 %d\n#define V2_TEXTURE_START %d\n#define V2_TEXTURE_COUNT %d\n'
        % (LAST_ID + 1, V2_TEXTURE_START, len(sprites)))
    open(LIST, 'w').write('\n'.join(sprites) + '\n')
    print('%d objects from wiidash, %d kept, %d sprites (%d from wiidash)'
          % (from_wii, kept, len(sprites), sum(1 for s in sprites if s.startswith('wii:'))))


SLOTS = {}  # id -> wiidash's layer for the row's sprite, then each detail's


# ---- animations (wiidash's animation.c) ------------------------------------

def fire(n, count):
    return [('Fire_%02d_2_looped_%03d.png' % (n, k), 'Fire_%02d_looped_%03d.png' % (n, k)) for k in range(1, count + 1)]


def wave(n, seq):
    return [('d_animWave_%02d_%03d.png' % (n, k),) for k in seq]


def loading(n, count):
    return [('d_animLoading_%02d_color_%03d.png' % (n, k), 'd_animLoading_%02d_%03d.png' % (n, k)) for k in range(1, count + 1)]


# id -> frames (each one sprite per wiidash layer), fps, each layer's scale,
# the frames drawn flipped, whether each object runs its own clock
FRAME_ANIMS = {
    920: (fire(3, 9), 12, (2, 1), (), True),
    921: (fire(4, 11), 12, (2, 1), (), True),
    923: (fire(1, 9), 12, (2, 1), (), True),
    924: (fire(2, 9), 12, (2, 1), (), True),
    1050: (wave(1, [1, 2, 3, 4, 5, 6, 7, 8, 9, 8, 7]), 20, (1,), (9, 10), False),
    1051: (wave(2, [1, 2, 3, 4, 5, 6, 5, 4, 3, 2]), 20, (1,), (6, 7, 8, 9), False),
    1052: (wave(3, [1, 2, 3, 4, 5, 6, 5, 4, 3, 2]), 20, (1,), (6, 7, 8, 9), False),
    1053: (loading(1, 6), 20, (1, 1), (), False),
    1054: (loading(2, 7), 20, (1, 1), (), False),
}

# id -> plist, animation, fps, parts drawn in order: (sprite, color type,
# default channel, plist part), its hitbox. wiidash's speeds (38, 18, 20)
# looked too fast next to GD's: slower here
SKEL_ANIMS = {
    918: ('monster1.plist', 'GJBeast01_bite', 24, [
        ('GJBeast01_01_glow_001.png', 'COLOR_GLOW', 'LBG', 1),
        ('GJBeast01_02_glow_001.png', 'COLOR_GLOW', 'LBG', 0),
        ('GJBeast01_01_001.png', 'COLOR_MAIN', 'BLACK', 1),
        ('GJBeast01_03_001.png', 'COLOR_DETAIL', 'WHITE', 1),
        ('GJBeast01_02_001.png', 'COLOR_MAIN', 'BLACK', 0)]),
    1327: ('monster2.plist', 'GJBeast02_idle01', 12, [
        ('GJBeast02_01_glow_001.png', 'COLOR_GLOW', 'LBG', 0),
        ('GJBeast02_01_001.png', 'COLOR_MAIN', 'BLACK', 0),
        ('GJBeast02_02_001.png', 'COLOR_DETAIL', 'WHITE', 1)]),
    1328: ('monster3.plist', 'GJBeast03_idle01', 12, [
        ('GJBeast03_01_glow_001.png', 'COLOR_GLOW', 'LBG', 0),
        ('GJBeast03_01_001.png', 'COLOR_MAIN', 'BLACK', 0),
        ('GJBeast03_02_001.png', 'COLOR_DETAIL', 'WHITE', 1)]),
    # its parts are details with no main: they take the main channel
    919: ('black_sludge.plist', 'BlackSludge_loop', 12, [
        ('dA_blackSludge_01_001.png', 'COLOR_MAIN', 'BLACK', 0),
        ('dA_blackSludge_02_001.png', 'COLOR_MAIN', 'BLACK', 1),
        ('dA_blackSludge_02_001.png', 'COLOR_MAIN', 'BLACK', 2)]),
}


def plist_frames(path, anim):
    """The animation's frames, each its parts' (x, y, sx, sy, rot) in
    sprite_N order, as wiidash reads them."""
    import plistlib
    data = plistlib.load(open(path, 'rb'))['animationContainer']
    frames = []
    for name, frame in data.items():
        if re.sub(r'_\d+$', '', name.rsplit('.', 1)[0]) != anim:
            continue
        parts = []
        for _, sp in frame.items():
            def pair(k, d):
                v = sp.get(k)
                return tuple(float(x) for x in v.strip('{} ').split(',')) if v else d
            x, y = pair('position', (0.0, 0.0))
            sx, sy = pair('scale', (1.0, 1.0))
            parts.append((x, y, sx, sy, float(sp.get('rotation', 0) or 0)))
        frames.append(parts)
    return frames


def skel_row(oid, spec, wii, w, blocks, sprite, defs):
    plist, anim, fps, parts = spec
    layers = [{'texture': p[0][:-4] + '_png', 'color_type': p[1], 'col_channel': p[2], 'zlayer_offset': '0'}
              for p in parts]
    row = wii_row(oid, {'layers': layers, 'hitbox': w.get('hitbox', {}), 'def_zlayer': w.get('def_zlayer', 'LAYER_T1'),
                        'def_zorder': w.get('def_zorder', '2')}, blocks, sprite, defs, glow_details=True)
    frames = plist_frames(os.path.join(wii, 'data', 'extra', plist), anim)
    return row, (fps, [parts[s][3] for s in SLOTS[oid]], frames)


def spin_source(wii, ids):
    """The 2.0 objects that turn by themselves (saws, wheels, the green orb)
    and how fast: wiidash's get_rotation_speed()."""
    src = strip_comments(open(os.path.join(wii, 'source', 'objects.c')).read())
    m = re.search(r'float get_rotation_speed\([^)]*\)\s*\{', src)
    body = inside(src, m.end() - 1)
    speeds, pending = {}, []
    for tok in re.finditer(r'case\s+(\w+)\s*:|return\s+([-\d.]+)f?\s*;', body):
        if tok.group(1):
            pending.append(tok.group(1))
        else:
            for name in pending:
                if name in ids:
                    speeds[ids[name]] = float(tok.group(2))
            pending = []
    rows = ['    { %d, %d },' % (i, s) for i, s in sorted(speeds.items()) if i >= FIRST_ID and s]
    return '\n/* the 2.0 objects that turn by themselves, degrees a second (wiidash) */\n' \
           'static const struct { short id; short speed; } v2_spin[] = {\n' + '\n'.join(rows) + '\n};\n'


ROBOT_ANIMS = ['Robot_run', 'Robot_jump_start', 'Robot_jump_loop', 'Robot_fall_start', 'Robot_fall_loop']


def robot_source(wii, sprite):
    """The player's robot: its parts (robot_01_0N, main color and second
    color sprites) and wiidash's animations of them (robot.plist)."""
    import plistlib
    path = os.path.join(wii, 'data', 'extra', 'robot.plist')
    first = next(iter(plistlib.load(open(path, 'rb'))['animationContainer'].values()))
    parts = [sp['texture'] for sp in first.values()]
    out = ['', '/* the player\'s robot: each part\'s main color and second color sprites */',
           '#define ROBOT_PARTS %d' % len(parts),
           'static const short robot_part_tex[ROBOT_PARTS][2] = {']
    for t in parts:
        out.append('    { %d, %d }, /* %s */' % (sprite('wii:' + t), sprite('wii:' + t.replace('_001.png', '_2_001.png')), t))
    out.append('};')
    entries = []
    for a in ROBOT_ANIMS:
        frames = plist_frames(path, a)
        var = 'robot_' + a[6:].lower()
        out.append('static const AnimPose %s[] = {' % var)
        for f in frames:
            out.append('    ' + ' '.join('{ %.3ff, %.3ff, %.4ff, %.4ff, %.3ff },' % p for p in f))
        out.append('};')
        entries.append('    { %d, %s },' % (len(frames), var))
    out += ['static const struct { unsigned char frames; const AnimPose *poses; } robot_anims[] = {'] + entries + ['};']
    return '\n'.join(out) + '\n'


def anim_source(sprite, frame_rows, skel):
    out = ['/* generated by psp/tools/wii2objects.py: the animated 2.0 objects, as wiidash animates them */', '']
    entries = []
    for oid, (frames, fps, scale, flipped, own) in sorted(FRAME_ANIMS.items()):
        slots = SLOTS[oid]
        tex = []
        for f in frames:
            tex += [str(sprite('wii:' + f[slot])) for slot in slots]
        out.append('static const short anim_tex_%d[] = { %s };' % (oid, ', '.join(tex)))
        out.append('static const float anim_scale_%d[] = { %s };' % (oid, ', '.join('%.1ff' % scale[s] for s in slots)))
        flip = ', '.join('1' if k in flipped else '0' for k in range(len(frames)))
        out.append('static const unsigned char anim_flip_%d[] = { %s };' % (oid, flip))
        entries.append('    { %d, %d, %d, %d, %d, anim_tex_%d, anim_flip_%d, anim_scale_%d },'
                       % (oid, fps, len(frames), len(slots), int(own), oid, oid, oid))
    out += ['', 'static const FrameAnim frame_anims[] = {'] + entries + ['};', '']
    entries = []
    for oid, (fps, part_of, frames) in sorted(skel.items()):
        nparts = len(frames[0])
        out.append('static const unsigned char anim_part_%d[] = { %s };' % (oid, ', '.join(map(str, part_of))))
        out.append('static const AnimPose anim_pose_%d[] = {' % oid)
        for f in frames:
            out.append('    ' + ' '.join('{ %.3ff, %.3ff, %.4ff, %.4ff, %.3ff },' % p for p in f))
        out.append('};')
        entries.append('    { %d, %d, %d, %d, %d, anim_part_%d, anim_pose_%d },'
                       % (oid, fps, len(frames), nparts, len(part_of), oid, oid))
    out += ['', 'static const SkelAnim skel_anims[] = {'] + entries + ['};']
    return '\n'.join(out) + '\n'


def channel(v):
    v = (v or 'OBJ').strip()
    return CHANNELS[v] if v in CHANNELS else int(v, 0)


def num(v, default=0.0):
    try:
        return float(str(v).rstrip('f'))
    except (TypeError, ValueError):
        return default


def wii_row(oid, w, blocks, sprite, defs, glow_details=False, extras=None):
    """glow_details: glow layers as details (animated ones move them).
    extras: (texture, x, y, z, zlayer) sprites over the main one (a badge)."""
    layers = w['layers']
    names = [l['texture'][:-4] + '.png' for l in layers]
    glow = [i for i, n in enumerate(names) if '_glow_' in n or layers[i].get('color_type') == 'COLOR_GLOW']
    body = [i for i in range(len(layers)) if i not in glow]
    if not body:
        body, glow = glow, []
    main = next((i for i in body if int(num(layers[i].get('zlayer_offset'))) == 0), body[0])
    if glow_details:
        body, glow = list(range(len(layers))), []

    def color(l):
        ct = l.get('color_type', 'COLOR_MAIN')
        if ct == 'COLOR_UNMOD':
            # a fixed color: its channel's (wiidash only has black and white ones)
            return 'COLOR_TYPE_BLACK' if l.get('col_channel', '').strip() == 'BLACK' else 'COLOR_TYPE_WHITE'
        return {'COLOR_DETAIL': 'COLOR_TYPE_DETAIL', 'COLOR_GLOW': 'COLOR_TYPE_GLOW'}.get(ct, 'COLOR_TYPE_BASE')

    base_color = detail_color = 0
    has_main = False
    for i in body:
        c = color(layers[i])
        if c == 'COLOR_TYPE_BASE' and not has_main:
            base_color = channel(layers[i].get('col_channel'))
            has_main = True
        if c == 'COLOR_TYPE_DETAIL' and not detail_color:
            detail_color = channel(layers[i].get('col_channel'))
    if not has_main and detail_color:
        # only a detail: it takes the object's main channel (as in GD and
        # wiidash), so that's the one to default
        base_color, detail_color = detail_color, 0

    children = []
    before = [i for i in body if i < main]
    after = [i for i in body if i > main]
    SLOTS[oid] = [main] + before + after   # wiidash's layer of each sprite
    for k, i in enumerate(before + after):
        l = layers[i]
        z = -(len(before) - k) if i < main else 2 + (k - len(before))
        children.append('    { %d, %.2f, %.2f, 1.00, 1.00, 0, 0, %d, %d, 0.00, %s, 1.00 },' % (
            sprite('wii:' + names[i]), num(l.get('x_offset')), -num(l.get('y_offset')),
            z, int(num(l.get('zlayer_offset'))), color(l)))
    for tex, x, y, z, zl in (extras or []):
        children.append('    { %d, %.2f, %.2f, 1.00, 1.00, 0, 0, %d, %d, 0.00, COLOR_TYPE_WHITE, 1.00 },' % (
            tex, x, y, z, zl))

    hitbox_ref = 'NULL'
    if ('hitbox', oid) in blocks and oid not in WII_HITBOX:
        defs += ['const ObjectHitbox hitbox_v2_%d[] = {' % oid, blocks[('hitbox', oid)], '};']
        hitbox_ref = 'hitbox_v2_%d' % oid
    else:
        hb = w['hitbox']
        kind = HITBOX.get(hb.get('type'))
        if kind:
            r = num(hb.get('radius'))
            circle = hb.get('is_circular', '').upper() in ('TRUE', '1', 'true') and r > 0
            # a circle's "width" is its radius here (as the game's saws have it)
            ww, hh = (r, r) if circle else (num(hb.get('width')), num(hb.get('height')))
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
