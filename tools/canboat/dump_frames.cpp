// Copyright (C) 2026 Manuel - EA7JRS — SPDX-License-Identifier: GPL-3.0-or-later
// Para tools/canboat/run.sh: sin argumentos escribe las tramas NMEA 2000 de nuestros codificadores (n2ktx) con los valores
// que deben llevar; con "dec <pgn> <hex>" pasa un mensaje al lector (n2k) y escribe los datos de motor que ha leido.
#include "n2k.h"
#include "n2ktx.h"
#include "store.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
using namespace plotter;

static void hex(const uint8_t* p, int n) { for (int i = 0; i < n; i++) printf("%02x", p[i]); }

static int enc() {
    uint8_t b[8], f[34];
    n2kWind(7, 14.0, 325.0, 2, b);                        // 14 kn = 7,2022 m/s; 325 grados = 5,6723 rad; aparente
    printf("{\"pgn\":130306,\"hex\":\""); hex(b, 8);
    printf("\",\"expect\":{\"SID\":7,\"Wind Speed\":7.2022,\"Wind Angle\":5.6723,\"Reference\":2}}\n");
    n2kXte(3, -42.37, false, b);
    printf("{\"pgn\":129283,\"hex\":\""); hex(b, 8);
    printf("\",\"expect\":{\"SID\":3,\"XTE mode\":0,\"Navigation Terminated\":0,\"XTE\":-42.37}}\n");
    N2kNav n;
    n.distanceM = 18520.0; n.bearingOrigToDestRad = 1.2345; n.bearingPosToDestRad = 1.2001; n.originWp = 4; n.destWp = 5;
    n.destLat = 36.5412345; n.destLon = -6.2987654; n.closingVelMs = 2.57; n.arrivalEntered = true; n.calcGreatCircle = false;
    n2kNavData(9, n, f);
    printf("{\"pgn\":129284,\"hex\":\""); hex(f, 34);
    printf("\",\"expect\":{\"SID\":9,\"Distance to Waypoint\":18520,\"Course/Bearing reference\":0,\"Perpendicular Crossed\":0,"
           "\"Arrival Circle Entered\":1,\"Calculation Type\":1,\"Bearing, Origin to Destination Waypoint\":1.2345,"
           "\"Bearing, Position to Destination Waypoint\":1.2001,\"Origin Waypoint Number\":4,\"Destination Waypoint Number\":5,"
           "\"Destination Latitude\":36.5412345,\"Destination Longitude\":-6.2987654,\"Waypoint Closing Velocity\":2.57}}\n");
    return 0;
}

int main(int argc, char** argv) {
    if (argc == 1) return enc();
    if (argc == 4 && !strcmp(argv[1], "dec")) {
        uint8_t d[64] = {}; int n = 0;
        for (const char* p = argv[3]; p[0] && p[1] && n < 64; p += 2) { char t[3] = {p[0], p[1], 0}; d[n++] = (uint8_t)strtoul(t, nullptr, 16); }
        N2kReader r; DataStore st;
        const bool ok = r.feedMessage((uint32_t)strtoul(argv[2], nullptr, 10), d, n, Source::Can, 1000, st) == N2kResult::Data;
        static const struct { Key k; const char* name; } K[] = {
            {Key::Rpm, "rpm"}, {Key::Boost, "boostKpa"}, {Key::OilPress, "oilPressKpa"}, {Key::OilTemp, "oilTempC"},
            {Key::CoolantTemp, "coolantC"}, {Key::FuelRate, "fuelRateLh"}, {Key::EngineHours, "hours"}, {Key::EngineLoad, "load"},
            {Key::FuelLevel, "fuelPct"}, {Key::BatteryV, "volt"}};
        printf("{\"ok\":%s,\"v\":{", ok ? "true" : "false");
        bool first = true; double v;
        for (auto& e : K) if (st.get(e.k, 1000, v)) { printf("%s\"%s\":%.4f", first ? "" : ",", e.name, v); first = false; }
        printf("}}\n");
        return 0;
    }
    return 2;
}
