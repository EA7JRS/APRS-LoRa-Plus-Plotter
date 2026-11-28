#!/bin/sh
# Copyright (C) 2026 Manuel - EA7JRS — SPDX-License-Identifier: GPL-3.0-or-later
# Valida el almanaque del nucleo (ephem) contra astropy/ERFA (patron sin descargas).
# Requiere: g++, python3 con numpy y astropy (pip install astropy). No va en la CI.
set -e
cd "$(dirname "$0")/../.."
mkdir -p .build
g++ -std=gnu++17 -O2 -Ilib/plotter_core/src lib/plotter_core/src/*.cpp tools/almanac_validate/dump_ephem.cpp -o .build/dump_ephem
.build/dump_ephem > .build/alm_ours.txt
python3 -I tools/almanac_validate/ref.py       > .build/alm_ref.txt
python3 -I tools/almanac_validate/ref_apsis.py > .build/alm_ref_apsis.txt
echo "== fases / estaciones / apsides: ephem vs astropy =="
python3 -I - <<'PY'
import sys; sys.argv=['cmp']
exec(open('tools/almanac_validate/cmp.py').read().replace('/tmp/almanac/ours.txt','.build/alm_ours.txt').replace('/tmp/almanac/ref.txt','.build/alm_ref.txt'))
PY
