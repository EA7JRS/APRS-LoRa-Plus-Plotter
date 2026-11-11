#!/bin/sh
# Copyright (C) 2026 Manuel - EA7JRS
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Dibuja en el PC la interfaz del firmware (src/ui.cpp, con LVGL de verdad y el simulador) y la guarda como imagen PPM.
# Sirve para ver una pantalla sin tener la placa y para comprobar que la interfaz compila contra LVGL.
# La tarjeta (S:) es la carpeta /tmp/plotter_sd: tools/ui_preview/fake_tiles.py la llena con teselas de prueba.
#   git clone --depth 1 --branch v9.6.0 https://github.com/lvgl/lvgl /tmp/lvgl
#   tools/ui_preview/build.sh /tmp/lvgl salida.ppm [pestana 0-7] [segundos de simulacion] [acciones: cerrar,mob,marcar,tocar:x:y,boton:n,pagina:n,esperar:s]
set -e
LVGL="$1"; OUT="${2:-ui.ppm}"
[ -d "$LVGL/src" ] || { echo "uso: $0 <carpeta de lvgl 9.6> [salida.ppm]"; exit 2; }
HERE="$(cd "$(dirname "$0")" && pwd)"; ROOT="$HERE/../.."
B="${UI_PREVIEW_BUILD:-/tmp/plotter_ui_preview}"; mkdir -p "$B/obj"
DEFS="-DBOARD_LCD7B -DLV_CONF_SKIP=1 -DLV_FONT_MONTSERRAT_12=1 -DLV_FONT_MONTSERRAT_14=1 -DLV_FONT_MONTSERRAT_16=1 -DLV_FONT_MONTSERRAT_20=1 -DLV_FONT_MONTSERRAT_24=1 -DLV_USE_LOG=0 -include $HERE/host_conf.h -include $ROOT/src/lv_defs.h -I$HERE -I$LVGL -I$ROOT/src -I$ROOT/lib/plotter_core/src"
if [ ! -f "$B/lvgl.done" ]; then      # LVGL solo se compila la primera vez
    find "$LVGL/src" -name "*.c" | xargs -P 8 -I{} sh -c 'gcc -O1 -w -c '"$DEFS"' {} -o '"$B"'/obj/$(echo {} | md5sum | cut -c1-16).o'
    touch "$B/lvgl.done"
fi
for f in "$ROOT"/src/fonts/*.c; do gcc -O1 -w -c $DEFS "$f" -o "$B/obj/font_$(basename "$f" .c).o"; done
g++ -std=gnu++17 -O1 -Wall -Wextra $DEFS "$HERE/host.cpp" "$ROOT/src/ui.cpp" "$ROOT/src/links.cpp" "$ROOT/src/settings.cpp" "$ROOT"/lib/plotter_core/src/*.cpp "$B"/obj/*.o -o "$B/host"
"$B/host" "$OUT" "${3:-0}" "${4:-20}" $5
echo "imagen en $OUT"
