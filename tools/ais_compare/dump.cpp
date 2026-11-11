/* Copyright (C) 2026 Manuel - EA7JRS
 *
 * This file is part of APRS LoRa+ Plotter.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
// Vuelca lo que decodifica el nucleo del plotter, una linea por mensaje AIS
#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <string>
#include "nmea.h"
using namespace plotter;
int main(int argc, char** argv) {
    if (argc < 2) return 2;
    std::ifstream in(argv[1]);
    std::string line;
    Nmea0183 nm;
    DataStore st;
    while (std::getline(in, line)) {
        while (!line.empty() && (line.back() == '\r' || line.back() == '\n')) line.pop_back();
        if (line.empty() || line[0] == '#') continue;
        if (nm.feed(line.c_str(), Source::Uart, 1, st) != NmeaResult::Ais) continue;
        const AisReport& a = nm.ais();
        long lon = a.hasPos ? std::lround(a.lon * 600000.0) : 0, lat = a.hasPos ? std::lround(a.lat * 600000.0) : 0;
        std::printf("%d|%u|%d|%ld|%ld|%s|%s|%d|%d|%d|%d|%d|%s\n", a.type, a.mmsi, a.hasPos, lon, lat, a.name, a.callsign, a.shipType,
                    a.hasDim ? a.toBow : -1, a.hasDim ? a.toStern : -1, a.hasSog ? int(std::lround(a.sogKn * 10)) : -1,
                    a.hasCog ? int(std::lround(a.cogDeg * 10)) : -1, a.text);
    }
    const NmeaStats& s = nm.stats();
    std::fprintf(stderr, "good %u badsum %u malformed %u aisRejected %u unsupported %u oversized %u\n", s.good, s.badChecksum, s.malformed,
                 s.aisRejected, s.aisUnsupported, s.aisOversized);
}
