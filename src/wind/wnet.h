/*
 * Copyright (C) 2026 Manuel - EA7JRS
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
// WiFi del instrumento: se une a la red del barco y manda el viento en NMEA 0183 por UDP a toda la red (difusion), al
// puerto que escuche el plotter (Ajustes > Red > puerto UDP del plotter, 10110 por defecto aqui).
#pragma once
#include <Arduino.h>

namespace wnet {
struct Status { bool on = false, connected = false; uint32_t sent = 0; char ip[16] = ""; int rssi = 0; };
void apply();                    // tras cambiar los ajustes de WiFi
void loop();                     // reintenta la conexion
bool send(const char* text, int len);
const Status& status();
}
