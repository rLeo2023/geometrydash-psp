#!/usr/bin/env python3
"""
mkrobots.py - the icon kit's robots, from the player's own copy of GD

Each robot (icons/robot_NN.plist/png, the standard resolution) has four parts,
each in the main color, the second color and, for some, an uncolored extra.
They go in one sheet, ROBOT_SPRITES per robot, in this order:

    (robot - 1) * 12 + (part - 1) * 3 + layer     layer 0 main, 1 second, 2 extra

(a missing extra is an empty pixel). Each part back at its untrimmed size, so
it centers like GD draws it (mkv2sheet.cut).

Usage: mkrobots.py <GD Resources folder> <work folder> <out.psheet>
"""
import os
import plistlib
import subprocess
import sys

from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
from mkv2sheet import cut  # noqa: E402

# the first ones only (GD has 68): the same as ROBOT_COUNT in source/icons.h
ROBOTS = 25
LAYERS = ('_001', '_2_001', '_extra_001')


def main():
    res, work, out = sys.argv[1:4]
    os.makedirs(work, exist_ok=True)
    blank = os.path.join(work, 'blank.png')
    Image.new('RGBA', (1, 1), (0, 0, 0, 0)).save(blank)

    t3s = ['--atlas --border edge -f rgba8888 -z auto']
    for r in range(1, ROBOTS + 1):
        name = 'robot_%02d' % r
        meta = plistlib.load(open(os.path.join(res, 'icons', name + '.plist'), 'rb'))
        sheet = Image.open(os.path.join(res, 'icons', name + '.png')).convert('RGBA')
        for part in range(1, 5):
            for suffix in LAYERS:
                frame = '%s_%02d%s.png' % (name, part, suffix)
                d = meta['frames'].get(frame)
                if not d:
                    t3s.append('blank.png')
                    continue
                cut(sheet, d).save(os.path.join(work, frame))
                t3s.append(frame)

    t3s_path = os.path.join(work, 'robots.t3s')
    open(t3s_path, 'w').write('\n'.join(t3s) + '\n')
    subprocess.check_call([sys.executable, os.path.join(HERE, 'mkatlas.py'), t3s_path, out])


if __name__ == '__main__':
    main()
