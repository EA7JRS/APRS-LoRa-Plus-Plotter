/*
 * Copyright (C) 2026 Manuel - EA7JRS
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
// APRS LoRa+ Wind: instrumento de viento para la Waveshare ESP32-S3-Touch-LCD-4, accesorio del plotter.
// Veleta (NASA Target 2 Wind) por el RS485 -> tabla de datos -> pantalla y, por Bluetooth, WiFi (UDP) y NMEA 2000, al plotter.
#include <Arduino.h>
#include <lvgl.h>

#include "board.h"
#include "store.h"
#include "wble.h"
#include "wlinks.h"
#include "wnet.h"
#include "wsettings.h"
#include "wui.h"

SET_LOOP_TASK_STACK_SIZE(32 * 1024);            // LVGL 9 no cabe en los 8 KB de serie de loop()

static plotter::DataStore store;
static lv_display_t* disp = nullptr;

static void drawDone() { lv_display_flush_ready(disp); }

static void flush(lv_display_t*, const lv_area_t* a, uint8_t* px) {
    if (board::swapBytes()) lv_draw_rgb565_swap(px, (uint32_t)lv_area_get_width(a) * lv_area_get_height(a));
    board::draw(a->x1, a->y1, a->x2, a->y2, px);
}

static void readTouch(lv_indev_t*, lv_indev_data_t* data) {
    static int lx = 0, ly = 0;
    static bool pressed = false;
    int x, y;
    int r = board::touch(x, y);
    if (r == 1) { lx = x; ly = y; pressed = true; }
    else if (r == 0) pressed = false;
    data->point.x = lx; data->point.y = ly;
    data->state = pressed ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
}

static uint32_t tickMs() { return millis(); }

void setup() {
    Serial.begin(115200);                       // USB nativo (el UART0 es el RS485 de la veleta)
    Serial.println("\n=== APRS LoRa+ Wind " PLOTTER_VERSION " (" BOARD_ENV ") ===");
    Serial.println("[INFO][Main] APRS LoRa+ (c) 2026 Manuel - EA7JRS");
    bool ok = board::begin();

    lv_init();
    lv_tick_set_cb(tickMs);
    const uint32_t bytes = (uint32_t)board::W * board::drawLines() * 2;
    void* b1 = board::allocDrawBuffer(bytes);
    void* b2 = board::allocDrawBuffer(bytes);
    if (!b1 || !b2) Serial.println("[main] sin memoria para los buffers de LVGL");
    disp = lv_display_create(board::W, board::H);
    board::setDrawDone(drawDone);
    lv_display_set_color_format(disp, LV_COLOR_FORMAT_RGB565);
    lv_display_set_flush_cb(disp, flush);
    lv_display_set_buffers(disp, b1, b2, bytes, LV_DISPLAY_RENDER_MODE_PARTIAL);

    lv_indev_t* indev = lv_indev_create();
    lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(indev, readTouch);

    wsettings::load();
    const wsettings::Values& v = wsettings::get();
    wlinks::begin(&store);
    wlinks::setSimulator(v.simulator);
    wlinks::setCan(v.can, v.canOut);
    wnet::apply();
    wble::set(v.bleOn);
    if (ok) board::setBacklight(v.brightness);
    wui::begin(&store);
    if (!ok) Serial.println("La placa no ha arrancado bien: revisar docs/INSTRUMENTO_VIENTO.md");
}

// Por el USB: "APRS-INFO?" como el resto de dispositivos APRS LoRa+, y "SIM 0" / "SIM 1"
static void console() {
    static char buf[32];
    static uint8_t n = 0;
    while (Serial.available()) {
        char c = (char)Serial.read();
        if (c == '\n' || c == '\r') {
            buf[n] = 0;
            if (n && strcmp(buf, "APRS-INFO?") == 0)
                Serial.println("APRS-LoRa-Plus: device=wind board=" BOARD_ENV " version=" PLOTTER_VERSION " chip=ESP32-S3 ota=0");
            else if (n && (strcmp(buf, "SIM 0") == 0 || strcmp(buf, "SIM 1") == 0)) {
                wsettings::get().simulator = buf[4] == '1';
                wlinks::setSimulator(wsettings::get().simulator);
                wsettings::save();
                Serial.printf("[main] simulador %s\n", buf[4] == '1' ? "encendido" : "apagado");
            }
            n = 0;
        } else if (n < sizeof(buf) - 1) {
            buf[n++] = c;
        } else {
            n = 0;
        }
    }
}

void loop() {
    console();
    wlinks::loop();
    static uint32_t t = 0;
    if (millis() - t > 300) { t = millis(); wui::update(); }
    wui::tick();
    lv_timer_handler();
    delay(5);
}
