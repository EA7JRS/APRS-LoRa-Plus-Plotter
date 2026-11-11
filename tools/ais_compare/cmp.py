# Copyright (C) 2026 Manuel - EA7JRS
#
# This file is part of APRS LoRa+ Plotter.
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Compara lo que decodifica el nucleo (salida de dump) con la salida de referencia de gpsd (.log.chk).
# gpsd solo saca el mensaje 24 cuando tiene las partes A y B; las partes sueltas salen aqui como "no esta en gpsd".
import sys, json
dump, chk = sys.argv[1], sys.argv[2]
SUP = {1,2,3,4,5,9,12,14,18,19,21,24,27}
pos, stat, exp_n = {}, {}, 0
for l in open(chk, encoding='latin-1'):
    l = l.strip()
    if not l.startswith('{"class":"AIS"'): continue
    try: d = json.loads(l)
    except Exception: continue
    if d.get('type') not in SUP or d.get('mmsi', 0) == 0: continue
    exp_n += 1
    k = (d['type'], d['mmsi'])
    if 'lon' in d: pos.setdefault(k, []).append(d)
    else: stat.setdefault(k, []).append(d)
got = [l.rstrip('\n').split('|') for l in open(dump, encoding='latin-1')]
bad = 0; nok = 0
def rep(g, why):
    global bad
    bad += 1
    if bad <= 20: print(' ', why, g)
for g in got:
    t, m = int(g[0]), int(g[1]); k = (t, m)
    if t in (1,2,3,4,9,18,19,21,27):
        c = pos.get(k)
        if not c: rep(g, 'no esta en gpsd'); continue
        def same(d):
            if t == 27: lo, la = d['lon']*1000, d['lat']*1000
            else: lo, la = d['lon'], d['lat']
            na = (d['lon'], d['lat']) in ((181*600000, 91*600000), (181*600, 91*600)) or d['lon'] in (181*600000, 181*600) or d['lat'] in (91*600000, 91*600)
            if g[2] == '0': return na
            if int(g[3]) != lo or int(g[4]) != la: return False
            if t in (1,2,3,18,19):
                if (int(g[10]) if g[10] != '-1' else 1023) != d['speed']: return False
                if g[11] != '-1' and int(g[11]) != d['course']: return False
                if g[11] == '-1' and d['course'] < 3600: return False
            if t in (19, 21):
                nm = d.get('shipname', d.get('name', ''))
                if g[5] != nm.rstrip(): return False
            return True
        if any(same(d) for d in c): nok += 1
        else: rep(g, 'distinto: %s' % {x: c[0].get(x) for x in ('lon','lat','speed','course','shipname','name')})
    else:
        c = stat.get(k)
        if not c: rep(g, 'no esta en gpsd'); continue
        def same(d):
            if t in (12, 14): return g[12] == d.get('text', '').rstrip()
            if g[5] and g[5] != d.get('shipname', '').rstrip(): return False
            if g[6] and g[6] != d.get('callsign', '').rstrip(): return False
            if g[7] != '-1' and int(g[7]) != d.get('shiptype', -2): return False
            if g[8] != '-1' and (int(g[8]) != d.get('to_bow', -2) or int(g[9]) != d.get('to_stern', -2)): return False
            return True
        if any(same(d) for d in c): nok += 1
        else: rep(g, 'distinto: %s' % {x: c[0].get(x) for x in ('shipname','callsign','shiptype','to_bow','to_stern','text')})
print('%d mensajes nuestros, %d iguales a gpsd, %d diferencias (gpsd: %d salidas)' % (len(got), nok, bad, exp_n))
