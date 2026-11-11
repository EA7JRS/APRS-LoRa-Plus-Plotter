/* Copyright (C) 2026 Manuel - EA7JRS
 *
 * This file is part of APRS LoRa+ Plotter.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
// Sustituto minimo de Arduino.h para comprobar en el PC que el codigo de la interfaz compila contra LVGL
#pragma once
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#define SERIAL_8N1 0
#define SET_LOOP_TASK_STACK_SIZE(x)
uint32_t hostMillis(); inline uint32_t millis() { return hostMillis(); }
inline void delay(uint32_t) {}
inline unsigned uxTaskGetStackHighWaterMark(void*) { return 0; }
struct String {
    std::string s;
    String(const char* c = "") : s(c) {}
    void replace(const char* a, const char* b) { size_t p; while ((p = s.find(a)) != std::string::npos) s.replace(p, strlen(a), b); }
    const char* c_str() const { return s.c_str(); }
};
struct HardwareSerial {
    void begin(unsigned long, int = 0, int = -1, int = -1) {}
    void setRxBufferSize(size_t) {}
    void updateBaudRate(unsigned long) {}
    int available() { return 0; }
    int read() { return -1; }
    template <typename... A> void printf(const char*, A...) {}
    void println(const char*) {}
};
extern HardwareSerial Serial, Serial1;
