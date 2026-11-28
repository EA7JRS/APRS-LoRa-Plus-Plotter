# Copyright (C) 2026 Manuel - EA7JRS
# SPDX-License-Identifier: GPL-3.0-or-later
# Parte de APRS LoRa+ Plotter: validacion del almanaque (ephem) contra astropy/ERFA. No se compila en la CI.
import sys, datetime as dt

def parse(path, kinds):
    ev=[]
    for ln in open(path):
        p=ln.split()
        if not p or p[0] not in kinds: continue
        k,name,ts=p[0],p[1],p[2]
        t=dt.datetime.strptime(ts[:19],'%Y-%m-%dT%H:%M:%S')
        val=None
        for tok in p[3:]:
            if tok.replace('.','').replace('-','').isdigit(): val=float(tok)
            if '=' in tok:
                try: val=float(tok.split('=')[1])
                except: pass
        ev.append((k,name,t,val))
    return ev

KINDS={'PHASE','SEASON','MOONAPSIS','EARTHAPSIS'}
ours=parse('/tmp/almanac/ours.txt',KINDS)
ref=parse('/tmp/almanac/ref.txt',KINDS)

from collections import defaultdict
byk=defaultdict(list)
for e in ref: byk[(e[0],e[1])].append(e)

rows=defaultdict(list)
for k,name,t,val in ours:
    cand=byk[(k,name)]
    if not cand: continue
    best=min(cand,key=lambda r:abs((r[2]-t).total_seconds()))
    ds=(t-best[2]).total_seconds()
    dv=None if (val is None or best[3] is None) else (val-best[3])
    rows[(k,name)].append((t,ds,dv))

print("evento            n   |Δt| máx   Δt medio   |Δdist| máx")
for key in sorted(rows):
    r=rows[key]
    adt=[abs(x[1]) for x in r]
    mx=max(adt); md=sorted([x[1] for x in r])[len(r)//2]
    dv=[abs(x[2]) for x in r if x[2] is not None]
    dvs = ("%.1f"%max(dv)) if dv else "-"
    unit = "km" if key[0]=='MOONAPSIS' else ("AU" if key[0]=='EARTHAPSIS' else "")
    print("%-16s %3d  %6.0f s   %+6.0f s   %s %s"%(key[0]+" "+key[1],len(r),mx,md,dvs,unit))

# worst phase/season offenders
allrows=[(k,n,t,ds) for (k,n),lst in rows.items() for (t,ds,dv) in lst if k in ('PHASE','SEASON')]
allrows.sort(key=lambda x:-abs(x[3]))
print("\npeores (fase/estación):")
for k,n,t,ds in allrows[:6]:
    print("  %-14s %s  Δ%+.0f s"%(k+" "+n,t.strftime('%Y-%m-%d %H:%M'),ds))
