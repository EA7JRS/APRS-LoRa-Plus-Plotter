/*
 * Copyright (C) 2026 Manuel - EA7JRS
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Interfaz del instrumento de viento (480x480): Rosa (tres repartos), Viento, Numeros, Historial y Ajustes.
 * Diseno propio, con la paleta y las fuentes del plotter.
 */
#include "wui.h"

#include <Arduino.h>
#include <lvgl.h>

#include "board.h"
#include "geo.h"
#include "motion.h"
#include "wble.h"
#include "wind.h"
#include "wlinks.h"
#include "wnet.h"
#include "about_text.h"
#include "ui_assets.h"
#include "windalarm.h"
#include "wsettings.h"

LV_FONT_DECLARE(dejavu_12)
LV_FONT_DECLARE(dejavu_14)
LV_FONT_DECLARE(dejavu_16)
LV_FONT_DECLARE(dejavu_bold_16)
LV_FONT_DECLARE(dejavu_bold_20)
LV_FONT_DECLARE(dejavu_bold_28)
LV_FONT_DECLARE(dejavu_num_32)
LV_FONT_DECLARE(dejavu_num_44)
LV_FONT_DECLARE(dejavu_num_60)

namespace wui {

using namespace plotter;

// Paleta: la de dia y la de noche (rojos sobre negro, para no perder la vista nocturna). Al cambiar, se rehace todo.
static uint32_t C_BG, C_BAR, C_PANEL, C_LINE, C_TEXT, C_DIM, C_SOFT, C_ACC, C_APP, C_OK, C_PORT, C_OFF, C_BORDER, C_ALERT;
static void palette(bool night) {
    if (!night) {
        C_BG = 0x0B1622; C_BAR = 0x0F2033; C_PANEL = 0x13263A; C_LINE = 0x1E3A56; C_TEXT = 0xEAF2FA; C_DIM = 0x9DB2C7;
        C_SOFT = 0xB7C8D9; C_ACC = 0x4DA3FF; C_APP = 0xFF9F43; C_OK = 0x3FA65A; C_PORT = 0xD9413A; C_OFF = 0x5E7A96;
        C_BORDER = 0x2F5F88; C_ALERT = 0xE5484D;
    } else {
        C_BG = 0x000000; C_BAR = 0x0C0202; C_PANEL = 0x160404; C_LINE = 0x2C0909; C_TEXT = 0xE0342A; C_DIM = 0x8C1F18;
        C_SOFT = 0xB02A21; C_ACC = 0xC2345E; C_APP = 0xFF7A33; C_OK = 0x5A6A1E; C_PORT = 0x8F1D16; C_OFF = 0x3A0E0B;
        C_BORDER = 0x4A1210; C_ALERT = 0xFF3B30;
    }
}
static const int TOP_H = 36, NAV_H = 44, PAD = 8, BODY = 480 - TOP_H - NAV_H - 2 * PAD;      // 384
static const int kPages = 5;
static const char* const kTabs[kPages] = {"Rosa", "Viento", "N\xC3\xBAmeros", "Historial", "Ajustes"};
static const char* const kTitles[kPages] = {"Rosa de viento", "Viento", "N\xC3\xBAmeros", "Historial \xC2\xB7 30 min", "Ajustes y enlace"};

static DataStore* store = nullptr;
static lv_obj_t* pages[kPages] = {};
static lv_obj_t* tabBtn[kPages] = {};
static lv_obj_t* lblTitle = nullptr;
static lv_obj_t* dotVane = nullptr;
static lv_obj_t* dotBle = nullptr;
static lv_obj_t* dotCan = nullptr;
static lv_obj_t* dotWifi = nullptr;
static int active = 0;

// Datos que se pintan en varias paginas a la vez: cada uno, con todas sus etiquetas
enum Datum { D_AWA, D_AWS, D_TWA, D_TWS, D_TWD, D_DRIFT, D_COUNT };
static const int kMaxLbl = 6;
static lv_obj_t* valLbl[D_COUNT][kMaxLbl] = {};
static lv_obj_t* subLbl[D_COUNT][kMaxLbl] = {};
static int valN[D_COUNT] = {}, subN[D_COUNT] = {};

struct Dial {
    int size = 0, r = 0;
    lv_obj_t* lineA = nullptr;
    lv_obj_t* lineT = nullptr;
    lv_obj_t* markA = nullptr;
    lv_obj_t* markT = nullptr;
    lv_point_precise_t pa[2], pt[2], hull[8];
};
static Dial dials[4];
static lv_obj_t* roseStyle[3] = {};

// Historial: 30 min, una muestra cada 30 s
static const int kHist = 60;
static float histSpd[kHist], histDir[kHist];
static int histN = 0;
static uint32_t histMs = 0;
static lv_obj_t* chSpd = nullptr;
static lv_obj_t* chDir = nullptr;
static lv_chart_series_t* serSpd = nullptr;
static lv_chart_series_t* serDir = nullptr;
static lv_obj_t* lblSpdNow = nullptr;
static lv_obj_t* lblDirNow = nullptr;
static lv_obj_t* lblSpdFoot = nullptr;
static lv_obj_t* lblDirFoot = nullptr;
static lv_obj_t* lblSpdTitle = nullptr;

// Ajustes
static const int kRows = 4, kSet = 13;
static lv_obj_t* rowState[kRows] = {};
static lv_obj_t* rowLine[kRows] = {};
static lv_obj_t* setLbl[kSet] = {};
// Pantalla de WiFi (sobre todo lo demas)
static lv_obj_t* wifiScr = nullptr;
static lv_obj_t* wifiTa[3] = {};
static lv_obj_t* wifiKb = nullptr;
static lv_obj_t* wifiOnLbl = nullptr;
static bool wifiOnEdit = false;
// Alarma de viento
static WindAlarm alarm;
static lv_obj_t* alarmBox = nullptr;
static lv_obj_t* alarmTitle = nullptr;
static lv_obj_t* alarmText = nullptr;
static bool alarmShown = false;
static lv_obj_t* aboutScr = nullptr;
static void build();
static lv_obj_t* mainScr = nullptr;              // la pantalla principal (durante la bienvenida no es la activa)

static lv_obj_t* box(lv_obj_t* parent, uint32_t color, int radius = 0) {
    lv_obj_t* o = lv_obj_create(parent);
    lv_obj_remove_style_all(o);
    lv_obj_set_style_bg_color(o, lv_color_hex(color), 0);
    lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(o, radius, 0);
    lv_obj_set_scrollable(o, false);
    return o;
}

static lv_obj_t* label(lv_obj_t* parent, const char* text, const lv_font_t* font, uint32_t color) {
    lv_obj_t* l = lv_label_create(parent);
    lv_label_set_text(l, text);
    lv_obj_set_style_text_font(l, font, 0);
    lv_obj_set_style_text_color(l, lv_color_hex(color), 0);
    return l;
}

static void comma(char* b) {
    for (; *b; b++) if (*b == '.') *b = ',';
}

static lv_obj_t* regVal(Datum d, lv_obj_t* l) { if (valN[d] < kMaxLbl) valLbl[d][valN[d]++] = l; return l; }
static lv_obj_t* regSub(Datum d, lv_obj_t* l) { if (subN[d] < kMaxLbl) subLbl[d][subN[d]++] = l; return l; }
static uint32_t datumColor(Datum d) { return d == D_AWA ? C_APP : (d == D_TWA ? C_ACC : C_TEXT); }
static const char* const kName[D_COUNT] = {"APARENTE", "APARENTE", "REAL", "REAL", "DIRECCI\xC3\x93N", "DERIVA"};
static const char* const kUnit[D_COUNT] = {"AWA", "AWS kn", "TWA", "TWS kn", "TWD", "kn"};

// ---- Rosa: proa arriba, sectores de cenida, aguja A (aparente) y R (real) ----
static lv_obj_t* dialArc(lv_obj_t* parent, int r, int from, int to, uint32_t color, int width) {
    lv_obj_t* a = lv_arc_create(parent);
    lv_obj_remove_style_all(a);
    lv_obj_set_size(a, 2 * r, 2 * r);
    lv_obj_center(a);
    lv_arc_set_bg_angles(a, (lv_value_precise_t)((from + 270) % 360), (lv_value_precise_t)((to + 270) % 360));
    lv_obj_set_style_arc_color(a, lv_color_hex(color), 0);
    lv_obj_set_style_arc_width(a, width, 0);
    lv_obj_set_style_arc_opa(a, LV_OPA_COVER, 0);
    lv_obj_set_clickable(a, false);
    return a;
}

static void makeDial(Dial& d, lv_obj_t* parent, int size) {
    d.size = size;
    d.r = size / 2 - 2;
    bool big = size >= 260;
    lv_obj_t* root = box(parent, C_BG, LV_RADIUS_CIRCLE);
    lv_obj_set_size(root, size, size);
    lv_obj_center(root);
    lv_obj_set_style_border_color(root, lv_color_hex(C_BORDER), 0);
    lv_obj_set_style_border_width(root, 2, 0);
    lv_obj_set_clickable(root, false);
    int aw = big ? 12 : 9;
    dialArc(root, d.r - 6, 300, 340, C_PORT, aw);         // cenida por babor
    dialArc(root, d.r - 6, 20, 60, C_OK, aw);             // y por estribor
    const lv_font_t* f = big ? &dejavu_bold_16 : &dejavu_14;
    int lr = d.r - (big ? 38 : 30);
    for (int a = 0; a < 360; a += 30) {                   // 0 a 180 por cada banda
        double r = deg2rad((double)a);
        char t[8];
        snprintf(t, sizeof(t), "%d", a > 180 ? 360 - a : a);
        lv_obj_t* l = label(root, t, f, C_TEXT);
        lv_obj_align(l, LV_ALIGN_CENTER, (int)lround(sin(r) * lr), (int)lround(-cos(r) * lr));
    }
    {                                                     // el barco, visto desde arriba: proa arriba
        int c = size / 2 - 2, hw = size / 16, hl = size / 6;
        const double xs[8] = {0, 0.80, 1.0, 0.92, -0.92, -1.0, -0.80, 0}, ys[8] = {-1.0, -0.45, 0.15, 1.0, 1.0, 0.15, -0.45, -1.0};
        for (int i = 0; i < 8; i++) d.hull[i] = {(lv_value_precise_t)(c + xs[i] * hw), (lv_value_precise_t)(c + ys[i] * hl)};
        lv_obj_t* hull = lv_line_create(root);
        lv_line_set_points(hull, d.hull, 8);
        lv_obj_set_style_line_color(hull, lv_color_hex(C_SOFT), 0);
        lv_obj_set_style_line_width(hull, 2, 0);
        lv_obj_set_style_line_rounded(hull, true, 0);
    }
    d.lineT = lv_line_create(root);
    lv_obj_set_style_line_color(d.lineT, lv_color_hex(C_ACC), 0);
    lv_obj_set_style_line_width(d.lineT, big ? 5 : 4, 0);
    lv_obj_set_style_line_rounded(d.lineT, true, 0);
    d.lineA = lv_line_create(root);
    lv_obj_set_style_line_color(d.lineA, lv_color_hex(C_APP), 0);
    lv_obj_set_style_line_width(d.lineA, big ? 7 : 5, 0);
    lv_obj_set_style_line_rounded(d.lineA, true, 0);
    d.markT = label(root, "R", &dejavu_bold_16, C_ACC);
    d.markA = label(root, "A", &dejavu_bold_16, C_APP);
}

static void needle(Dial& d, lv_obj_t* line, lv_point_precise_t* p, lv_obj_t* mark, bool has, double angle) {
    lv_obj_set_hidden(line, !has);
    lv_obj_set_hidden(mark, !has);
    if (!has) return;
    double r = deg2rad(angle);
    double sx = sin(r), cy = -cos(r);
    int c = d.size / 2 - 2;                               // el borde de 2 px desplaza el origen de los hijos
    int outer = d.r - 4, inner = d.size / 5;
    p[0] = {(lv_value_precise_t)(c + sx * outer), (lv_value_precise_t)(c + cy * outer)};      // viene de fuera
    p[1] = {(lv_value_precise_t)(c + sx * inner), (lv_value_precise_t)(c + cy * inner)};
    lv_line_set_points(line, p, 2);
    // la letra, cerca del centro y a un lado de la aguja, lejos de los numeros de la esfera
    int mr = d.size / 5 + (d.size >= 260 ? 18 : 14);
    lv_obj_align(mark, LV_ALIGN_CENTER, (int)lround(sx * mr - cy * 15), (int)lround(cy * mr + sx * 15));
}

// ---- Casillas ----
// Nombre arriba a la izquierda, unidad arriba a la derecha, valor y linea de detalle
static lv_obj_t* tile(lv_obj_t* parent, Datum d, int x, int y, int w, int h, const lv_font_t* font, bool withSub) {
    lv_obj_t* t = box(parent, C_PANEL, 10);
    lv_obj_set_size(t, w, h);
    lv_obj_set_pos(t, x, y);
    lv_obj_set_style_pad_all(t, 8, 0);
    const char* name = (d == D_TWD && w < 140) ? "DIRECC." : kName[d];
    lv_obj_align(label(t, name, &dejavu_12, C_DIM), LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_align(label(t, kUnit[d], &dejavu_12, C_DIM), LV_ALIGN_TOP_RIGHT, 0, 0);
    lv_obj_t* v = regVal(d, label(t, "--", font, datumColor(d)));
    if (withSub) {
        lv_obj_align(v, LV_ALIGN_LEFT_MID, 0, 4);
        lv_obj_align(regSub(d, label(t, "", &dejavu_12, C_SOFT)), LV_ALIGN_BOTTOM_LEFT, 0, 0);
    } else {
        lv_obj_align(v, LV_ALIGN_BOTTOM_RIGHT, 0, 2);
    }
    return t;
}

// Celda de banda: etiqueta pequena y valor, centrados
static void bandCell(lv_obj_t* parent, Datum d, int x, int w, int h, const lv_font_t* font, const char* tag) {
    lv_obj_t* c = box(parent, C_PANEL, 0);
    lv_obj_set_size(c, w, h);
    lv_obj_set_pos(c, x, 0);
    lv_obj_align(label(c, tag, &dejavu_12, C_DIM), LV_ALIGN_TOP_MID, 0, 6);
    lv_obj_align(regVal(d, label(c, "--", font, datumColor(d))), LV_ALIGN_BOTTOM_MID, 0, -6);
}

static void makeRose(lv_obj_t* page) {
    for (int s = 0; s < 3; s++) {
        roseStyle[s] = box(page, C_BG, 0);
        lv_obj_set_size(roseStyle[s], 464, BODY);
        lv_obj_set_pos(roseStyle[s], 0, 0);
    }
    // 0: rosa grande y una banda con los cinco datos
    {
        lv_obj_t* p = box(roseStyle[0], C_PANEL, 10);
        lv_obj_set_size(p, 464, 312);
        makeDial(dials[0], p, 300);
        lv_obj_t* b = box(roseStyle[0], C_PANEL, 10);
        lv_obj_set_size(b, 464, 64);
        lv_obj_set_pos(b, 0, 320);
        static const Datum ds[5] = {D_AWA, D_AWS, D_TWA, D_TWS, D_TWD};
        static const char* const tg[5] = {"AWA", "AWS kn", "TWA", "TWS kn", "TWD"};
        for (int i = 0; i < 5; i++) bandCell(b, ds[i], 2 + i * 92, 92, 64, &dejavu_bold_28, tg[i]);
    }
    // 1: aparente arriba, rosa en medio, real abajo
    {
        lv_obj_t* a = box(roseStyle[1], C_PANEL, 10);
        lv_obj_set_size(a, 464, 76);
        bandCell(a, D_AWA, 6, 226, 76, &dejavu_num_44, "APARENTE \xC2\xB7 AWA");
        bandCell(a, D_AWS, 232, 226, 76, &dejavu_num_44, "APARENTE \xC2\xB7 kn");
        lv_obj_t* p = box(roseStyle[1], C_PANEL, 10);
        lv_obj_set_size(p, 464, 216);
        lv_obj_set_pos(p, 0, 84);
        makeDial(dials[1], p, 206);
        lv_obj_t* b = box(roseStyle[1], C_PANEL, 10);
        lv_obj_set_size(b, 464, 76);
        lv_obj_set_pos(b, 0, 308);
        bandCell(b, D_TWA, 6, 150, 76, &dejavu_num_32, "REAL \xC2\xB7 TWA");
        bandCell(b, D_TWS, 157, 150, 76, &dejavu_num_32, "REAL \xC2\xB7 kn");
        bandCell(b, D_TWD, 308, 150, 76, &dejavu_num_32, "DIRECCI\xC3\x93N");
    }
    // 2: rosa a la izquierda y los datos en columna
    {
        lv_obj_t* p = box(roseStyle[2], C_PANEL, 10);
        lv_obj_set_size(p, 300, BODY);
        makeDial(dials[2], p, 288);
        static const Datum ds[5] = {D_AWA, D_AWS, D_TWA, D_TWS, D_TWD};
        for (int i = 0; i < 5; i++) tile(roseStyle[2], ds[i], 308, (i * 157) / 2, 156, 70, &dejavu_num_32, false);
    }
}

static void makeWind(lv_obj_t* page) {
    lv_obj_t* p = box(page, C_PANEL, 10);
    lv_obj_set_size(p, 280, 280);
    makeDial(dials[3], p, 266);
    tile(page, D_AWA, 288, 0, 176, 136, &dejavu_num_44, true);
    tile(page, D_AWS, 288, 144, 176, 136, &dejavu_num_44, true);
    static const Datum ds[4] = {D_TWA, D_TWS, D_TWD, D_DRIFT};
    for (int i = 0; i < 4; i++) tile(page, ds[i], i * 118, 288, 110, 96, &dejavu_bold_28, true);
}

static void makeNumbers(lv_obj_t* page) {
    static const Datum ds[4] = {D_AWS, D_AWA, D_TWS, D_TWA};
    for (int i = 0; i < 4; i++) tile(page, ds[i], (i % 2) * 236, (i / 2) * 196, 228, 188, &dejavu_num_60, true);
}

static lv_obj_t* histChart(lv_obj_t* panel, uint32_t color, lv_chart_series_t*& ser) {
    lv_obj_t* c = lv_chart_create(panel);
    lv_obj_remove_style_all(c);
    lv_obj_set_size(c, 440, 116);
    lv_obj_align(c, LV_ALIGN_CENTER, 0, 4);
    lv_chart_set_type(c, LV_CHART_TYPE_LINE);
    lv_chart_set_point_count(c, kHist);
    lv_chart_set_div_line_count(c, 3, 0);
    lv_obj_set_style_line_color(c, lv_color_hex(C_LINE), LV_PART_MAIN);
    lv_obj_set_style_line_width(c, 1, LV_PART_MAIN);
    lv_obj_set_style_line_width(c, 3, LV_PART_ITEMS);
    lv_obj_set_style_size(c, 0, 0, LV_PART_INDICATOR);
    lv_obj_set_clickable(c, false);
    ser = lv_chart_add_series(c, lv_color_hex(color), LV_CHART_AXIS_PRIMARY_Y);
    return c;
}

static void makeHistory(lv_obj_t* page) {
    for (int i = 0; i < 2; i++) {
        lv_obj_t* p = box(page, C_PANEL, 10);
        lv_obj_set_size(p, 464, 188);
        lv_obj_set_pos(p, 0, i * 196);
        lv_obj_set_style_pad_all(p, 8, 0);
        lv_obj_t* t = label(p, i ? "DIRECCI\xC3\x93N REAL" : "VELOCIDAD \xC2\xB7 kn", &dejavu_12, C_DIM);
        lv_obj_align(t, LV_ALIGN_TOP_LEFT, 0, 0);
        lv_obj_t* now = label(p, "--", &dejavu_bold_20, C_TEXT);
        lv_obj_align(now, LV_ALIGN_TOP_RIGHT, 0, -2);
        lv_obj_t* foot = label(p, "", &dejavu_12, C_DIM);
        lv_obj_align(foot, LV_ALIGN_BOTTOM_LEFT, 0, 0);
        if (i) { chDir = histChart(p, C_APP, serDir); lblDirNow = now; lblDirFoot = foot; }
        else { chSpd = histChart(p, C_ACC, serSpd); lblSpdNow = now; lblSpdFoot = foot; lblSpdTitle = t; }
    }
}

static void refreshSettings();
static void openWifi();
static void onSetting(lv_event_t* e) {
    int i = (int)(intptr_t)lv_event_get_user_data(e);
    wsettings::Values& v = wsettings::get();
    switch (i) {
        case 0: v.roseStyle = (v.roseStyle + 1) % 3; break;
        case 1: v.simulator = !v.simulator; wlinks::setSimulator(v.simulator); break;
        case 2: v.brightness = v.brightness >= 100 ? 20 : v.brightness + 20; board::setBacklight(v.brightness); break;
        case 3: v.vaneOffset = v.vaneOffset >= 180 ? -175 : v.vaneOffset + 5; break;
        case 4: v.rs485Swap = !v.rs485Swap; wlinks::applySerialPins(); break;
        case 5: v.bleOn = !v.bleOn; wble::set(v.bleOn); break;
        case 6:                                                       // apagado -> escucha -> escucha y emite -> apagado
            if (!v.can) { v.can = true; v.canOut = false; }
            else if (!v.canOut) v.canOut = true;
            else { v.can = false; v.canOut = false; }
            wlinks::setCan(v.can, v.canOut);
            break;
        case 7: openWifi(); return;
        case 8: v.night = !v.night; wsettings::save(); lv_async_call([](void*) { build(); }, nullptr); return;
        case 9: v.twsMax = v.twsMax == 0 ? 15 : (v.twsMax >= 40 ? 0 : v.twsMax + 5); break;
        case 10: v.awsMax = v.awsMax == 0 ? 20 : (v.awsMax >= 45 ? 0 : v.awsMax + 5); break;
        case 12: lv_obj_set_hidden(aboutScr, false); return;
        case 11: v.shiftMax = v.shiftMax == 0 ? 10 : (v.shiftMax >= 45 ? 0 : v.shiftMax + (v.shiftMax < 20 ? 5 : 10)); break;
        default: break;
    }
    wsettings::save();
    refreshSettings();
}

static lv_obj_t* button(lv_obj_t* parent, int x, int y, int w, int h, lv_event_cb_t cb, intptr_t data, lv_obj_t** lbl) {
    lv_obj_t* b = lv_button_create(parent);
    lv_obj_remove_style_all(b);
    lv_obj_set_size(b, w, h);
    lv_obj_set_pos(b, x, y);
    lv_obj_set_style_bg_color(b, lv_color_hex(C_BAR), 0);
    lv_obj_set_style_bg_opa(b, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(b, lv_color_hex(C_LINE), LV_STATE_PRESSED);
    lv_obj_set_style_radius(b, 8, 0);
    lv_obj_set_style_border_color(b, lv_color_hex(C_BORDER), 0);
    lv_obj_set_style_border_width(b, 1, 0);
    lv_obj_add_event_cb(b, cb, LV_EVENT_CLICKED, (void*)data);
    *lbl = label(b, "", &dejavu_14, C_TEXT);
    lv_obj_center(*lbl);
    return b;
}

// La pagina se desplaza: cuatro enlaces arriba y ocho botones debajo
static void makeSettings(lv_obj_t* page) {
    lv_obj_set_scrollable(page, true);
    lv_obj_set_scroll_dir(page, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(page, LV_SCROLLBAR_MODE_ACTIVE);
    static const char* const names[kRows] = {"Veleta", "Bluetooth", "NMEA 2000", "WiFi"};
    for (int i = 0; i < kRows; i++) {
        lv_obj_t* r = box(page, C_PANEL, 10);
        lv_obj_set_size(r, 464, 54);
        lv_obj_set_pos(r, 0, i * 60);
        lv_obj_set_style_pad_hor(r, 12, 0);
        lv_obj_set_style_pad_ver(r, 7, 0);
        lv_obj_align(label(r, names[i], &dejavu_bold_16, C_TEXT), LV_ALIGN_TOP_LEFT, 0, 0);
        rowState[i] = label(r, "", &dejavu_14, C_DIM);
        lv_obj_align(rowState[i], LV_ALIGN_TOP_RIGHT, 0, 2);
        rowLine[i] = label(r, "", &dejavu_12, C_DIM);
        lv_obj_align(rowLine[i], LV_ALIGN_BOTTOM_LEFT, 0, 0);
    }
    for (int i = 0; i < kSet; i++) button(page, (i % 2) * 236, 244 + (i / 2) * 54, 228, 46, onSetting, i, &setLbl[i]);
}

static void refreshSettings() {
    const wsettings::Values& v = wsettings::get();
    static const char* const styles[3] = {"banda", "dos bandas", "columna"};
    char b[64];
    snprintf(b, sizeof(b), "Rosa: %s", styles[v.roseStyle]);
    lv_label_set_text(setLbl[0], b);
    lv_label_set_text(setLbl[1], v.simulator ? "Simulador: encendido" : "Simulador: apagado");
    snprintf(b, sizeof(b), "Brillo: %d %%", v.brightness);
    lv_label_set_text(setLbl[2], b);
    snprintf(b, sizeof(b), "Veleta: %+d\xC2\xB0", v.vaneOffset);
    lv_label_set_text(setLbl[3], b);
    snprintf(b, sizeof(b), "RS485: RX en GPIO%d", v.rs485Swap ? board::PIN_RS485_TX : board::PIN_RS485_RX);
    lv_label_set_text(setLbl[4], b);
    lv_label_set_text(setLbl[5], v.bleOn ? "Bluetooth: encendido" : "Bluetooth: apagado");
    lv_label_set_text(setLbl[6], !v.can ? "NMEA 2000: apagado" : (v.canOut ? "NMEA 2000: emite" : "NMEA 2000: escucha"));
    lv_label_set_text(setLbl[7], v.wifiOn && v.wifiSsid[0] ? "WiFi: encendido \xE2\x80\xA6" : "WiFi \xE2\x80\xA6");
    lv_label_set_text(setLbl[8], v.night ? "Noche: encendido" : "Noche: apagado");
    if (v.twsMax) snprintf(b, sizeof(b), "Alarma real: %d kn", v.twsMax); else snprintf(b, sizeof(b), "Alarma real: apagada");
    lv_label_set_text(setLbl[9], b);
    if (v.awsMax) snprintf(b, sizeof(b), "Alarma aparente: %d kn", v.awsMax); else snprintf(b, sizeof(b), "Alarma aparente: apagada");
    lv_label_set_text(setLbl[10], b);
    if (v.shiftMax) snprintf(b, sizeof(b), "Alarma rolada: %d\xC2\xB0", v.shiftMax); else snprintf(b, sizeof(b), "Alarma rolada: apagada");
    lv_label_set_text(setLbl[11], b);
    lv_label_set_text(setLbl[12], "Acerca de");
    WindAlarmConfig ac;
    ac.twsMaxKn = v.twsMax; ac.awsMaxKn = v.awsMax; ac.shiftDeg = v.shiftMax;
    alarm.configure(ac);
    for (int s = 0; s < 3; s++) lv_obj_set_hidden(roseStyle[s], s != v.roseStyle);
}

// ---- Pantalla de WiFi: red, clave y puerto UDP con el teclado de LVGL ----
static void closeWifi(bool save) {
    if (save) {
        wsettings::Values& v = wsettings::get();
        snprintf(v.wifiSsid, sizeof(v.wifiSsid), "%s", lv_textarea_get_text(wifiTa[0]));
        snprintf(v.wifiPass, sizeof(v.wifiPass), "%s", lv_textarea_get_text(wifiTa[1]));
        int port = atoi(lv_textarea_get_text(wifiTa[2]));
        v.udpPort = port >= 1 && port <= 65535 ? port : 10110;
        v.wifiOn = wifiOnEdit;
        wsettings::save();
        wnet::apply();
    }
    lv_obj_set_hidden(wifiScr, true);
    refreshSettings();
}
static void onWifiBtn(lv_event_t* e) {
    int i = (int)(intptr_t)lv_event_get_user_data(e);
    if (i == 0) {
        wifiOnEdit = !wifiOnEdit;
        lv_label_set_text(wifiOnLbl, wifiOnEdit ? "WiFi: encendido" : "WiFi: apagado");
    } else closeWifi(i == 2);
}
static void onWifiTa(lv_event_t* e) {
    lv_obj_t* ta = (lv_obj_t*)lv_event_get_target(e);
    lv_keyboard_set_textarea(wifiKb, ta);
    lv_keyboard_set_mode(wifiKb, ta == wifiTa[2] ? LV_KEYBOARD_MODE_NUMBER : LV_KEYBOARD_MODE_TEXT_LOWER);
}
static void makeWifi() {
    wifiScr = box(lv_layer_top(), C_BG, 0);
    lv_obj_set_size(wifiScr, 480, 480);
    lv_obj_t* bar = box(wifiScr, C_BAR, 0);
    lv_obj_set_size(bar, 480, TOP_H);
    lv_obj_align(label(bar, "WiFi \xC2\xB7 viento por UDP a la red", &dejavu_bold_16, C_TEXT), LV_ALIGN_LEFT_MID, 12, 0);
    static const char* const names[3] = {"Red", "Clave", "Puerto UDP"};
    for (int i = 0; i < 3; i++) {
        lv_obj_t* l = label(wifiScr, names[i], &dejavu_14, C_DIM);
        lv_obj_set_pos(l, 12, TOP_H + 18 + i * 50);
        lv_obj_t* ta = lv_textarea_create(wifiScr);
        lv_obj_set_size(ta, 340, 42);
        lv_obj_set_pos(ta, 128, TOP_H + 6 + i * 50);
        lv_textarea_set_one_line(ta, true);
        lv_obj_set_style_text_font(ta, &dejavu_16, 0);
        lv_obj_set_style_bg_color(ta, lv_color_hex(C_PANEL), 0);
        lv_obj_set_style_text_color(ta, lv_color_hex(C_TEXT), 0);
        lv_obj_set_style_border_color(ta, lv_color_hex(C_BORDER), 0);
        lv_obj_set_style_border_color(ta, lv_color_hex(C_ACC), LV_STATE_FOCUSED);
        lv_obj_add_event_cb(ta, onWifiTa, LV_EVENT_FOCUSED, nullptr);
        wifiTa[i] = ta;
    }
    lv_textarea_set_max_length(wifiTa[0], 32);
    lv_textarea_set_max_length(wifiTa[1], 64);
    lv_textarea_set_password_mode(wifiTa[1], true);
    lv_textarea_set_max_length(wifiTa[2], 5);
    lv_textarea_set_accepted_chars(wifiTa[2], "0123456789");
    static const char* const bt[3] = {"", "Cancelar", "Guardar"};
    lv_obj_t* lbl = nullptr;
    for (int i = 0; i < 3; i++) {
        button(wifiScr, 8 + i * 158, TOP_H + 160, 150, 44, onWifiBtn, i, &lbl);
        lv_label_set_text(lbl, bt[i]);
        if (i == 0) wifiOnLbl = lbl;
    }
    wifiKb = lv_keyboard_create(wifiScr);
    lv_obj_set_size(wifiKb, 480, 480 - (TOP_H + 214));
    lv_obj_align(wifiKb, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_color(wifiKb, lv_color_hex(C_BAR), 0);
    lv_obj_set_style_border_width(wifiKb, 0, 0);
    lv_obj_set_style_bg_color(wifiKb, lv_color_hex(C_PANEL), LV_PART_ITEMS);
    lv_obj_set_style_bg_color(wifiKb, lv_color_hex(C_LINE), LV_PART_ITEMS | LV_STATE_PRESSED);
    lv_obj_set_style_bg_color(wifiKb, lv_color_hex(C_LINE), LV_PART_ITEMS | LV_STATE_CHECKED);
    lv_obj_set_style_text_color(wifiKb, lv_color_hex(C_TEXT), LV_PART_ITEMS);
    lv_obj_set_style_text_color(wifiKb, lv_color_hex(C_TEXT), LV_PART_ITEMS | LV_STATE_CHECKED);
    lv_obj_set_style_text_color(wifiKb, lv_color_hex(C_TEXT), LV_PART_ITEMS | LV_STATE_DISABLED);
    lv_obj_set_style_border_width(wifiKb, 0, LV_PART_ITEMS);
    lv_obj_set_style_shadow_width(wifiKb, 0, LV_PART_ITEMS);
    lv_obj_set_hidden(wifiScr, true);
}
static void openWifi() {
    const wsettings::Values& v = wsettings::get();
    char b[8];
    lv_textarea_set_text(wifiTa[0], v.wifiSsid);
    lv_textarea_set_text(wifiTa[1], v.wifiPass);
    snprintf(b, sizeof(b), "%d", v.udpPort);
    lv_textarea_set_text(wifiTa[2], b);
    wifiOnEdit = v.wifiOn || !v.wifiSsid[0];                          // la primera vez, encendido
    lv_label_set_text(wifiOnLbl, wifiOnEdit ? "WiFi: encendido" : "WiFi: apagado");
    lv_keyboard_set_textarea(wifiKb, wifiTa[0]);
    lv_obj_add_state(wifiTa[0], LV_STATE_FOCUSED);
    lv_obj_set_hidden(wifiScr, false);
}

static void showTab(int i) {
    active = i;
    lv_label_set_text(lblTitle, kTitles[i]);
    for (int k = 0; k < kPages; k++) {
        lv_obj_set_hidden(pages[k], k != i);
        lv_obj_set_style_bg_color(tabBtn[k], lv_color_hex(k == i ? C_PANEL : C_BAR), 0);
        lv_obj_set_style_border_color(tabBtn[k], lv_color_hex(k == i ? C_ACC : C_BAR), 0);
        lv_obj_set_style_text_color(lv_obj_get_child(tabBtn[k], 0), lv_color_hex(k == i ? C_TEXT : C_SOFT), 0);
    }
}

static void onTab(lv_event_t* e) { showTab((int)(intptr_t)lv_event_get_user_data(e)); }

static lv_obj_t* statusDot(lv_obj_t* bar, const char* name, int rightX) {
    lv_obj_t* l = label(bar, name, &dejavu_12, C_SOFT);
    lv_obj_align(l, LV_ALIGN_RIGHT_MID, rightX, 0);
    lv_obj_t* d = box(bar, C_OFF, LV_RADIUS_CIRCLE);
    lv_obj_set_size(d, 8, 8);
    lv_obj_align_to(d, l, LV_ALIGN_OUT_LEFT_MID, -5, 0);
    return d;
}

// La ventana de alarma, sobre todo lo demas, como las del plotter
static void onAlarmBtn(lv_event_t* e) {
    uint32_t now = millis();
    if ((intptr_t)lv_event_get_user_data(e) == 0) alarm.silence(now);
    else alarm.dismiss(now);
    alarmShown = false;
    lv_obj_set_hidden(alarmBox, true);
    board::buzzer(false);
}
// "Acerca de": version, copyright, licencia, proyecto, donativo con QR (como en APRS LoRa+ Display) y creditos
static void onAboutClose(lv_event_t*) { lv_obj_set_hidden(aboutScr, true); }
static void makeAbout() {
    aboutScr = box(lv_layer_top(), C_BG, 0);
    lv_obj_set_size(aboutScr, 480, 480);
    lv_obj_set_clickable(aboutScr, true);
    lv_obj_t* bar = box(aboutScr, C_BAR, 0);
    lv_obj_set_size(bar, 480, TOP_H);
    lv_obj_align(label(bar, "Acerca de", &dejavu_bold_16, C_TEXT), LV_ALIGN_LEFT_MID, 12, 0);
    lv_obj_t* l = nullptr;
    button(aboutScr, 480 - 116, 3, 108, 30, onAboutClose, 0, &l);
    lv_label_set_text(l, "Cerrar");
    char v[64];
    snprintf(v, sizeof(v), "APRS LoRa+ Wind %s", PLOTTER_VERSION);
    lv_obj_set_pos(label(aboutScr, v, &dejavu_bold_20, C_TEXT), 14, TOP_H + 12);
    snprintf(v, sizeof(v), "Instrumento de viento \xC2\xB7 %s", BOARD_ENV);
    lv_obj_set_pos(label(aboutScr, v, &dejavu_12, C_SOFT), 14, TOP_H + 40);
    lv_obj_t* qw = about::donateQr(aboutScr, 112, C_BG);
    lv_obj_set_pos(qw, 14, TOP_H + 66);
    lv_obj_update_layout(qw);
    int tx = 14 + lv_obj_get_width(qw) + 14, tw = 480 - tx - 12;
    lv_obj_t* t = label(aboutScr, "APRS LoRa+\n\xC2\xA9 2026 Manuel - EA7JRS", &dejavu_bold_16, C_TEXT);
    lv_obj_set_width(t, tw);
    lv_obj_set_pos(t, tx, TOP_H + 66);
    t = label(aboutScr, about::kProject, &dejavu_14, C_ACC);
    lv_obj_set_pos(t, tx, TOP_H + 114);
    t = label(aboutScr, about::kDonateText, &dejavu_14, C_ACC);
    lv_obj_set_pos(t, tx, TOP_H + 136);
    t = label(aboutScr, about::kScan, &dejavu_12, C_DIM);
    lv_obj_set_pos(t, tx, TOP_H + 158);
    t = label(aboutScr, about::kLicense, &dejavu_12, C_SOFT);
    lv_obj_set_width(t, 452);
    lv_obj_set_pos(t, 14, TOP_H + 206);
    t = label(aboutScr, about::kCredits, &dejavu_12, C_DIM);
    lv_obj_set_width(t, 452);
    lv_obj_set_pos(t, 14, TOP_H + 252);
    t = label(aboutScr, "Viento: NASA Marine Target 2 Wind. Formatos NMEA 2000 contrastados con canboat.", &dejavu_12, C_DIM);
    lv_obj_set_width(t, 452);
    lv_obj_set_pos(t, 14, TOP_H + 322);
    lv_obj_set_hidden(aboutScr, true);
}

static void makeAlarm() {
    alarmBox = box(lv_layer_top(), C_PANEL, 14);
    lv_obj_set_size(alarmBox, 420, 250);
    lv_obj_center(alarmBox);
    lv_obj_set_style_border_color(alarmBox, lv_color_hex(C_ALERT), 0);
    lv_obj_set_style_border_width(alarmBox, 4, 0);
    alarmTitle = label(alarmBox, "", &dejavu_bold_28, C_ALERT);
    lv_obj_align(alarmTitle, LV_ALIGN_TOP_MID, 0, 22);
    alarmText = label(alarmBox, "", &dejavu_16, C_TEXT);
    lv_obj_set_width(alarmText, 380);
    lv_obj_set_style_text_align(alarmText, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(alarmText, LV_ALIGN_TOP_MID, 0, 80);
    static const char* const bt[2] = {"SILENCIAR 10 MIN", "CERRAR"};
    for (int i = 0; i < 2; i++) {
        lv_obj_t* l = nullptr;
        lv_obj_t* b = button(alarmBox, 18 + i * 196, 176, 188, 54, onAlarmBtn, i, &l);
        lv_label_set_text(l, bt[i]);
        lv_obj_set_style_text_font(l, &dejavu_bold_16, 0);
        if (i == 0) lv_obj_set_style_border_color(b, lv_color_hex(C_ALERT), 0);
    }
    lv_obj_set_hidden(alarmBox, true);
}

// Construye toda la interfaz con la paleta de ahora. Se llama al arrancar y al cambiar el modo noche.
static void build() {
    palette(wsettings::get().night);
    lv_obj_t* scr = mainScr;
    lv_obj_clean(scr);
    lv_obj_clean(lv_layer_top());
    for (int d = 0; d < D_COUNT; d++) valN[d] = subN[d] = 0;
    alarmShown = false;
    lv_obj_set_style_bg_color(scr, lv_color_hex(C_BG), 0);
    lv_obj_set_scrollable(scr, false);

    lv_obj_t* bar = box(scr, C_BAR, 0);
    lv_obj_set_size(bar, 480, TOP_H);
    lblTitle = label(bar, "", &dejavu_bold_16, C_TEXT);
    lv_obj_align(lblTitle, LV_ALIGN_LEFT_MID, 12, 0);
    dotWifi = statusDot(bar, "WiFi", -12);
    dotCan = statusDot(bar, "CAN", -62);
    dotBle = statusDot(bar, "BLE", -110);
    dotVane = statusDot(bar, "Veleta", -158);

    for (int i = 0; i < kPages; i++) {
        pages[i] = box(scr, C_BG, 0);
        lv_obj_set_size(pages[i], 464, BODY);
        lv_obj_set_pos(pages[i], PAD, TOP_H + PAD);
    }
    makeRose(pages[0]);
    makeWind(pages[1]);
    makeNumbers(pages[2]);
    makeHistory(pages[3]);
    makeSettings(pages[4]);

    lv_obj_t* nav = box(scr, C_BAR, 0);
    lv_obj_set_size(nav, 480, NAV_H);
    lv_obj_set_pos(nav, 0, 480 - NAV_H);
    for (int i = 0; i < kPages; i++) {
        lv_obj_t* b = lv_button_create(nav);
        lv_obj_remove_style_all(b);
        lv_obj_set_size(b, 96, NAV_H);
        lv_obj_set_pos(b, i * 96, 0);
        lv_obj_set_style_bg_opa(b, LV_OPA_COVER, 0);
        lv_obj_set_style_border_side(b, LV_BORDER_SIDE_TOP, 0);
        lv_obj_set_style_border_width(b, 3, 0);
        lv_obj_add_event_cb(b, onTab, LV_EVENT_CLICKED, (void*)(intptr_t)i);
        lv_obj_center(label(b, kTabs[i], &dejavu_14, C_SOFT));
        tabBtn[i] = b;
    }
    makeWifi();
    makeAbout();
    makeAlarm();
    refreshSettings();
    showTab(active);
    update();
}

void begin(DataStore* s) {
    store = s;
    mainScr = lv_screen_active();
    build();
    showSplash(mainScr, "Wind " PLOTTER_VERSION, &dejavu_bold_20);     // la bienvenida de APRS LoRa+, y luego esto
}

using WindNow = wlinks::Wind;
static WindNow windNow() { return wlinks::wind(); }

static const char* cardinalEs(double deg) {
    static const char* const k[16] = {"N", "NNE", "NE", "ENE", "E", "ESE", "SE", "SSE", "S", "SSO", "SO", "OSO", "O", "ONO", "NO", "NNO"};
    return k[(int)lround(norm360(deg) / 22.5) % 16];
}

static void setVal(Datum d, const char* v, const char* sub) {
    for (int i = 0; i < valN[d]; i++) lv_label_set_text(valLbl[d][i], v);
    for (int i = 0; i < subN[d]; i++) lv_label_set_text(subLbl[d][i], sub);
}

static void sample(const WindNow& w) {
    uint32_t now = millis();
    if (histN && now - histMs < 30000) return;
    if (!w.app) return;
    histMs = now;
    if (histN == kHist) {
        memmove(histSpd, histSpd + 1, sizeof(float) * (kHist - 1));
        memmove(histDir, histDir + 1, sizeof(float) * (kHist - 1));
        histN--;
    }
    histSpd[histN] = (float)(w.tru ? w.tws : w.aws);
    histDir[histN] = w.tru ? (float)w.twd : NAN;
    histN++;
}

static void updateHistory(const WindNow& w) {
    char b[64];
    lv_label_set_text(lblSpdTitle, w.tru || !w.app ? "VELOCIDAD REAL \xC2\xB7 kn" : "VELOCIDAD APARENTE \xC2\xB7 kn");
    float lo = 1e9f, hi = -1e9f, sum = 0;
    // la direccion se dibuja alrededor de la ultima, para que el paso por el norte no parta la grafica
    float ref = NAN, dlo = 1e9f, dhi = -1e9f;
    for (int i = histN - 1; i >= 0 && std::isnan(ref); i--) ref = histDir[i];
    for (int i = 0; i < kHist; i++) {
        int k = i - (kHist - histN);                      // lo mas nuevo, a la derecha
        if (k < 0) {
            lv_chart_set_series_value_by_id(chSpd, serSpd, (uint32_t)i, LV_CHART_POINT_NONE);
            lv_chart_set_series_value_by_id(chDir, serDir, (uint32_t)i, LV_CHART_POINT_NONE);
            continue;
        }
        float v = histSpd[k];
        lo = v < lo ? v : lo; hi = v > hi ? v : hi; sum += v;
        lv_chart_set_series_value_by_id(chSpd, serSpd, (uint32_t)i, (int32_t)lround(v * 10));
        if (std::isnan(histDir[k])) { lv_chart_set_series_value_by_id(chDir, serDir, (uint32_t)i, LV_CHART_POINT_NONE); continue; }
        float dv = (float)norm180(histDir[k] - ref);
        dlo = dv < dlo ? dv : dlo; dhi = dv > dhi ? dv : dhi;
        lv_chart_set_series_value_by_id(chDir, serDir, (uint32_t)i, (int32_t)lround(dv));
    }
    if (histN) {
        lv_chart_set_axis_range(chSpd, LV_CHART_AXIS_PRIMARY_Y, (int32_t)floor(lo - 1) * 10, (int32_t)ceil(hi + 1) * 10);
        snprintf(b, sizeof(b), "M\xC3\xADn %.1f   M\xC3\xA1x %.1f   Media %.1f", lo, hi, sum / histN); comma(b);
        lv_label_set_text(lblSpdFoot, b);
    } else lv_label_set_text(lblSpdFoot, "Sin datos todav\xC3\xAD" "a");
    if (!std::isnan(ref) && dlo <= dhi) {
        lv_chart_set_axis_range(chDir, LV_CHART_AXIS_PRIMARY_Y, (int32_t)floor(dlo - 5), (int32_t)ceil(dhi + 5));
        snprintf(b, sizeof(b), "Entre %03.0f\xC2\xB0 y %03.0f\xC2\xB0", norm360(ref + dlo), norm360(ref + dhi));
        lv_label_set_text(lblDirFoot, b);
    } else lv_label_set_text(lblDirFoot, "Falta la velocidad o el rumbo del barco");
    lv_chart_refresh(chSpd);
    lv_chart_refresh(chDir);
    if (w.tru || w.app) { snprintf(b, sizeof(b), "%.1f", w.tru ? w.tws : w.aws); comma(b); lv_label_set_text(lblSpdNow, b); }
    else lv_label_set_text(lblSpdNow, "--");
    if (w.tru) { snprintf(b, sizeof(b), "%03.0f\xC2\xB0", w.twd); lv_label_set_text(lblDirNow, b); }
    else lv_label_set_text(lblDirNow, "--");
}

static void setDot(lv_obj_t* d, bool ok) { lv_obj_set_style_bg_color(d, lv_color_hex(ok ? C_OK : C_OFF), 0); }

void update() {
    uint32_t now = millis();
    WindNow w = windNow();
    sample(w);
    char b[48], s[128];
    for (Dial& d : dials) {
        needle(d, d.lineA, d.pa, d.markA, w.app, w.awa);
        needle(d, d.lineT, d.pt, d.markT, w.tru, w.twa);
    }
    if (w.app) {
        snprintf(b, sizeof(b), "%.0f\xC2\xB0", fabs(w.awa));
        setVal(D_AWA, b, w.awa >= 0 ? "Estribor" : "Babor");
        snprintf(b, sizeof(b), "%.1f", w.aws); comma(b);
        int f = beaufort(w.aws);
        snprintf(s, sizeof(s), "Fuerza %d \xC2\xB7 %s", f, beaufortNameEs(f));
        setVal(D_AWS, b, s);
    } else { setVal(D_AWA, "--", "Sin veleta"); setVal(D_AWS, "--", ""); }
    if (w.tru) {
        snprintf(b, sizeof(b), "%.0f\xC2\xB0", fabs(w.twa));
        setVal(D_TWA, b, w.twa >= 0 ? "Estribor" : "Babor");
        snprintf(b, sizeof(b), "%.1f", w.tws); comma(b);
        snprintf(s, sizeof(s), "Fuerza %d", beaufort(w.tws));
        setVal(D_TWS, b, s);
        snprintf(b, sizeof(b), "%03.0f\xC2\xB0", w.twd);
        snprintf(s, sizeof(s), "Del %s", cardinalEs(w.twd));
        setVal(D_TWD, b, s);
    } else {
        setVal(D_TWA, "--", w.app ? "Falta rumbo" : "");
        setVal(D_TWS, "--", "");
        setVal(D_TWD, "--", "");
    }
    if (w.drift) { snprintf(b, sizeof(b), "%.1f", w.driftKn); comma(b); setVal(D_DRIFT, b, ""); }
    else setVal(D_DRIFT, "--", "");

    const wlinks::Status& ls = wlinks::status();
    const wble::Status& bs = wble::status();
    bool vane = ls.vaneLastMs && now - ls.vaneLastMs < 5000;
    bool can = ls.canLastMs && now - ls.canLastMs < 5000;
    setDot(dotVane, vane || (ls.simulator && w.app));
    setDot(dotBle, bs.connected);
    setDot(dotCan, can);
    setDot(dotWifi, wnet::status().connected);
    if (active == 3) updateHistory(w);
    // alarmas: con el viento de ahora; la ventana sale una vez y se queda hasta silenciar o cerrar
    alarm.update(now, w.tru ? w.tws : NAN, w.app ? w.aws : NAN, w.tru ? w.twd : NAN);
    bool alert = alarm.alerting(now);
    if (alert) {
        WindAlarmKind k = alarm.active();
        if (k == WindAlarmKind::Shift) {
            lv_label_set_text(alarmTitle, "WIND SHIFT");
            snprintf(s, sizeof(s), "El viento real ha rolado %.0f\xC2\xB0 a la %s\nL\xC3\xADmite %.0f\xC2\xB0 \xC2\xB7 ahora del %03.0f\xC2\xB0",
                     fabs(alarm.value()), alarm.value() >= 0 ? "derecha" : "izquierda", alarm.limit(), w.tru ? w.twd : 0.0);
        } else {
            lv_label_set_text(alarmTitle, "STRONG WIND");
            snprintf(s, sizeof(s), "Viento %s %.1f kn \xC2\xB7 fuerza %d\nL\xC3\xADmite %.0f kn", k == WindAlarmKind::Tws ? "real" : "aparente",
                     alarm.value(), beaufort(alarm.value()), alarm.limit());
            comma(s);
        }
        lv_label_set_text(alarmText, s);
    }
    if (alert != alarmShown) {
        alarmShown = alert;
        lv_obj_set_hidden(alarmBox, !alert);
        if (!alert) board::buzzer(false);
    }
    if (active == 4) {
        lv_label_set_text(rowState[0], vane ? "Recibiendo" : (ls.simulator ? "Simulador" : "Sin datos"));
        lv_obj_set_style_text_color(rowState[0], lv_color_hex(vane ? C_OK : C_DIM), 0);
        snprintf(s, sizeof(s), "NASA Target 2 Wind \xC2\xB7 MWV a 4800 \xC2\xB7 %u sentencias, %u bytes", (unsigned)ls.vaneLines, (unsigned)ls.serialBytes);
        lv_label_set_text(rowLine[0], s);
        lv_label_set_text(rowState[1], !bs.on ? "Apagado" : (bs.connected ? "Plotter enlazado" : "Esperando al plotter"));
        lv_obj_set_style_text_color(rowState[1], lv_color_hex(bs.connected ? C_OK : C_DIM), 0);
        bool nav = ls.navLastMs && now - ls.navLastMs < 5000;
        snprintf(s, sizeof(s), "APRS LoRa+ Wind \xC2\xB7 %u env\xC3\xAD" "os \xC2\xB7 %s", (unsigned)bs.sent,
                 nav ? "rumbo y velocidad del plotter" : "sin rumbo del plotter");
        lv_label_set_text(rowLine[1], s);
        if (!ls.can) lv_label_set_text(rowState[2], "Apagado");
        else if (ls.canOut && ls.canAddress == 254) lv_label_set_text(rowState[2], "Sin direcci\xC3\xB3n libre");
        else if (ls.canOut && ls.canAddress >= 0) { snprintf(b, sizeof(b), "%s \xC2\xB7 dir. %d", can ? "En el bus" : "Sin tr\xC3\xA1" "fico", ls.canAddress); lv_label_set_text(rowState[2], b); }
        else lv_label_set_text(rowState[2], can ? "En el bus" : "Sin tr\xC3\xA1" "fico");
        lv_obj_set_style_text_color(rowState[2], lv_color_hex(can ? C_OK : C_DIM), 0);
        snprintf(s, sizeof(s), "%s \xC2\xB7 %u recibidas, %u enviadas", ls.canOut ? "Emite el viento (130306)" : "Solo escucha",
                 (unsigned)ls.canFrames, (unsigned)ls.canSent);
        lv_label_set_text(rowLine[2], s);
        const wnet::Status& ns = wnet::status();
        if (ns.connected) { snprintf(b, sizeof(b), "Conectado \xC2\xB7 %s", ns.ip); lv_label_set_text(rowState[3], b); }
        else lv_label_set_text(rowState[3], ns.on ? "Uni\xC3\xA9ndose\xE2\x80\xA6" : "Apagado");
        lv_obj_set_style_text_color(rowState[3], lv_color_hex(ns.connected ? C_OK : C_DIM), 0);
        const wsettings::Values& v = wsettings::get();
        if (v.wifiSsid[0]) snprintf(s, sizeof(s), "%s \xC2\xB7 UDP a toda la red, puerto %d \xC2\xB7 %u env\xC3\xAD" "os", v.wifiSsid, v.udpPort, (unsigned)ns.sent);
        else snprintf(s, sizeof(s), "Sin red elegida \xC2\xB7 salida NMEA por UDP %d", v.udpPort);
        lv_label_set_text(rowLine[3], s);
    }
}

// El zumbador de la placa, con la alarma en la ventana: 0,15 s encendido cada segundo. Desde el bucle principal.
void tick() {
    static bool on = false;
    bool want = alarmShown && (millis() % 1000) < 150;
    if (want != on) { on = want; board::buzzer(on); }
}

void showPage(int i) { if (i >= 0 && i < kPages) showTab(i); }
void testWifi() { openWifi(); }
void testAbout() { lv_obj_set_hidden(aboutScr, false); }
void testNight() { wsettings::get().night = true; build(); }
void testAlarm() {
    wsettings::Values& v = wsettings::get();
    v.twsMax = 10; v.awsMax = 0; v.shiftMax = 0;
    refreshSettings();
}
void testStyle(int s) { if (s >= 0 && s < 3) { wsettings::get().roseStyle = s; refreshSettings(); } }

}
