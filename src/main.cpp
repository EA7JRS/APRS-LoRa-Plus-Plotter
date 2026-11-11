/*
 * Copyright (C) 2026 Manuel - EA7JRS
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * APRS LoRa+ Plotter: plotter nautico para la Waveshare ESP32-S3-Touch-LCD-7B.
 * Arranque de la placa y de LVGL como en APRS LoRa+ Display; los datos van por links (entradas) -> DataStore -> ui.
 */
#include <Arduino.h>
#include <lvgl.h>

#include "board.h"
#include "links.h"
#include "settings.h"
#include "store.h"
#include "ui.h"

// LVGL 9 dibuja cada nivel de objetos con dos funciones que se llaman entre si y no cabe en los 8 KB de serie de loop()
// (visto en la 7B con APRS LoRa+ Display): 32 KB de pila.
SET_LOOP_TASK_STACK_SIZE(32 * 1024);

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
    Serial.begin(115200);
    Serial.println("\n=== APRS LoRa+ Plotter " PLOTTER_VERSION " (" BOARD_ENV ") ===");
    bool ok = board::begin();
    bool sd = board::sdBegin();                 // cartas en /charts/<mapa>/<z>/<x>/<y>.png

    lv_init();
    lv_tick_set_cb(tickMs);

    // Dos buffers parciales (LVGL dibuja en uno mientras el otro va al panel)
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

    settings::load();
    links::begin(&store);
    links::setSimulator(settings::get().simulator);
    links::setSerialBaud(settings::get().serialBaud);
    links::setCan(settings::get().can);
    board::setBacklight(settings::get().brightness);
    ui::begin(&store);
    if (!ok) Serial.println("La placa no ha arrancado bien: revisar docs/HARDWARE.md");
    if (!sd) Serial.println("[main] sin tarjeta: la carta sale sin mapa de fondo");
}

// Por el puerto serie del USB (UART0): "APRS-INFO?" como el resto de dispositivos APRS LoRa+, "SIM 0" / "SIM 1" y "N2K <id> <bytes>".
static void console() {
    static char buf[48];
    static uint8_t n = 0;
    while (Serial.available()) {
        char c = (char)Serial.read();
        if (c == '\n' || c == '\r') {
            buf[n] = 0;
            if (n && strcmp(buf, "APRS-INFO?") == 0)
                Serial.println("APRS-LoRa-Plus: device=plotter board=" BOARD_ENV " version=" PLOTTER_VERSION " chip=ESP32-S3 ota=0");
            else if (n && (strcmp(buf, "SIM 0") == 0 || strcmp(buf, "SIM 1") == 0)) {
                settings::get().simulator = buf[4] == '1';
                links::setSimulator(settings::get().simulator);
                settings::save();
                Serial.printf("[main] simulador %s\n", buf[4] == '1' ? "encendido" : "apagado");
            }
            else if (n > 13 && strncmp(buf, "N2K ", 4) == 0) {      // "N2K 09F8012B 14A7B21F6C513C03": una trama como si llegara del bus
                char* end = nullptr;
                uint32_t id = strtoul(buf + 4, &end, 16);
                uint8_t d[8];
                int len = 0;
                while (end && *end == ' ') end++;
                while (end && len < 8 && isxdigit((unsigned char)end[0]) && isxdigit((unsigned char)end[1])) {
                    char h[3] = {end[0], end[1], 0};
                    d[len++] = (uint8_t)strtoul(h, nullptr, 16);
                    end += 2;
                }
                if (len) links::injectCanFrame(id, d, len);
                Serial.printf("[main] N2K %08X, %d bytes\n", (unsigned)id, len);
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
    links::loop();
    static uint32_t t = 0;
    if (millis() - t > 300) { t = millis(); ui::update(); }
    lv_timer_handler();
    static uint32_t ts = 0;
    if (millis() - ts > 60000) {
        ts = millis();
        const links::Status& ls = links::status();
        Serial.printf("[main] pila libre %u bytes; serie %u lineas, %u AIS\n", (unsigned)uxTaskGetStackHighWaterMark(nullptr),
                      (unsigned)ls.serialLines, (unsigned)ls.aisMessages);
    }
    delay(5);
}
