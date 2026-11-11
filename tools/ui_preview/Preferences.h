/* Copyright (C) 2026 Manuel - EA7JRS
 *
 * This file is part of APRS LoRa+ Plotter.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
// Sustituto de Preferences.h para el PC: no guarda nada, devuelve siempre el valor por defecto.
#pragma once
#include <cstdint>
struct Preferences {
    void begin(const char*, bool) {}
    void end() {}
    bool getBool(const char*, bool d) { return d; }
    uint32_t getUInt(const char*, uint32_t d) { return d; }
    int getInt(const char*, int d) { return d; }
    void putBool(const char*, bool) {}
    void putUInt(const char*, uint32_t) {}
    void putInt(const char*, int) {}
};
