/*
 * Copyright (C) 2026 Manuel - EA7JRS
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
// Entradas y salidas del instrumento de viento: veleta por el RS485 (NASA Target 2 Wind, MWV a 4800), NMEA 2000
// (escucha velocidad y rumbo del barco para el viento real, y emite el viento con direccion reclamada), simulador, y
// salida del viento al plotter por Bluetooth, WiFi (UDP) y NMEA 2000.
#pragma once
#include <Arduino.h>

#include "store.h"

namespace wlinks {
struct Status {
    bool simulator = true, can = false, canOut = false;
    uint32_t vaneLines = 0, vaneLastMs = 0;      // sentencias de viento recibidas por el RS485
    uint32_t serialBytes = 0;                    // bytes recibidos, valgan o no (para ver si los pines estan al reves)
    uint32_t canFrames = 0, canLastMs = 0;
    uint32_t canSent = 0, canErrors = 0;
    int canAddress = -1;                         // -1 sin reclamar, 254 sin direccion libre
};
void begin(plotter::DataStore* store);
void loop();
void setSimulator(bool on);
void setCan(bool listen, bool transmit);
void applySerialPins();                          // tras cambiar rs485Swap
const Status& status();
// Viento de ahora: el aparente de la veleta y el real calculado con la velocidad y el rumbo del barco
struct Wind { bool app = false, tru = false, drift = false; double awa = 0, aws = 0, twa = 0, tws = 0, twd = 0, driftKn = 0; };
Wind wind();
}
