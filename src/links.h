/*
 * Copyright (C) 2026 Manuel - EA7JRS
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Entradas de datos del plotter. Fase 1: simulador y puerto serie (RS485 de la placa) con NMEA 0183 / AIS.
 * Lo siguiente (NMEA 2000 por CAN, TCP y UDP por WiFi) entra por aqui mismo.
 */
#pragma once
#include <Arduino.h>

#include "ais.h"
#include "nmea.h"
#include "store.h"

namespace links {

struct Status {
    bool simulator = true;
    uint32_t serialBaud = 38400;
    uint32_t serialLines = 0;           // lineas recibidas por el puerto serie
    uint32_t serialLastMs = 0;          // millis() de la ultima
    uint32_t aisMessages = 0;
};

void begin(plotter::DataStore* store);
void loop();                            // llamar a menudo
void setSimulator(bool on);
void setSerialBaud(uint32_t baud);      // 4800 (NMEA clasico) o 38400 (AIS)
const Status& status();
const plotter::NmeaStats& serialStats();
// Ultimo mensaje AIS recibido (para la lista de blancos, que llega en el paso siguiente)
const plotter::AisReport& lastAis();
}
