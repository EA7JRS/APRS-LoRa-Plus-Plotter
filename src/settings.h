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
    bool anchorOn = false;          // fondeo vigilado: sigue tras un reinicio
    double anchorLat = 0, anchorLon = 0;
    int anchorRadius = 40;          // radio de alarma, m
    int anchorRode = 30;            // cadena filada, m
};
Values& get();
void load();
void save();
}
