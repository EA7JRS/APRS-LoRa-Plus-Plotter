/* Copyright (C) 2026 Manuel - EA7JRS
 *
 * This file is part of APRS LoRa+ Plotter.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "coast.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "geo.h"

namespace plotter {

static bool number(const std::string& s, double& v) {
    if (s.empty()) return false;
    char* end = nullptr;
    v = std::strtod(s.c_str(), &end);
    return end != s.c_str() && *end == 0;
}

bool parseCoastRecord(const char* line, CoastStation& out) {
    if (!line) return false;
    std::string f[8];
    int n = 0;
    for (const char* p = line; *p && *p != '\r' && *p != '\n'; p++) {
        if (*p == ';') { if (++n >= 8) return false; }
        else f[n] += *p;
    }
    if (n < 3 || f[0].empty() || f[0][0] == '#') return false;
    CoastStation s;
    std::snprintf(s.name, sizeof(s.name), "%s", f[0].c_str());
    if (!number(f[1], s.lat) || !number(f[2], s.lon) || s.lat < -90 || s.lat > 90 || s.lon < -180 || s.lon > 180) return false;
    if (f[3].size() != 1 || !std::strchr("VMS", f[3][0])) return false;
    s.kind = f[3][0];
    double v;
    if (!f[4].empty()) {
        if (f[4].size() != 9 || !number(f[4], v)) return false;
        s.mmsi = (uint32_t)v;
    }
    if (!f[5].empty()) { if (!number(f[5], v) || v < 1 || v > 88) return false; s.channel = (int)v; }
    if (!f[6].empty()) { if (!number(f[6], v) || v < 1 || v > 88) return false; s.channel2 = (int)v; }
    if (!f[7].empty()) { if (!number(f[7], v) || v < 400 || v > 30000) return false; s.khz = v; }
    out = s;
    return true;
}

int formatCoastRecord(char* out, int size, const CoastStation& s) {
    char mmsi[12] = "", c1[8] = "", c2[8] = "", k[16] = "";
    if (s.mmsi) std::snprintf(mmsi, sizeof(mmsi), "%09lu", (unsigned long)s.mmsi);
    if (s.channel) std::snprintf(c1, sizeof(c1), "%d", s.channel);
    if (s.channel2) std::snprintf(c2, sizeof(c2), "%d", s.channel2);
    if (s.khz > 0) std::snprintf(k, sizeof(k), "%.1f", s.khz);
    const int n = std::snprintf(out, size, "%s;%.5f;%.5f;%c;%s;%s;%s;%s", s.name, s.lat, s.lon, s.kind, mmsi, c1, c2, k);
    return n > 0 && n < size ? n : 0;
}

int nearestCoast(const CoastStation* list, int n, char kind, double lat, double lon, double& distNm, double& bearingDeg) {
    int best = -1;
    double bd = 0;
    for (int i = 0; i < n; i++) {
        if (list[i].kind != kind) continue;
        const double d = gcDistanceNm({lat, lon}, {list[i].lat, list[i].lon});
        if (best < 0 || d < bd) { best = i; bd = d; }
    }
    if (best < 0) return -1;
    distNm = bd;
    bearingDeg = gcInitialBearing({lat, lon}, {list[best].lat, list[best].lon});
    return best;
}

std::string coastLine(const CoastStation& s, double distNm, double rangeNm, bool english) {
    char b[64];
    std::string t = s.name;
    std::snprintf(b, sizeof(b), english ? ", %.0f nm" : " a %.0f mn", distNm);
    t += b;
    if (rangeNm > 0 && distNm > rangeNm) t += english ? " (out of range)" : " (fuera de alcance)";
    if (s.channel) {
        std::snprintf(b, sizeof(b), english ? " \xC2\xB7 channel %d" : " \xC2\xB7 canal %d", s.channel);
        t += b;
        if (s.channel2) { std::snprintf(b, sizeof(b), english ? " (rescue service %d)" : " (Salvamento %d)", s.channel2); t += b; }
    }
    if (s.khz > 0) { std::snprintf(b, sizeof(b), " \xC2\xB7 %.1f kHz", s.khz); t += b; }
    if (s.mmsi) { std::snprintf(b, sizeof(b), english ? " \xC2\xB7 DSC %09lu" : " \xC2\xB7 LSD %09lu", (unsigned long)s.mmsi); t += b; }
    return t;
}

}  // namespace plotter
