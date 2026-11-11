/* Copyright (C) 2026 Manuel - EA7JRS
 *
 * This file is part of APRS LoRa+ Plotter.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
// Pasa ficheros enteros (texto o binario) por el troceador de lineas y el lector NMEA, para comprobar que nada lo rompe.
// Pensado para las grabaciones de receptores reales de gpsd (test/daemon/*.log). Compilar con -fsanitize=address,undefined.
#include <cstdio>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include "nmea.h"
#include "stream.h"
using namespace plotter;

int main(int argc, char** argv) {
    unsigned long files = 0, lines = 0, data = 0, ais = 0, bad = 0, ignored = 0, maxSats = 0;
    NmeaStats tot;
    for (int a = 1; a < argc; a++) {
        std::ifstream in(argv[a], std::ios::binary);
        if (!in) continue;
        std::vector<char> buf((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        Nmea0183 nm;
        DataStore st;
        LineBuffer lb;
        uint32_t now = 0;
        files++;
        for (char c : buf) {
            if (!lb.push(c)) continue;
            lines++;
            now += 50;
            switch (nm.feed(lb.line(), Source::Serial, now, st)) {
                case NmeaResult::Data: data++; break;
                case NmeaResult::Ais: ais++; break;
                case NmeaResult::Bad: bad++; break;
                default: ignored++; break;
            }
            if (unsigned(nm.sky().count()) > maxSats) maxSats = unsigned(nm.sky().count());
            nm.sky().expire(now);
        }
        const NmeaStats& s = nm.stats();
        tot.good += s.good; tot.badChecksum += s.badChecksum; tot.malformed += s.malformed;
        tot.aisRejected += s.aisRejected; tot.aisUnsupported += s.aisUnsupported; tot.aisOversized += s.aisOversized;
    }
    std::printf("%lu ficheros, %lu lineas: %lu con datos, %lu AIS, %lu sin uso, %lu malas\n", files, lines, data, ais, ignored, bad);
    std::printf("tramas buenas %u, suma mal %u, mal formadas %u, AIS rechazados %u, AIS sin uso %u, AIS largos %u, max. satelites %lu\n",
                tot.good, tot.badChecksum, tot.malformed, tot.aisRejected, tot.aisUnsupported, tot.aisOversized, maxSats);
    return 0;
}
