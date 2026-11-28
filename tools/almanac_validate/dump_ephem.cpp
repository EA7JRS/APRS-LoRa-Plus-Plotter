// Copyright (C) 2026 Manuel - EA7JRS
// SPDX-License-Identifier: GPL-3.0-or-later
#include "astro.h"
#include "ephem.h"
#include <cstdio>

using namespace plotter;

static void iso(double jd) {
    int y, m, d; double h;
    jdToUtc(jd, y, m, d, h);
    int hh = (int)h, mm = (int)((h - hh) * 60), ss = (int)(((h - hh) * 60 - mm) * 60 + 0.5);
    if (ss == 60) { ss = 0; mm++; } if (mm == 60) { mm = 0; hh++; }
    printf("%04d-%02d-%02dT%02d:%02d:%02dZ", y, m, d, hh, mm, ss);
}

int main() {
    double jd0 = julianDay(2026, 1, 1, 0.0);
    double jd1 = julianDay(2028, 1, 1, 0.0);

    // Fases principales
    for (int q = 0; q < 4; q++) {
        const char* nm[] = {"new", "firstq", "full", "lastq"};
        double jd = jd0;
        while (true) {
            jd = nextMoonPhase(jd, q);
            if (jd >= jd1) break;
            printf("PHASE %s ", nm[q]); iso(jd); printf("\n");
            jd += 1.0;
        }
    }
    // Estaciones
    { double jd = jd0; while (true) { int s; double j = nextSeason(jd, s);
        if (j >= jd1) break; const char* sn[] = {"mar","jun","sep","dec"};
        printf("SEASON %s ", sn[s]); iso(j); printf("\n"); jd = j + 1.0; } }
    // Perigeo / apogeo
    for (int p = 0; p < 2; p++) { double jd = jd0; while (true) { double dk;
        double j = nextMoonApsis(jd, p == 0, dk); if (j >= jd1) break;
        printf("MOONAPSIS %s ", p == 0 ? "perigee" : "apogee"); iso(j);
        printf(" %.1f\n", dk); jd = j + 1.0; } }
    // Perihelio / afelio
    for (int p = 0; p < 2; p++) { double jd = jd0; while (true) { double da;
        double j = nextEarthApsis(jd, p == 0, da); if (j >= jd1) break;
        printf("EARTHAPSIS %s ", p == 0 ? "perihelion" : "aphelion"); iso(j);
        printf(" %.8f\n", da); jd = j + 1.0; } }
    // Eclipses
    for (int l = 0; l < 2; l++) { double jd = jd0; while (true) {
        Eclipse e = nextEclipse(jd, l == 1, 14); if (e.kind == EclipseKind::None || e.jd >= jd1) break;
        printf("ECLIPSE %s %s ", l == 1 ? "lunar" : "solar", eclipseNameEn(e.kind));
        iso(e.jd); printf(" mag=%.4f\n", e.magnitude); jd = e.jd + 20.0; } }
    // Escalares en fechas de muestra
    double sjd[] = { julianDay(2026,1,3,12), julianDay(2026,4,15,0), julianDay(2026,7,4,12) };
    for (double jd : sjd) {
        printf("SCALAR "); iso(jd);
        printf(" sunAu=%.8f sunSd=%.4f moonSd=%.4f\n",
               sunDistanceAu(jd), sunAngularDiameterArcmin(jd),
               moonAngularDiameterArcmin(384400.0));  // a dist media, solo sanity
    }
    return 0;
}
