# Copyright (C) 2026 Manuel - EA7JRS
# SPDX-License-Identifier: GPL-3.0-or-later
# Parte de APRS LoRa+ Plotter: validacion del almanaque (ephem) contra astropy/ERFA. No se compila en la CI.
"""Patron de referencia del almanaque con astropy (ERFA builtin, sin descargas).
Fases de la Luna, estaciones, perigeo/apogeo y perihelio/afelio 2026-2027."""
import numpy as np
import astropy.units as u
from astropy.time import Time
from astropy.coordinates import get_sun, get_body, solar_system_ephemeris, GeocentricTrueEcliptic
solar_system_ephemeris.set('builtin')

def lons(times):
    """Longitudes eclipticas aparentes (deg) del Sol y la Luna en un array de tiempos."""
    fr = GeocentricTrueEcliptic(equinox=times)
    sl = get_sun(times).transform_to(fr).lon.deg
    ml = get_body('moon', times).transform_to(fr).lon.deg
    return sl, ml

def refine(f, t0, t1, iters=40):
    """Biseccion sobre f(Time)->valor con cambio de signo en [t0,t1] (dias julianos)."""
    a, b = t0, t1
    fa = f(a)
    for _ in range(iters):
        m = 0.5*(a+b)
        fm = f(m)
        if fa*fm <= 0: b = m
        else: a, fa = m, fm
    return 0.5*(a+b)

def wrap180(x): return (x+180.0)%360.0-180.0

T0, T1 = 2026.0, 2028.0
# rejilla fina de 2 h
jd0 = Time('2026-01-01T00:00:00', scale='utc').jd
jd1 = Time('2028-01-01T00:00:00', scale='utc').jd
grid = np.arange(jd0, jd1, 2.0/24.0)
tg = Time(grid, format='jd', scale='utc')
sl, ml = lons(tg)

def iso(jd):
    return Time(jd, format='jd', scale='utc').utc.isot[:19]+'Z'

out = []
# --- Estaciones: sun_lon cruza 0/90/180/270 ---
for target, name in [(0,'mar'),(90,'jun'),(180,'sep'),(270,'dec')]:
    d = wrap180(sl - target)
    for i in range(len(d)-1):
        if d[i] <= 0 < d[i+1] and abs(d[i+1]-d[i])<10:
            f = lambda jd: wrap180(lons(Time(jd,format='jd',scale='utc'))[0]-target)
            j = refine(f, grid[i], grid[i+1])
            out.append(('SEASON',name,j,None))

# --- Fases: (moon_lon - sun_lon) cruza 0/90/180/270 ---
elong = (ml - sl)%360.0
for target, name in [(0,'new'),(90,'firstq'),(180,'full'),(270,'lastq')]:
    d = wrap180(elong - target)
    for i in range(len(d)-1):
        if d[i] <= 0 < d[i+1] and abs(d[i+1]-d[i])<30:
            def f(jd,target=target):
                s,m = lons(Time(jd,format='jd',scale='utc'))
                return wrap180((m-s)-target)
            j = refine(f, grid[i], grid[i+1])
            out.append(('PHASE',name,j,None))

# --- Perigeo/apogeo: extremos de la distancia Tierra-Luna ---
md = get_body('moon', tg).distance.to(u.km).value
for i in range(1,len(md)-1):
    if md[i]<md[i-1] and md[i]<=md[i+1]:
        # minimo local -> perigeo; refina con parabola
        a,b,c = grid[i-1],grid[i],grid[i+1]; fa,fb,fc=md[i-1],md[i],md[i+1]
        denom=(fa-2*fb+fc); j=b+0.5*(fa-fc)/denom*(b-a) if denom!=0 else b
        dk = get_body('moon', Time(j,format='jd',scale='utc')).distance.to(u.km).value
        out.append(('MOONAPSIS','perigee',j,dk))
    if md[i]>md[i-1] and md[i]>=md[i+1]:
        a,b,c = grid[i-1],grid[i],grid[i+1]; fa,fb,fc=md[i-1],md[i],md[i+1]
        denom=(fa-2*fb+fc); j=b+0.5*(fa-fc)/denom*(b-a) if denom!=0 else b
        dk = get_body('moon', Time(j,format='jd',scale='utc')).distance.to(u.km).value
        out.append(('MOONAPSIS','apogee',j,dk))

# --- Perihelio/afelio: extremos de la distancia Tierra-Sol (rejilla diaria) ---
dgrid = np.arange(jd0, jd1, 1.0)
td = Time(dgrid, format='jd', scale='utc')
sd = get_sun(td).distance.to(u.au).value
for i in range(1,len(sd)-1):
    if sd[i]<sd[i-1] and sd[i]<=sd[i+1]:
        a,b,c=dgrid[i-1],dgrid[i],dgrid[i+1]; fa,fb,fc=sd[i-1],sd[i],sd[i+1]
        denom=(fa-2*fb+fc); j=b+0.5*(fa-fc)/denom*(b-a) if denom!=0 else b
        da=get_sun(Time(j,format='jd',scale='utc')).distance.to(u.au).value
        out.append(('EARTHAPSIS','perihelion',j,da))
    if sd[i]>sd[i-1] and sd[i]>=sd[i+1]:
        a,b,c=dgrid[i-1],dgrid[i],dgrid[i+1]; fa,fb,fc=sd[i-1],sd[i],sd[i+1]
        denom=(fa-2*fb+fc); j=b+0.5*(fa-fc)/denom*(b-a) if denom!=0 else b
        da=get_sun(Time(j,format='jd',scale='utc')).distance.to(u.au).value
        out.append(('EARTHAPSIS','aphelion',j,da))

for kind,name,j,val in sorted(out, key=lambda r:r[2]):
    extra = '' if val is None else ' %.4f'%val
    print(kind, name, iso(j)+extra)
