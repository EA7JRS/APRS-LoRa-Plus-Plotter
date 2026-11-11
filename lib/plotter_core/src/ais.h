/* Copyright (C) 2026 Manuel - EA7JRS
 *
 * This file is part of APRS LoRa+ Plotter.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
// Decodificador AIS (ITU-R M.1371): mensajes 1, 2, 3, 5, 18, 19 y 24 a partir de tramas VDM / VDO.
#ifndef PLOTTER_AIS_H_
#define PLOTTER_AIS_H_

#include <cstdint>

namespace plotter {

struct AisReport {
    uint32_t mmsi = 0;
    int type = 0;
    bool own = false;               // llego en VDO: es el barco propio
    bool hasPos = false;
    double lat = 0, lon = 0;
    bool hasSog = false, hasCog = false, hasHeading = false;
    double sogKn = 0, cogDeg = 0;
    int heading = 0;
    int navStatus = -1;             // -1 sin dato
    bool hasStatic = false;
    char name[21] = "";
    char callsign[8] = "";
    int shipType = -1;
};

class AisDecoder {
   public:
    // Campos de la trama: total de fragmentos, numero de fragmento, identificador de secuencia (-1 si vacio), carga y bits de relleno.
    // true cuando hay un mensaje completo y reconocido en "out".
    bool feed(int total, int num, int seqId, const char* payload, int fillBits, bool own, AisReport& out);

   private:
    bool decode(const uint8_t* six, int nbits, bool own, AisReport& out);
    static constexpr int kMaxChars = 180;       // tres fragmentos largos
    uint8_t buf_[kMaxChars];
    int len_ = 0, seq_ = -2, next_ = 1, total_ = 0;
};

}  // namespace plotter
#endif
