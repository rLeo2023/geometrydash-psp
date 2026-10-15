#!/usr/bin/env python3
"""
mkatlas.py - build PSP sprite sheets (.psheet) from the game's tex3ds .t3s lists

The game picks its sprites by their position in the .t3s list, so the order is
kept. Images are packed into pages of at most 512x512 (the GE's limit), each
page in the format the .t3s asked for, as close as the PSP gets:

  l8                    -> T8 with a grey palette
  everything else       -> T8 with its page's own 256 colors (libimagequant),
                           a quarter of 8888: the PSP-1000 has no room for 8888
  --psm 8888|4444       -> that instead, for every sheet (to compare)

The pages are stored swizzled (16 byte x 8 line blocks), the order the GE
reads fastest.

File layout, little endian:
  "PSH1" u32 images u32 pages
  pages:  u16 w, u16 h, u8 psm (0 8888, 1 4444, 2 T8; | 0x80 swizzled),
          u8 pad[3], then for T8 256 palette entries (u32 RGBA), then the texels
  images: u16 page, u16 x, u16 y, u16 w, u16 h, u16 pad

Usage: mkatlas.py [--psm 8888|4444] <file.t3s> <out.psheet>
"""
import struct
import sys
import os
from PIL import Image

PAGE_MAX = 512
PSM_8888, PSM_4444, PSM_T8 = 0, 1, 2
PSM_SWIZZLED = 0x80
FORCE_PSM = None


def psp_override(path):
    """gfx/x/y.png -> psp/gfx/x/y.png when there is one: the PSP's own art
    (the title's logo...), without touching the 3DS build's files."""
    rel = os.path.relpath(path, 'gfx')
    alt = os.path.join('psp', 'gfx', rel)
    return alt if not rel.startswith('..') and os.path.exists(alt) else path


def parse_t3s(path):
    lines = [l.strip() for l in open(path) if l.strip()]
    opts = lines[0].split()
    fmt = opts[opts.index('-f') + 1] if '-f' in opts else 'rgba8888'
    border = opts[opts.index('--border') + 1] if '--border' in opts else 'none'
    base = os.path.dirname(path)
    return fmt, border, [psp_override(os.path.join(base, l)) for l in lines[1:]]


def pick_psm(fmt, t3s):
    if FORCE_PSM is not None:
        return FORCE_PSM
    return PSM_T8


def pow2(n):
    p = 16  # a swizzled T8 page is at least one 16 byte block wide
    while p < n:
        p *= 2
    return p


def pack(images, border):
    """Shelf packer: tallest first. Returns per image (page, x, y) and page sizes."""
    pad = 1 if border != 'none' else 0
    order = sorted(range(len(images)), key=lambda i: (-images[i].size[1], -images[i].size[0]))
    pages = []          # [shelves, used_h] ; shelf = [y, h, x_cursor]
    place = [None] * len(images)

    for i in order:
        w, h = images[i].size
        p = pad if w + 2 * pad <= PAGE_MAX and h + 2 * pad <= PAGE_MAX else 0
        bw, bh = w + 2 * p, h + 2 * p
        done = False
        for pi, page in enumerate(pages):
            shelves = page[0]
            for shelf in shelves:
                if bh <= shelf[1] and shelf[2] + bw <= PAGE_MAX:
                    place[i] = (pi, shelf[2] + p, shelf[0] + p, p)
                    shelf[2] += bw
                    done = True
                    break
            if done:
                break
            if page[1] + bh <= PAGE_MAX:
                shelves.append([page[1], bh, bw])
                place[i] = (pi, p, page[1] + p, p)
                page[1] += bh
                done = True
                break
        if not done:
            pages.append([[[0, bh, bw]], bh])
            place[i] = (len(pages) - 1, p, p, p)

    sizes = []
    for pi, page in enumerate(pages):
        mw = max(s[2] for s in page[0])
        sizes.append((pow2(mw), pow2(page[1])))
    return place, sizes


def blit(canvas, img, x, y, p, border):
    canvas.paste(img, (x, y))
    if p == 0 or border != 'edge':
        return
    w, h = img.size
    # extend the edge pixels into the border, so linear filtering doesn't
    # pull in the neighbour's texels (tex3ds --border edge)
    canvas.paste(img.crop((0, 0, w, 1)), (x, y - 1))
    canvas.paste(img.crop((0, h - 1, w, h)), (x, y + h))
    canvas.paste(img.crop((0, 0, 1, h)), (x - 1, y))
    canvas.paste(img.crop((w - 1, 0, w, h)), (x + w, y))
    for cx, cy, sx, sy in ((x - 1, y - 1, 0, 0), (x + w, y - 1, w - 1, 0),
                           (x - 1, y + h, 0, h - 1), (x + w, y + h, w - 1, h - 1)):
        canvas.putpixel((cx, cy), img.getpixel((sx, sy)))


