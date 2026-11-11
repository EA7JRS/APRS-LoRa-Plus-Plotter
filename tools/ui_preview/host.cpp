/* Copyright (C) 2026 Manuel - EA7JRS
 *
 * This file is part of APRS LoRa+ Plotter.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
// Dibuja la interfaz del firmware en el PC: LVGL real, pantalla en memoria, reloj simulado. Salida: PPM.
#include <Arduino.h>
#include <lvgl.h>
#include <cstdio>
#include <vector>
#include "links.h"
#include "store.h"
#include "ui.h"

static uint32_t fakeMs = 0;
uint32_t hostMillis() { return fakeMs; }
HardwareSerial Serial, Serial1;
static const int W = 1024, H = 600;
static std::vector<uint16_t> fb(W * H);
static void flush(lv_display_t* d, const lv_area_t* a, uint8_t* px) {
    const uint16_t* s = (const uint16_t*)px;
    for (int y = a->y1; y <= a->y2; y++)
        for (int x = a->x1; x <= a->x2; x++) fb[y * W + x] = *s++;
    lv_display_flush_ready(d);
}
static uint32_t tick() { return fakeMs; }
int main(int argc, char** argv) {
    int tab = argc > 2 ? atoi(argv[2]) : 0;
    lv_init();
    lv_tick_set_cb(tick);
    static std::vector<uint8_t> buf(W * 60 * 2);
    lv_display_t* d = lv_display_create(W, H);
    lv_display_set_color_format(d, LV_COLOR_FORMAT_RGB565);
    lv_display_set_flush_cb(d, flush);
    lv_display_set_buffers(d, buf.data(), nullptr, buf.size(), LV_DISPLAY_RENDER_MODE_PARTIAL);
    static plotter::DataStore store;
    links::begin(&store);
    ui::begin(&store);
    int seconds = argc > 3 ? atoi(argv[3]) : 20;
    ui::showPage(tab);
    for (int i = 0; i < seconds * 200; i++) {    // reloj simulado, 5 ms por vuelta
        fakeMs += 5;
        links::loop();
        if (i % 60 == 0) ui::update();
        lv_timer_handler();
    }
    FILE* f = fopen(argc > 1 ? argv[1] : "out.ppm", "wb");
    fprintf(f, "P6\n%d %d\n255\n", W, H);
    for (uint16_t p : fb) {
        unsigned char rgb[3] = {(unsigned char)((p >> 11) * 255 / 31), (unsigned char)(((p >> 5) & 63) * 255 / 63), (unsigned char)((p & 31) * 255 / 31)};
        fwrite(rgb, 1, 3, f);
    }
    fclose(f);
    return 0;
}
