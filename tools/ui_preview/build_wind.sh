#!/bin/sh
# Copyright (C) 2026 Manuel - EA7JRS
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Dibuja en el PC la interfaz del instrumento de viento (src/wind/wui.cpp, con LVGL de verdad y el simulador).
#   tools/ui_preview/build_wind.sh <carpeta de lvgl 9.6> salida.ppm [pagina 0-4] [segundos] [reparto de la rosa 0-2] [wifi]
# Comparte los objetos de LVGL con build.sh (UI_PREVIEW_BUILD).
set -e
LVGL="$1"; OUT="${2:-wind.ppm}"
[ -d "$LVGL/src" ] || { echo "uso: $0 <carpeta de lvgl 9.6> [salida.ppm]"; exit 2; }
HERE="$(cd "$(dirname "$0")" && pwd)"; ROOT="$HERE/../.."
B="${UI_PREVIEW_BUILD:-/tmp/plotter_ui_preview}"; mkdir -p "$B/obj"
DEFS="-DBOARD_LCD4 -DLV_CONF_SKIP=1 -DLV_FONT_MONTSERRAT_12=1 -DLV_FONT_MONTSERRAT_14=1 -DLV_FONT_MONTSERRAT_16=1 -DLV_FONT_MONTSERRAT_20=1 -DLV_FONT_MONTSERRAT_24=1 -DLV_USE_LOG=0 -include $HERE/host_conf.h -include $ROOT/src/lv_defs.h -I$HERE -I$LVGL -I$ROOT/src -I$ROOT/src/wind -I$ROOT/lib/plotter_core/src"
if [ ! -f "$B/lvgl.done" ]; then      # LVGL solo se compila la primera vez
    find "$LVGL/src" -name "*.c" | xargs -P 8 -I{} sh -c 'gcc -O1 -w -c '"$DEFS"' {} -o '"$B"'/obj/$(echo {} | md5sum | cut -c1-16).o'
    touch "$B/lvgl.done"
fi
for f in "$ROOT"/src/fonts/*.c; do gcc -O1 -w -c $DEFS "$f" -o "$B/obj/font_$(basename "$f" .c).o"; done
g++ -std=gnu++17 -O1 -Wall -Wextra -Werror $DEFS "$HERE/host_wind.cpp" "$ROOT"/src/wind/wui.cpp "$ROOT"/src/wind/wlinks.cpp "$ROOT"/src/wind/wsettings.cpp "$ROOT"/src/wind/wble.cpp "$ROOT"/src/wind/wnet.cpp "$ROOT"/lib/plotter_core/src/*.cpp "$B"/obj/*.o -o "$B/host_wind"
"$B/host_wind" "$OUT" "${3:-0}" "${4:-20}" ${5:-2} $6
echo "imagen en $OUT"