def swizzle(data, width_bytes, height):
    """The GE's fast layout: 16 byte x 8 line blocks, row of blocks by row."""
    out = bytearray(len(data))
    o = 0
    for by in range(0, height, 8):
        for bx in range(0, width_bytes, 16):
            for y in range(by, by + 8):
                i = y * width_bytes + bx
                out[o:o + 16] = data[i:i + 16]
                o += 16
    return bytes(out)


def quantize(canvas, fmt):
    """A page's 256 colors and its indices. Returns (palette, indices, error)."""
    if fmt == 'l8':
        # opaque grey: the luminance is the index
        pal = [(l, l, l, 255) for l in range(256)]
        return pal, canvas.convert('L').tobytes()
    q = canvas.quantize(256, method=Image.Quantize.LIBIMAGEQUANT)
    raw = q.getpalette('RGBA') or []
    pal = [tuple(raw[i:i + 4]) for i in range(0, len(raw), 4)][:256]
    pal += [(0, 0, 0, 0)] * (256 - len(pal))
    return pal, q.tobytes()


def error_of(canvas, pal, idx):
    """Mean difference per channel, alpha weighted (what shows on screen)."""
    px = canvas.tobytes()
    total, n = 0, 0
    for i in range(0, len(idx), 7):  # a sample is enough
        r, g, b, a = px[i * 4:i * 4 + 4]
        pr, pg, pb, pa = pal[idx[i]]
        total += (abs(r - pr) + abs(g - pg) + abs(b - pb)) * max(a, pa) / 255 + abs(a - pa)
        n += 4
    return total / max(n, 1)


def encode(canvas, psm, fmt):
    """The page's data (palette first for T8) and its error."""
    w, h = canvas.size
    px = canvas.tobytes()  # RGBA
    if psm == PSM_8888:
        return swizzle(px, w * 4, h), 0.0
    if psm == PSM_4444:
        out = bytearray(len(px) // 2)
        for i in range(0, len(px), 4):
            r, g, b, a = px[i], px[i + 1], px[i + 2], px[i + 3]
            v = (r >> 4) | ((g >> 4) << 4) | ((b >> 4) << 8) | ((a >> 4) << 12)
            out[i // 2] = v & 0xff
            out[i // 2 + 1] = v >> 8
        return swizzle(bytes(out), w * 2, h), 0.0
    pal, idx = quantize(canvas, fmt)
    err = error_of(canvas, pal, idx)
    return b''.join(struct.pack('<BBBB', *c) for c in pal) + swizzle(idx, w, h), err


def main():
    global FORCE_PSM
    args = sys.argv[1:]
    if args[0] == '--psm':
        FORCE_PSM = {'8888': PSM_8888, '4444': PSM_4444}[args[1]]
        args = args[2:]
    t3s, out = args
    fmt, border, files = parse_t3s(t3s)
    psm = pick_psm(fmt, t3s)
    images = [Image.open(f).convert('RGBA') for f in files]
    place, sizes = pack(images, border)

    canvases = [Image.new('RGBA', s, (0, 0, 0, 0)) for s in sizes]
    for i, img in enumerate(images):
        pi, x, y, p = place[i]
        blit(canvases[pi], img, x, y, p, border)

    worst = 0.0
    with open(out, 'wb') as f:
        f.write(b'PSH1' + struct.pack('<II', len(images), len(canvases)))
        for c in canvases:
            data, err = encode(c, psm, fmt)
            worst = max(worst, err)
            f.write(struct.pack('<HHB3x', c.size[0], c.size[1], psm | PSM_SWIZZLED))
            f.write(data)
        for i, img in enumerate(images):
            pi, x, y, p = place[i]
            f.write(struct.pack('<HHHHHH', pi, x, y, img.size[0], img.size[1], 0))

    total = sum(w * h for w, h in sizes)
    print(f"{os.path.basename(out)}: {len(images)} images, {len(sizes)} pages {sizes[:4]}{'...' if len(sizes) > 4 else ''}, "
          f"{total * (4 if psm == PSM_8888 else 2 if psm == PSM_4444 else 1) // 1024} KB, error {worst:.2f}")


if __name__ == '__main__':
    main()
