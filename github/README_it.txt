# Geometry Dash PSP

A PSP port of [GD3DS](https://github.com/AleFunky/gd3ds) (AleFunky's Geometry
Dash for the 3DS; its README is [README-3DS.md](README-3DS.md)), by rLeo, with
Claude as collaborator. It runs on PSP-1000 and later, and in PPSSPP.

The game code is the 3DS one, mostly unchanged: the PSP side fakes the 3DS
libraries it uses (libctru, citro2d, citro3d) in `psp/`. Where the PSP needs
something different, the game code has it under `#ifdef __PSP__`.

## Building

Needs the [PSP toolchain](https://github.com/pspdev/pspdev) (`PSPDEV`, its `bin`
in `PATH`) and Python 3 with Pillow (the sprite sheet converter).

```
make -f Makefile.psp package        # psp/out/gdpsp/: EBOOT.PBP + romfs/
make -f Makefile.psp deploy DEPLOY=/path/to/folder
```

Copy `psp/out/gdpsp/` to `ms0:/PSP/GAME/`, or open its `EBOOT.PBP` in PPSSPP.
Saves, custom levels (`external_levels/`) and custom songs (`saved_songs/`)
go in `mstick/psp/gdpsp/` next to the EBOOT.

## What's where

| Path | What |
|---|---|
| `psp/src/c2d_psp.c` | the renderer: citro2d on the PSP's GE, the 3DS screens laid out on 480x272 |
| `psp/src/ctru_psp.c` | input (d-pad menus, the cursor), threads, file paths, the HID ring |
| `psp/src/ndsp_psp.c` | the 3DS DSP channels, mixed in software |
| `psp/src/mp3_me_psp.c` | the music, decoded on the Media Engine |
| `psp/include/` | the 3DS headers the game includes (`3ds.h`, `citro2d.h`, ...) |
| `psp/menus/*_psp.txt` | the PSP's one-screen layout of each menu |
| `psp/gfx/` | PSP art that replaces the 3DS one (the title logo, ...) |
| `psp/tools/mkatlas.py` | `.t3s` sprite lists to PSP sheets (8 bit, one palette per page) |
| `psp/eboot/` | the XMB icon (`ICON0.PNG`, 144x80) and background (`PIC1.PNG`, 480x272) |
| `Makefile.psp` | the PSP build |
