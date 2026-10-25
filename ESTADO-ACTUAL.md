# Geometry Dash 2.0 para PSP — estado actual

Snapshot al **25/10/2026**. Este archivo resume lo esencial para retomar el trabajo.
`CONTEXT.md` y `contexto completo.txt` son de fechas anteriores (22-23/10) y pueden
estar desactualizados: ante duda, vale este archivo.

## Proyecto

- Port de Geometry Dash 2.0 (base GD3DS de AleFunky + soporte 2.0 de wiidash).
- Directorio: `/home/familia_oso/geometrydash2.0-psp`, rama **v2.0**.
- Último commit: `edbff9a revert`. Hay cambios sin commitear (ver abajo).
- `gd3ds/`, `gd3ds-2.0/`, `wiidash/` y `~/gdopenpsp` son **solo lectura** (referencias).
- El usuario prueba él mismo en PPSSPP y testers en PSP real. **No** automatizar
  pruebas ni capturas.
- Responder en español rioplatense.

## Build y deploy

```bash
export PSPDEV=$HOME/pspdev
export PATH=$PATH:$PSPDEV/bin
make -f Makefile.psp -j8                 # -> EBOOT.PBP + gdpsp.prx
make -f Makefile.psp package             # regenera los .psheet
```

- Build actual: EBOOT md5 **`15353dbbb8c672e57111c9ca7cdf0e29`**
  (PRX 1.426.054 bytes, compilado 25/10 02:36).
- Deploy de prueba: `/mnt/e/Juegos PS2/gd20psp` (= `E:\Juegos PS2\gd20psp`);
  cerrar PPSSPP antes de copiar el EBOOT.
- Carpeta release: `github/` (EBOOT + `romfs/gfx/*.psheet` + `mstick/` +
  `README_it.txt`/`README_pt.txt`). Está sin commitear.
- Psheets actuales (idénticos en `psp/out/gdpsp/romfs/gfx/` y `github/romfs/gfx/`):
  - `glow.psheet` `6f575674a00d3b406495982c55d6d519`
  - `sprites.psheet` `64695816b83f364cd14af5a06e83a74c`
  - `portals.psheet` `3a51db42cb65787d84d5a6b35a30579a`
- Un atlas nuevo: `python3 psp/tools/mkatlas.py <archivo.t3s> <salida.psheet>`.
- En PSP real, los `.psheet` van en `ms0:/PSP/GAME/GeometryDashPSP/romfs/`.
- El log de arranque identifica el build (`built ...`). `psp_log.txt` va junto al EBOOT.

## Cambios de las últimas sesiones (25/10)

- **Ghost trail** (`source/player/player.h`, `player.c`): copia cada 1/40 s,
  `P1_TRAIL_LENGTH 20`, `P1_TRAIL_DURATION 0.35f`, opacidad inicial **0.45**
  (fade `0.45 / P1_TRAIL_DURATION`). Ajustado a gusto del usuario; antes era
  0.16 s. Vigilar el costo en PSP real (unas 14 copias del icono).
- **Optimización conservada** (`source/graphics.c`): `spawn_object_at` calcula
  una vez por objeto la opacidad (`get_obj_opacity`) y el canal base, y cachea
  canales por tipo (`chan_of_cached`); `set_sprite_tint` los recibe como
  parámetros. Sin cambio visual.
- **Culling descartado**: el culling por sprite (`sprite_offscreen`) provocaba
  sprites apareciendo tarde en el borde; se revirtió. No reintentarlo tal cual.
- **Glow de los `d_gradient` (1011/1012, grupo 112) — SIN RESOLVER**: en el port
  se ve más grande que en GD (como si usara la textura grande en vez de la
  mediana). Ya se descartaron: texturas faltantes/desalineadas y forzar blending
  aditivo. No repetir esas dos hipótesis. Próxima vía sin explorar: cómo elige
  GD el tamaño del glow según la escala del objeto.
- **Portales 2.2**: badge de 3 capas (medio disco + icono) en portales de
  gamemode/gravedad; arte del portal de robot agregado al final de
  `portals.t3s`. Calibrado a +12 (badge) / +10 (icono).
- **Engranajes merged** (`merged_*` de cuartos): regenerados alineados; en
  `gfx/` hay 7 PNG modificados sin commitear.
- Otros ya implementados y probados: teclas (efecto + flote), user coins,
  escala 2.0 (keys 128/129), fondos 2.0, traducciones (ES/IT/PT), perfil
  offline con teclado propio, precisión noclip, fuego del robot.

## Correcciones pendientes de validación

- **Corrupción de texturas en PSP real**: el atlas de objetos se asigna en VRAM
  después del depth buffer (`PSP_VramAlloc` / `ZBUF_RESERVE` = 278.528 bytes) y
  una página 512×512 quedó en RAM (+256 KB). Falta que un tester (Zerium) lo
  pruebe en PSP real y mande `psp_log.txt`; en PPSSPP no se reproduce.
- **Popping en el borde**: confirmar que desapareció con el EBOOT actual.
- **Rendimiento**: 60,4–60,6 fps en gameplay (PPSSPP); update máx 6 ms, draw
  máx 4,8 ms. Hitch de ~266 ms al abrir pausa y ~6,1 s en carga/teardown de
  nivel. Falta `psp_log.txt` de Deadlocked en PSP real.

## Cambios sin commitear (al 25/10)

24 archivos modificados, entre ellos `source/graphics.c`, `source/player/player.{c,h}`,
`source/objects.c`, `source/objects_v2_defs.inc`, `source/menus/level_complete.c`,
`psp/src/c2d_psp.c`, `psp/tools/wii2objects.py`, `gfx/merged_*.png`,
`gfx/portals/portals.t3s`, `romfs/menus/*_psp.txt`; sin trackear:
`gfx/portals/portal_14_extra_001.png`, `gfx/portals/portal_14_extra_2_001.png`
y `github/`. Commitear solo cuando el usuario lo pida.

## Pendientes abiertos

1. Glow de `d_gradient` más grande que en GD (ver arriba; sin resolver).
2. Validar en PSP real la corrupción de texturas y el rendimiento.
3. Confirmar que el popping del borde ya no aparece.
4. Hitch del menú de pausa (~266 ms) y posible bug del UIList de idiomas.
5. Logros: sin sistema; falta definir alcance.
6. Push pendiente de la rama `v2.0` (el usuario pidió dejarlo para después).
7. Música > 4 MB: el motor MP3 necesita el archivo completo en RAM (streaming
   pendiente, baja prioridad).

## Restricciones del proyecto

- No tocar los fades laterales ni la cámara de portales.
- Editor de niveles descartado; no integrar 2.1 (sin spider/editor).
- Niveles de Meltdown solo en Extras, no en Custom.
- No usar `~/gdopenpsp` para editar; `gd3ds`/`wiidash` solo como referencia.
- `menuLoop.mp3` es la música interna de los menús; no cambiarla.

## Datos útiles

- Pantalla lógica 400×240; área de juego 533,33×320 escalada ×1,2 a 480×272
  (recorta 8 px arriba/abajo).
- `MAX_SPRITES` 4096. Referencia: Geometrical Dominator ~550 sprites, 10,2 MB
  usados / 9,5 MB libres.
- `autoload.gmd` junto al EBOOT: carga y juega solo con noclip; renombrarlo a
  `.off` para entrar a los menús.
- Niveles y canciones de Meltdown en `mstick/psp/gdpsp/external_levels/Meltdown/`.
