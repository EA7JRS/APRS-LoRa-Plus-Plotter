/* Copyright (C) 2026 Manuel - EA7JRS
 *
 * This file is part of APRS LoRa+ Plotter.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
// Dibuja la interfaz del firmware en el PC: LVGL real, pantalla en memoria, reloj simulado. Salida: PPM.
#include <Arduino.h>
#include <lvgl.h>
#include <cstdio>
#include <string>
#include <vector>
#include "links.h"
#include "settings.h"
#include "store.h"
#include "ui.h"

#include "board.h"
namespace board {
void setBacklight(int) {}
}
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
    if (argc > 4) {                              // cuarto argumento: acciones separadas por comas antes de la foto
        // ct:<astro>:<desvio>:<paso> | tablilla | cerrar | mob | fondeo | red | acerca | barco | rs485 | internet | ajuste:n | marcar | tocar:x:y | boton:n | pagina:n | wp:n | rutas:n | esperar:s
        std::string acts = argv[4];
        size_t pos = 0;
        while (pos <= acts.size()) {
            size_t e = acts.find(',', pos);
            if (e == std::string::npos) e = acts.size();
            std::string a = acts.substr(pos, e - pos);
            pos = e + 1;
            int x = 0, y = 0;
            if (a == "cerrar") ui::closeWindow();
            else if (a == "mob") ui::testMob();
            else if (a == "fondeo") ui::testAnchor();
            else if (a == "red") ui::testNet();
            else if (a == "acerca") ui::testAbout();
            else if (a == "barco") ui::testVessel();
            else if (a.rfind("astro:", 0) == 0) { size_t c = a.find(':', 6); ui::testAstro(a.substr(6, c == std::string::npos ? c : c - 6).c_str(), c == std::string::npos ? 0.0 : atof(a.c_str() + c + 1)); }
            else if (a.rfind("astrolat:", 0) == 0) { size_t c = a.find(':', 9); ui::testAstro(a.substr(9, c == std::string::npos ? c : c - 9).c_str(), c == std::string::npos ? 0.0 : atof(a.c_str() + c + 1), true); }
            else if (a.rfind("corriente", 0) == 0) { int m = 0, mi = 90; sscanf(a.c_str(), "corriente:%d:%d", &m, &mi); ui::testCurrent(m, mi); }
            else if (a == "astrocerrar") ui::closeAstro();
            else if (a == "astrolista") ui::testAstroList();
            else if (a.rfind("ct:", 0) == 0) {           // ct:<estrella o vacio = Sol>:<desvio>:<paso de rumbos, 0 = uno solo>
                char st[32] = ""; double dv = 0; int stp = 0;
                sscanf(a.c_str() + 3, "%31[^:]:%lf:%d", st, &dv, &stp);
                if (a.size() > 3 && a[3] == ':') sscanf(a.c_str() + 4, "%lf:%d", &dv, &stp), st[0] = 0;
                ui::testCt(st, dv, stp);
            }
            else if (a == "tablilla") ui::testCtCard();
            else if (a == "navtex") ui::testNavtex(false);
            else if (a == "navtexlista") ui::testNavtex(true);
            else if (sscanf(a.c_str(), "luces:%d:%d", &x, &y) == 2) {   // luces:<zoom>:<luz elegida, -1 ninguna>; lista inventada
                FILE* lf = fopen("/tmp/plotter_sd/luces.txt", "w");
                if (lf) {
                    fputs("# Lista de prueba con luces inventadas\n"
                          "Faro de ejemplo;36.5400;-6.3000;Fl(2) W 10s;40;18\n"
                          "Dique de ejemplo. Extremo;36.5270;-6.2780;Fl R 5s;9;4\n"
                          "Contradique. Extremo;36.5255;-6.2745;Fl G 5s;8;3\n"
                          "Bajo de ejemplo. Cardinal N;36.5390;-6.2600;Q W;6;5\n"
                          "Boya especial;36.5200;-6.3200;Fl(4) Y 11s;;3\n"
                          "Faro lejano;36.6200;-6.4500;Oc(3) W 12s;60;22\n", lf);
                    fclose(lf);
                }
                ui::testLights(x, y);
            }
            else if (a == "dsc") ui::testDsc(false);
            else if (a == "dsclista") ui::testDsc(true);
            else if (a == "rs485") ui::testRs485();
            else if (sscanf(a.c_str(), "piloto:%d", &x) == 1) settings::get().pilotOut = x;   // 1 RS485, 2 red, 3 Bluetooth
            else if (a == "internet") ui::testInternet();
            else if (sscanf(a.c_str(), "ajuste:%d", &x) == 1) ui::testSetting(x);
            else if (a == "marcar") ui::testMark();
            else if (sscanf(a.c_str(), "tocar:%d:%d", &x, &y) == 2) ui::testTapChart(x, y);
            else if (sscanf(a.c_str(), "boton:%d", &x) == 1) ui::testPanelButton(x);
            else if (sscanf(a.c_str(), "pagina:%d", &x) == 1) ui::showPage(x);
            else if (sscanf(a.c_str(), "wp:%d", &x) == 1) ui::testSelectWaypoint(x);
            else if (sscanf(a.c_str(), "rutas:%d", &x) == 1) ui::testRoutes(x);
            else if (sscanf(a.c_str(), "esperar:%d", &x) == 1) {
                for (int i = 0; i < x * 200; i++) { fakeMs += 5; links::loop(); if (i % 60 == 0) ui::update(); lv_timer_handler(); }
            }
            for (int i = 0; i < 120; i++) { fakeMs += 5; links::loop(); if (i % 60 == 0) ui::update(); lv_timer_handler(); }
        }
        for (int i = 0; i < 100; i++) { fakeMs += 5; links::loop(); if (i % 60 == 0) ui::update(); lv_timer_handler(); }
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
