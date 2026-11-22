#!/usr/bin/env python3
# Copyright (C) 2026 Manuel - EA7JRS — SPDX-License-Identifier: GPL-3.0-or-later
"""Libro de Radioseñales del IHM → lista de costeras y centros de salvamento para la tarjeta del plotter.

    python3 tools/radiosenales/radiosenales.py radiosenales.pdf -o costeras.txt

Saca del PDF (con pdftotext) y escribe, en el formato de coast.h (`nombre;lat;lon;tipo;MMSI;canal;canal 2;kHz`):
  - las estaciones costeras de VHF de la tabla «Zona A1» (nombre, posición, canal de trabajo, canal de Salvamento
    Marítimo y el MMSI del centro de comunicaciones que las lleva);
  - los centros de salvamento que el libro da con su posición y su MMSI.
La onda media no se saca (su tabla trae varios emplazamientos por estación). Este programa no lleva ningún dato del
libro: los datos son © Instituto Hidrográfico de la Marina y la lista es para uso propio. Revise lo que sale.
"""
import argparse, re, subprocess, sys

POS = r"(\d{1,2})[º°o](\d{1,2})(?:[,.](\d+))?\s*[’']?\s*([NS])[,\s]+(\d{1,3})[º°o]\s?(\d{1,2})(?:[,.](\d+))?\s*[’']?\s*([EW])"

def deg(d, m, frac, hemi):
    v = int(d) + float(m + ('.' + frac if frac else '')) / 60.0
    return -v if hemi in 'SW' else v

def position(text):
    m = re.search(POS, text)
    return (deg(m[1], m[2], m[3], m[4]), deg(m[5], m[6], m[7], m[8])) if m else None

def vhf(lines):
    """La tabla de VHF: filas «nombre  latitud  longitud  canal  canal»; el MMSI de cada grupo va a media altura."""
    out, start = [], None
    for i, l in enumerate(lines):
        if 'Zona A1' in l and start is None: start = i
    if start is None: return out
    rows = []                       # (nombre, lat, lon, canal, canal 2) o ('MMSI', numero)
    for l in lines[start:start + 90]:
        if l.startswith('Nota:') and rows: break
        mm = re.search(r"\b(00\d{7})\b", l)
        if mm: l2 = l[mm.end():]                       # el rótulo del grupo y su MMSI van delante del nombre
        else: l2 = l
        m = re.search(r"(?:^|\s{2,})([A-ZÁÉÍÓÚÑ][\wáéíóúñü. ]*?)\s{2,}" + POS + r"\s+(\d{1,2})\s+(\d{1,2})\s*$", l2)
        if mm and not m: rows.append(('MMSI', mm[1])); continue
        if not m: continue
        if mm: rows.append(('MMSI*', mm[1]))            # el MMSI en la misma línea que una estación
        rows.append((m[1].strip(), deg(m[2], m[3], m[4], m[5]), deg(m[6], m[7], m[8], m[9]), int(m[10]), int(m[11])))
    # el MMSI va centrado en su grupo: tantas filas detrás como delante (y la suya, si va en la línea de una estación)
    stations = [r for r in rows if not r[0].startswith('MMSI')]
    marks, n = [], 0
    for r in rows:
        if r[0] == 'MMSI': marks.append((n, r[1], False))
        elif r[0] == 'MMSI*': marks.append((n, r[1], True))
        else: n += 1
    first = 0
    for at, mmsi, same in marks:
        size = 2 * (at - first) + (1 if same else 0)
        for s in stations[first:first + size]: out.append((s[0], s[1], s[2], 'V', mmsi, s[3], s[4]))
        print(f'  VHF, MMSI {mmsi}: {", ".join(s[0] for s in stations[first:first + size])}', file=sys.stderr)
        first += size
    if first != len(stations):
        print(f'AVISO: {len(stations) - first} estaciones de VHF sin grupo (revise la tabla): salen sin MMSI', file=sys.stderr)
        for s in stations[first:]: out.append((s[0], s[1], s[2], 'V', '', s[3], s[4]))
    return out

def centres(lines):
    """Centros con posición y, en las líneas siguientes, «MMSI: 00…»."""
    out, seen = [], set()
    for i, l in enumerate(lines):
        m = re.match(r"\s*(?:MRCC\s+)?([A-ZÁÉÍÓÚÑ][\wáéíóúñü ]*?)\s*(?:\[[A-Z]\]\s*)*\((?:CCS|CROSS)\)", l) or re.match(r"\s*MRCC\s+([A-ZÁÉÍÓÚÑ][\wáéíóúñü ]*?)\s*\(", l)
        if not m: continue
        p = position(l)
        mm = next((x[1] for x in (re.search(r"MMSI:\s*(00\d{7})\b", t) for t in lines[i + 1:i + 5]) if x), None)
        name = m[1].strip()
        if p and mm and mm not in seen:
            seen.add(mm); out.append((name, p[0], p[1], 'S', mm, '', ''))
    return out

def main():
    ap = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    ap.add_argument('pdf'); ap.add_argument('-o', '--out', default='costeras.txt')
    a = ap.parse_args()
    text = subprocess.run(['pdftotext', '-layout', a.pdf, '-'], capture_output=True, check=True).stdout.decode('utf-8', 'replace')
    lines = text.split('\n')
    rows = vhf(lines) + centres(lines)
    if not rows: sys.exit('No he encontrado ninguna estación: ¿es el libro de Radioseñales?')
    with open(a.out, 'w', encoding='utf-8') as f:
        f.write('# Lista propia, sacada del libro de Radioseñales: no la publique. nombre;lat;lon;tipo;MMSI;canal;canal 2;kHz\n')
        for n, la, lo, k, mmsi, c1, c2 in rows:
            f.write(f'{n[:31]};{la:.5f};{lo:.5f};{k};{mmsi};{c1};{c2};\n')
    print(f'{a.out}: {sum(1 for r in rows if r[3] == "V")} costeras de VHF, {sum(1 for r in rows if r[3] == "S")} centros de salvamento', file=sys.stderr)

if __name__ == '__main__':
    main()
