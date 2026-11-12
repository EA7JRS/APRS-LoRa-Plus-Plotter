/*
 * Copyright (C) 2026 Manuel - EA7JRS
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
// Bluetooth del instrumento: periferico con el servicio de datos NMEA de docs/BLE_NMEA.md. El plotter (central) lo busca,
// se conecta y se suscribe; el instrumento le notifica el viento en sentencias NMEA 0183, y el plotter le escribe rumbo
// y velocidad del barco (HDT, VTG, VHW) para el viento real.
#pragma once
#include <Arduino.h>

namespace wble {
struct Status { bool on = false, connected = false; uint32_t sent = 0, received = 0; };
void set(bool on);
// Manda texto NMEA (con su "\r\n") a quien este suscrito. false si no hay nadie.
bool send(const char* text, int len);
const Status& status();
// Lo que ha escrito el plotter (NMEA 0183: rumbo y velocidad del barco) desde la ultima llamada
int read(uint8_t* buf, int max);
}
