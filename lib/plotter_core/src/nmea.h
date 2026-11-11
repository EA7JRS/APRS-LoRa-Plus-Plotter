/* Copyright (C) 2026 Manuel - EA7JRS
 *
 * This file is part of APRS LoRa+ Plotter.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
// NMEA 0183: comprobacion, campos y las tramas de la fase 1 (RMC, GLL, VTG, DPT, DBT, MWV, HDG, HDT, HDM, VHW, MTW, VDM, VDO).
#ifndef PLOTTER_NMEA_H_
#define PLOTTER_NMEA_H_

#include <cstdint>

#include "ais.h"
#include "store.h"

namespace plotter {

uint8_t nmeaChecksum(const char* body);         // XOR de lo que va entre '$' o '!' y '*'
bool nmeaChecksumOk(const char* line);          // false tambien si falta la suma

enum class NmeaResult { Bad, Ignored, Data, Ais };

class Nmea0183 {
   public:
    // Procesa una linea. Los datos van a "store" con la fuente indicada; un mensaje AIS completo queda en ais().
    NmeaResult feed(const char* line, Source src, uint32_t nowMs, DataStore& store);
    const AisReport& ais() const { return ais_; }

   private:
    AisDecoder dec_;
    AisReport ais_;
};

}  // namespace plotter
#endif
