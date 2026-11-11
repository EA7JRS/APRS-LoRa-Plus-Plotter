/*
 * Copyright (C) 2026 Manuel - EA7JRS
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Ajustes que se guardan entre arranques (memoria NVS del ESP32).
 */
#pragma once
#include <Arduino.h>

namespace settings {
struct Values {
    bool simulator = true;          // datos de prueba mientras no haya nada conectado
    uint32_t serialBaud = 38400;    // RS485: 38400 (AIS) o 4800 (NMEA clasico)
    int brightness = 100;           // luz de fondo, 10..100
};
Values& get();
void load();
void save();
}
