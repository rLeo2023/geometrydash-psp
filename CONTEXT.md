# Geometry Dash 2.0 — port a PSP: contexto exportable

Fecha: 2026-10-22 (build de diagnóstico `03944e38...`, compilado 20:49:14)

## 1. Objetivo del proyecto
Port de Geometry Dash 2.0 (originalmente 3DS) a PSP, manteniendo 60 fps en Deadlocked, fidelidad visual y menús. Se prueba en PPSSPP y PSP real (PSP-1000, CFW 6.60). No automatizar pruebas ni capturas: el usuario/tester las hace a mano.

## 2. Entorno de trabajo
- Repo: `/home/familia_oso/geometrydash2.0-psp` (git, rama principal, ~48 archivos modificados sin commitear).
- Build: `export PSPDEV=$HOME/pspdev; export PATH=$PATH:$PSPDEV/bin; make -f Makefile.psp -j8`
- Deploy: `make -f Makefile.psp deploy DEPLOY="/mnt/e/Juegos PS2/gd20psp"` (solo copia EBOOT + romfs/gfx/*.psheet; NO copia `mstick`).
- Carpeta de prueba del usuario: `E:\Juegos PS2\gd20psp` (mapeada como `/mnt/e/Juegos PS2/gd20psp`).
- Instalación limpia: `E:\Juegos PS2\gd20psp-clean` (EBOOT nuevo + romfs + mstick vacío).
- Referencias permitidas: `gd3ds`, `wiidash` (solo lectura). NO usar `~/gdopenpsp` para editar.
- PPSSPP no está instalado en esta máquina; el usuario prueba manualmente.

## 3. Bloqueos y restricciones del proyecto (congelados)
- No tocar fades laterales ni cámara de portales.
- Editor de niveles descartado por ahora.
- Resolución dinámica 400x240 descartada.
- `ReTraY` y `Cant Let Go v2` se dejan como están.
- Niveles de Meltdown solo en Extras (`extra_levels`), no en Custom.

## 4. Decisiones importantes ya tomadas
- Extras = pantalla nueva con UI del creator menu (logo + Meltdown + Map Packs), art en sheet propio `extras.psheet` (slot 10).
- Map Packs reutiliza el browser de `external_levels` con `external_mappacks_mode` y base `MAPPACKS_DIR`.
- Dificultad del popup desde `k26` (estrellas) vía `difficulty_stars[]`.
- Monedas plateadas = `state.mappack_level` hace que `level_has_unverified_coins()` devuelva false.
- Low Detail Mode = "Modo rendimiento"/"Performance mode": mantiene solo el trail del wave.
- Map Packs: descarga aparte, no redirige a Custom; si no hay archivos, mostrar mensaje para descargarlos del GitHub.
- Sin PNG/label "featured" en map packs.
- Monedas: tarjetas en 0; oficiales doradas, user level sin verificar marrones, map packs plateadas.
- Perfil replicando el diseño del GD original (sin araña, lunita, botones sociales ni global rank).
- Level Complete: mantener animación de la estrella aunque se salteen las monedas.
- No revertir el present temprano (input lag).
- Logging siempre encendido: línea `perf ms` cada 120 frames (2 s) + `slow frame` limitado a 1 por segundo con contador de frames tardíos.
- Contadores por frame `psp_draw_calls`/`psp_tex_changes`/`psp_blend_changes` (++ por evento).
- Línea de identificación de build al inicio del log (`built ...`, firmware) para saber qué EBOOT corrió.

## 5. Estado actual del reporte de "no arranca"
### Síntoma
El tester reporta "I try to launch the game and I get sent back to XMB". Un EBOOT suelto también crasheaba en PPSSPP.

### Evidencia recogida
- `psp_log.txt` del tester (build anterior, 22/10 19:32, PPSSPP) mostraba arranque + menús + carga de nivel.
- Con el build de diagnóstico (boot steps), el log del tester llega hasta:
  ```
  gdpsp: started, built Oct 22 2026 19:36:39, firmware 06060010
  boot: graphics ok
  boot: clock ok
  boot: config ok
  boot: screens ok
  boot: audio ok
  ```
  y se corta ahí. Es decir, muere en `required_loading_screen_assets_init()` (carga de `ui.t3x`, `ui_2.t3x`, `goldFont.t3x`, `bars.t3x`, `extras.t3x`), antes del primer frame.
- El tester dice ahora que **el EBOOT viejo (`f954bff`) tampoco arranca**. Eso apunta a un problema de instalación/entorno, no del build nuevo.
- El usuario confirmó que `gd20psp-clean` (instalación limpia) arranca bien en PPSSPP con el EBOOT nuevo.

### Instrumentación añadida (build de diagnóstico actual, md5 `03944e38...`)
- `source/main.c`: hitos `boot: ...` por cada paso del arranque.
- `psp/src/c2d_psp.c`: log por hoja (`sheet: ...: N pages, M images, K KB free`) y por página (`sheet: ...: page i WxH, K KB`), más guardas NULL en los `calloc`/`memalign` que antes crasheaban en silencio.
- `psp/src/ctru_psp.c`: stamp de build por archivo (`psp/src/build_id.c`, regenerado en cada build por el Makefile) para que la primera línea del log diga qué EBOOT corrió de verdad.
- `Makefile.psp`: regla `buildstamp` que reescribe `build_id.c` antes de compilar.

### Próximo paso con el tester
1. Copiar `EBOOT-diag-03944e38.pbp` (en `ab/diag-03944e38.zip`) como `EBOOT.PBP` en `ms0:/PSP/GAME/GeometryDashPSP/`.
2. Intentar abrir el juego.
3. Mandar el `psp_log.txt` completo. La última línea `sheet:` dirá exactamente qué hoja/página muere (o `C2D_SpriteSheetLoad: can't open ...` si falta un archivo).
4. Preguntar: espacio libre en el Memory Stick, y si probó copiar de cero a una carpeta nueva (no encima de la anterior).

### Hipótesis fuertes a verificar
- Instalación incompleta/corrupta del `romfs` (falta `romfs/gfx/*.psheet`).
- Memory Stick falso/lleno que corrompe lecturas grandes.
- Dos instalaciones mezcladas (el tester copió el zip encima de la vieja).

## 6. Archivos clave modificados (sin commitear)
- `source/main.c`: logging siempre activo, fases de tiempo, hitos de arranque.
- `psp/src/c2d_psp.c`: contadores `psp_draw_calls`/`psp_tex_changes`/`psp_blend_changes`, reset en `C3D_FrameBegin`, `psp_ge_sync_us`/`psp_frame_wait_us`, logging de sheets.
- `psp/src/psp_port.h`: externs de los contadores nuevos.
- `psp/src/ctru_psp.c`: `psp_log`, `envGetHeapSize`, stamp de build.
- `source/graphics.c`: `create_objects` (1749), `spawn_object_at` (928), `set_sprite_tint` (830), `sort_viewable_objects` (1240), `draw_objects` (1987), pico de sprites.
- `psp/include/citro2d.h`: `C2D_SpriteSetScale` (2 divisiones por sprite), `C2D_DrawSprite`.
- `source/menus/core/ui_screen.c`: `required_loading_screen_assets_init`, `ui_assets_init`.
- `Makefile.psp`: regla `buildstamp`.
- `psp/src/build_id.c`: nuevo, stamp por build.
- Más: `level_select.c`, `profile.c`, `settings.c/.h`, `i18n.c`, `external_levels.c`, `external_popup.c`, `extras_menu.c`.

## 7. Builds de referencia
- Funcional previo: `f954bff0613726550d5115c1884ea7f2` (1.941.480 bytes).
- Anterior: `06b8e2dcde47e348559186ffedcebf39`.
- Actual (con diagnóstico): `03944e38ffbc7705008048e2ea2dc03c` (1.944.256 bytes, stamp 20:49:14).
- El link siempre imprime `psp-fixup-imports: Warning: could not fixup imports, stubs out of order.` — también en builds que funcionaron; no es causa.

## 8. Convenciones para la próxima sesión
- Responder en español rioplatense.
- No automatizar pruebas ni sacar capturas; el usuario/tester las hace.
- No tocar fades laterales ni cámara de portales.
- No usar `~/gdopenpsp` para editar (solo lectura); `gd3ds`/`wiidash` como referencia.
- Commitear los cambios pendientes solo cuando el usuario lo pida.
- Carpeta de prueba: `E:\Juegos PS2\gd20psp` (`/mnt/e/Juegos PS2/gd20psp`).
- Si se necesita un build nuevo: `make -f Makefile.psp -j8` y `make -f Makefile.psp deploy DEPLOY="/mnt/e/Juegos PS2/gd20psp"`.
- El stamp de build se regenera solo (Makefile); la primera línea del log identifica el EBOOT.

## 9. Dónde están los artefactos
- `E:\Juegos PS2\gd20psp\ab\diag-03944e38.zip` — EBOOT de diagnóstico.
- `E:\Juegos PS2\gd20psp\ab\` — EBOOTs viejo/nuevo para prueba A/B.
- `E:\Juegos PS2\gd20psp-clean\` — instalación limpia (EBOOT nuevo + romfs + mstick vacío).
- `E:\Juegos PS2\gd20psp\psp_log.txt` — log de la última corrida (si lo hay).
- `E:\Juegos PS2\gd20psp\RELEASE.md` y `github\RELEASE.md` — notas v2.0.4.
