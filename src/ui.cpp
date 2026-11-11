/*
 * Copyright (C) 2026 Manuel - EA7JRS
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "ui.h"

#include <Arduino.h>
#include <lvgl.h>

#include "board.h"
#include "aistargets.h"
#include "chartui.h"
#include "links.h"
#include "motion.h"
#include "route.h"
#include "tiles.h"
#include "wind.h"

LV_FONT_DECLARE(dejavu_12)
LV_FONT_DECLARE(dejavu_14)
LV_FONT_DECLARE(dejavu_16)
LV_FONT_DECLARE(dejavu_bold_16)
LV_FONT_DECLARE(dejavu_bold_20)
LV_FONT_DECLARE(dejavu_num_60)

namespace ui {

using namespace plotter;

// Paleta de las maquetas (propia del plotter)
static const uint32_t C_BG = 0x0B1622, C_BAR = 0x0F2033, C_PANEL = 0x13263A, C_LINE = 0x1E3A56, C_TEXT = 0xEAF2FA, C_DIM = 0x9DB2C7,
                      C_SOFT = 0xB7C8D9, C_ACC = 0x4DA3FF, C_WARN = 0xFFD166, C_RED = 0xFF6B6B, C_INK = 0x131A22;

static const int TOP_H = 44, NAV_H = 60, PAD = 10;
static const char* const kTabs[8] = {"Instrumentos", "Carta", "Rutas", "Radar AIS", "Viento", "Piloto", "Meteo", "Ajustes"};

struct Tile {
    lv_obj_t* value = nullptr;
    lv_obj_t* sub = nullptr;
};
enum { T_SOG, T_COG, T_HDG, T_DEPTH, T_AWS, T_AWA, T_TWS, T_WATER, T_COUNT };

static DataStore* store = nullptr;
static Tile tiles[T_COUNT];
static lv_obj_t* lblPos = nullptr;
static lv_obj_t* lblBadge = nullptr;
static lv_obj_t* lblTitle = nullptr;
static lv_obj_t* badgeBox = nullptr;
static lv_obj_t* pages[8] = {};
static lv_obj_t* tabBtn[8] = {};
static int active = 0;

// Radar AIS
static const int RAD = 476, RAD_R = 220, kDots = 16, kRows = 8;
static const double kRanges[] = {0.75, 1.5, 3.0, 6.0, 12.0};
static int rangeIdx = 2;
struct Dot {
    lv_obj_t* dot = nullptr;
    lv_obj_t* vec = nullptr;
    lv_obj_t* num = nullptr;
    lv_point_precise_t pts[2];
};
static Dot dots[kDots];
static lv_obj_t* ownVec = nullptr;
static lv_point_precise_t ownPts[2];
static lv_obj_t* ringLbl[3] = {};
static lv_obj_t* radarInfo = nullptr;
static lv_obj_t* rowBox[kRows] = {};
static lv_obj_t* rowLbl[kRows][5] = {};
static lv_obj_t* lblAisBadge = nullptr;
static uint32_t rowMmsi[kRows] = {};

// Ventana centrada sobre cualquier pantalla: ficha de un blanco o alerta de abordaje (la misma que las emergencias de la app)
enum ModalMode { M_NONE, M_DETAIL, M_COLLISION };
static lv_obj_t* modalBack = nullptr;
static lv_obj_t* modalHead = nullptr;
static lv_obj_t* modalTitle = nullptr;
static lv_obj_t* modalSub = nullptr;
static lv_obj_t* modalName = nullptr;
static lv_obj_t* modalBody = nullptr;
static lv_obj_t* modalBtnA = nullptr;
static lv_obj_t* modalBtnALbl = nullptr;
static lv_obj_t* modalBtnBLbl = nullptr;
static ModalMode modalMode = M_NONE;
static uint32_t modalMmsi = 0;
// Alertas cerradas: el mismo buque no vuelve a abrirla hasta 10 minutos despues
struct Closed { uint32_t mmsi = 0; uint32_t untilMs = 0; };
static Closed closedAlerts[8];

// Carta
static const int CH_W = 1004, CH_H = 476, kTrack = 240, kChartDots = 16;
static lv_obj_t* chart = nullptr;
static lv_obj_t* chBoat = nullptr;
static lv_obj_t* chVec = nullptr;
static lv_obj_t* chTrack = nullptr;
static lv_obj_t* chScaleBar = nullptr;
static lv_obj_t* chScaleLbl = nullptr;
static lv_obj_t* chInfo = nullptr;
static lv_obj_t* chCenterBtn = nullptr;
static lv_point_precise_t chVecPts[2];
static lv_point_precise_t chTrackPts[kTrack];
static LatLon trackPts[kTrack];
static int trackN = 0;
static TrackSampler trackSampler;
static Dot chDots[kChartDots];
static int chZoom = 14;
// Teselas de la tarjeta: S:/charts/<mapa>/<z>/<x>/<y>.png (S: es la tarjeta, por el sistema de ficheros de LVGL)
static const int kTileImgs = 24;
struct TileImg { lv_obj_t* img = nullptr; int z = -1, x = 0, y = 0; bool used = false; };
static TileImg tileImgs[kTileImgs];
struct Missing { int z = -1, x = 0, y = 0; };
static Missing missing[96];                       // teselas que no estan en la tarjeta, para no preguntar cada vez
static int missingNext = 0;
static uint32_t missingClearedMs = 0;
static const char* chartFolder = "S:/charts/osm";
static bool chFollow = true;
static LatLon chCenter{36.53, -6.29};

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

static void showTab(int i) {
    active = i;
    if (lblTitle) {
        lv_label_set_text(lblTitle, kTabs[i]);
        lv_obj_update_layout(lblTitle);
        lv_obj_align_to(badgeBox, lblTitle, LV_ALIGN_OUT_RIGHT_MID, 12, 0);
    }
    for (int k = 0; k < 8; k++) {
        if (pages[k]) {
            lv_obj_set_hidden(pages[k], k != i);
        }
        if (tabBtn[k]) {
            lv_obj_set_style_bg_color(tabBtn[k], lv_color_hex(k == i ? C_PANEL : C_BAR), 0);
            lv_obj_set_style_border_color(tabBtn[k], lv_color_hex(k == i ? C_ACC : C_BAR), 0);
            lv_obj_set_style_text_color(lv_obj_get_child(tabBtn[k], 0), lv_color_hex(k == i ? C_TEXT : C_SOFT), 0);
        }
    }
}

static void onTab(lv_event_t* e) { showTab((int)(intptr_t)lv_event_get_user_data(e)); }

static void makeTile(lv_obj_t* grid, int idx, int col, int row, const char* name, const char* unit, uint32_t valueColor = C_TEXT) {
    lv_obj_t* t = box(grid, C_PANEL, 10);
    lv_obj_set_grid_cell(t, LV_GRID_ALIGN_STRETCH, col, 1, LV_GRID_ALIGN_STRETCH, row, 1);
    lv_obj_set_style_pad_all(t, 12, 0);
    lv_obj_t* n = label(t, name, &dejavu_bold_16, C_DIM);
    lv_obj_align(n, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_t* u = label(t, unit, &dejavu_14, C_DIM);
    lv_obj_align(u, LV_ALIGN_TOP_RIGHT, 0, 2);
    tiles[idx].value = label(t, "--", &dejavu_num_60, valueColor);
    lv_obj_align(tiles[idx].value, LV_ALIGN_LEFT_MID, 0, 6);
    tiles[idx].sub = label(t, "Sin datos", &dejavu_12, C_DIM);
    lv_obj_align(tiles[idx].sub, LV_ALIGN_BOTTOM_LEFT, 0, 0);
}

static void comma(char* b);

static void onRange(lv_event_t* e) {
    int d = (int)(intptr_t)lv_event_get_user_data(e);
    int n = (int)(sizeof(kRanges) / sizeof(kRanges[0]));
    rangeIdx += d;
    if (rangeIdx < 0) rangeIdx = 0;
    if (rangeIdx >= n) rangeIdx = n - 1;
}

static void onRow(lv_event_t* e);

static lv_obj_t* textButton(lv_obj_t* parent, const char* text, int w, int h, lv_event_cb_t cb, intptr_t data) {
    lv_obj_t* b = lv_button_create(parent);
    lv_obj_remove_style_all(b);
    lv_obj_set_size(b, w, h);
    lv_obj_set_style_bg_color(b, lv_color_hex(C_BAR), 0);
    lv_obj_set_style_bg_opa(b, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(b, 8, 0);
    lv_obj_set_style_border_color(b, lv_color_hex(C_LINE), 0);
    lv_obj_set_style_border_width(b, 1, 0);
    lv_obj_t* l = label(b, text, strlen(text) > 2 ? &dejavu_bold_16 : &dejavu_bold_20, C_TEXT);
    lv_obj_center(l);
    lv_obj_add_event_cb(b, cb, LV_EVENT_CLICKED, (void*)data);
    return b;
}

static void makeRadar(lv_obj_t* page) {
    // Izquierda: radar con el norte arriba y el barco en el centro
    lv_obj_t* rad = box(page, C_PANEL, 10);
    lv_obj_set_size(rad, RAD, RAD);
    lv_obj_align(rad, LV_ALIGN_LEFT_MID, 0, 0);
    for (int i = 0; i < 3; i++) {
        int d = 2 * RAD_R * (i + 1) / 3;
        lv_obj_t* ring = lv_obj_create(rad);
        lv_obj_remove_style_all(ring);
        lv_obj_set_size(ring, d, d);
        lv_obj_center(ring);
        lv_obj_set_style_radius(ring, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_border_color(ring, lv_color_hex(i == 2 ? 0x2F5F88 : C_LINE), 0);
        lv_obj_set_style_border_width(ring, i == 2 ? 2 : 1, 0);
        lv_obj_set_scrollable(ring, false);
        ringLbl[i] = label(rad, "", &dejavu_12, C_DIM);
        lv_obj_align(ringLbl[i], LV_ALIGN_CENTER, 4 + d * 354 / 1000, 4 + d * 354 / 1000);     // a 45 grados, por fuera del anillo
    }
    lv_obj_t* north = label(rad, "N", &dejavu_bold_16, C_SOFT);
    lv_obj_align(north, LV_ALIGN_TOP_MID, 0, 2);
    ownVec = lv_line_create(rad);
    lv_obj_set_style_line_color(ownVec, lv_color_hex(C_TEXT), 0);
    lv_obj_set_style_line_width(ownVec, 2, 0);
    lv_obj_t* own = box(rad, C_TEXT, LV_RADIUS_CIRCLE);
    lv_obj_set_size(own, 12, 12);
    lv_obj_center(own);
    for (int i = 0; i < kDots; i++) {
        dots[i].vec = lv_line_create(rad);
        lv_obj_set_style_line_width(dots[i].vec, 2, 0);
        dots[i].dot = box(rad, C_ACC, LV_RADIUS_CIRCLE);
        lv_obj_set_size(dots[i].dot, 14, 14);
        dots[i].num = label(rad, "", &dejavu_bold_16, C_TEXT);
        lv_obj_set_hidden(dots[i].dot, true);
        lv_obj_set_hidden(dots[i].vec, true);
        lv_obj_set_hidden(dots[i].num, true);
    }
    radarInfo = label(rad, "", &dejavu_12, C_DIM);
    lv_obj_align(radarInfo, LV_ALIGN_BOTTOM_LEFT, 10, -8);

    // Derecha: alcance y lista (los peligrosos primero)
    int lx = RAD + PAD, lw = board::W - 2 * PAD - lx;
    lv_obj_t* minus = textButton(page, "-", 56, 46, onRange, -1);
    lv_obj_set_pos(minus, lx, 0);
    lv_obj_t* plus = textButton(page, "+", 56, 46, onRange, 1);
    lv_obj_set_pos(plus, lx + 62, 0);
    lv_obj_t* badge = box(page, C_PANEL, 8);
    lv_obj_set_size(badge, lw - 124, 46);
    lv_obj_set_pos(badge, lx + 124, 0);
    lblAisBadge = label(badge, "Sin blancos", &dejavu_bold_16, C_TEXT);
    lv_obj_align(lblAisBadge, LV_ALIGN_LEFT_MID, 12, 0);

    static const int colX[5] = {10, 40, 232, 310, 376};
    static const char* const head[5] = {"N.\xC2\xBA", "Blanco", "Dist.", "Dem.", "CPA \xC2\xB7 TCPA"};
    for (int c = 0; c < 5; c++) {
        lv_obj_t* h = label(page, head[c], &dejavu_12, C_DIM);
        lv_obj_set_pos(h, lx + colX[c], 54);
    }
    for (int r = 0; r < kRows; r++) {
        rowBox[r] = box(page, C_PANEL, 8);
        lv_obj_set_size(rowBox[r], lw, 44);
        lv_obj_set_pos(rowBox[r], lx, 74 + r * 50);
        lv_obj_set_style_border_width(rowBox[r], 2, 0);
        lv_obj_set_style_border_color(rowBox[r], lv_color_hex(C_PANEL), 0);
        for (int c = 0; c < 5; c++) {
            rowLbl[r][c] = label(rowBox[r], "", c == 1 ? &dejavu_bold_16 : &dejavu_16, c == 0 ? C_ACC : C_TEXT);
            lv_obj_align(rowLbl[r][c], LV_ALIGN_LEFT_MID, colX[c], 0);
        }
        lv_obj_set_clickable(rowBox[r], true);
        lv_obj_add_event_cb(rowBox[r], onRow, LV_EVENT_CLICKED, (void*)(intptr_t)r);
        lv_label_set_long_mode(rowLbl[r][1], LV_LABEL_LONG_MODE_DOTS);
        lv_obj_set_width(rowLbl[r][1], 186);
    }
}

static void comma(char* b) {
    for (; *b; b++) if (*b == '.') *b = ',';
}

static void updateRadar() {
    AisTargets& tg = links::targets();
    double range = kRanges[rangeIdx];
    uint32_t now = millis();
    for (int i = 0; i < 3; i++) {
        char b[16];
        snprintf(b, sizeof(b), i == 2 ? "%.2g mn" : "%.2g", range * (i + 1) / 3);
        comma(b);
        lv_label_set_text(ringLbl[i], b);
    }
    // Vector propio: lo que se anda en 6 minutos
    double sog = 0, cog = 0;
    bool moving = store->get(Key::Sog, now, sog) && store->get(Key::Cog, now, cog) && sog > 0.3;
    const double c = RAD / 2.0;
    if (moving) {
        double len = sog / 10.0 / range * RAD_R;
        ownPts[0] = {(lv_value_precise_t)c, (lv_value_precise_t)c};
        ownPts[1] = {(lv_value_precise_t)(c + len * sin(deg2rad(cog))), (lv_value_precise_t)(c - len * cos(deg2rad(cog)))};
        lv_line_set_points(ownVec, ownPts, 2);
    }
    lv_obj_set_hidden(ownVec, !moving);

    int idx[kDots];
    int n = tg.sorted(idx, kDots), shown = 0, danger = tg.dangerousCount();
    for (int i = 0; i < kDots; i++) {
        Dot& d = dots[i];
        bool vis = false;
        if (i < n) {
            const AisTarget& t = tg.at(idx[i]);
            if (t.hasRange && t.rangeNm <= range) {
                vis = true;
                shown++;
                double r = t.rangeNm / range * RAD_R;
                double x = c + r * sin(deg2rad(t.bearing)), y = c - r * cos(deg2rad(t.bearing));
                uint32_t col = t.dangerous ? C_WARN : (t.lost ? 0x5B6B7A : (t.kind == AisKind::Aton ? 0xF2D43D : C_ACC));
                lv_obj_set_style_bg_color(d.dot, lv_color_hex(col), 0);
                lv_obj_set_pos(d.dot, (int)lround(x) - 7, (int)lround(y) - 7);
                char b[8];
                snprintf(b, sizeof(b), "%d", i + 1);
                lv_label_set_text(d.num, b);
                lv_obj_set_style_text_color(d.num, lv_color_hex(col), 0);
                lv_obj_set_pos(d.num, (int)lround(x) + 9, (int)lround(y) - 20);
                bool vec = t.hasSog && t.hasCog && t.sogKn > 0.3 && !t.lost;
                if (vec) {
                    double len = t.sogKn / 10.0 / range * RAD_R;
                    d.pts[0] = {(lv_value_precise_t)x, (lv_value_precise_t)y};
                    d.pts[1] = {(lv_value_precise_t)(x + len * sin(deg2rad(t.cogDeg))), (lv_value_precise_t)(y - len * cos(deg2rad(t.cogDeg)))};
                    lv_line_set_points(d.vec, d.pts, 2);
                    lv_obj_set_style_line_color(d.vec, lv_color_hex(col), 0);
                }
                lv_obj_set_hidden(d.vec, !vec);
            }
        }
        lv_obj_set_hidden(d.dot, !vis);
        lv_obj_set_hidden(d.num, !vis);
        if (!vis) lv_obj_set_hidden(d.vec, true);
    }
    char info[64];
    snprintf(info, sizeof(info), "Norte arriba \xC2\xB7 vectores de 6 min \xC2\xB7 %d de %d en pantalla", shown, tg.count());
    lv_label_set_text(radarInfo, info);
    if (tg.count() == 0) lv_label_set_text(lblAisBadge, "Sin blancos");
    else {
        char b[48];
        if (danger) snprintf(b, sizeof(b), "%d blancos \xC2\xB7 %d con riesgo", tg.count(), danger);
        else snprintf(b, sizeof(b), "%d blancos \xC2\xB7 sin riesgo", tg.count());
        lv_label_set_text(lblAisBadge, b);
    }
    lv_obj_set_style_text_color(lblAisBadge, lv_color_hex(danger ? C_WARN : C_TEXT), 0);

    for (int r = 0; r < kRows; r++) {
        if (r >= n) { lv_obj_set_hidden(rowBox[r], true); rowMmsi[r] = 0; continue; }
        lv_obj_set_hidden(rowBox[r], false);
        const AisTarget& t = tg.at(idx[r]);
        rowMmsi[r] = t.mmsi;
        char b[48];
        snprintf(b, sizeof(b), "%d", r + 1);
        lv_label_set_text(rowLbl[r][0], b);
        if (t.name[0]) lv_label_set_text(rowLbl[r][1], t.name);
        else { snprintf(b, sizeof(b), "MMSI %09u", (unsigned)t.mmsi); lv_label_set_text(rowLbl[r][1], b); }
        if (t.hasRange) {
            snprintf(b, sizeof(b), "%.1f mn", t.rangeNm); comma(b);
            lv_label_set_text(rowLbl[r][2], b);
            snprintf(b, sizeof(b), "%03d\xC2\xB0", (int)lround(t.bearing) % 360);
            lv_label_set_text(rowLbl[r][3], b);
        } else {
            lv_label_set_text(rowLbl[r][2], "--");
            lv_label_set_text(rowLbl[r][3], "--");
        }
        if (t.lost) lv_label_set_text(rowLbl[r][4], "Perdido");
        else if (!t.hasCpa) lv_label_set_text(rowLbl[r][4], t.kind == AisKind::Aton ? "Baliza" : "--");
        else if (t.tcpaMin < 0) lv_label_set_text(rowLbl[r][4], "Se aleja");
        else if (t.hasSog && t.sogKn < 0.3 && sog < 0.3) lv_label_set_text(rowLbl[r][4], "Parado");
        else { snprintf(b, sizeof(b), "%.1f mn \xC2\xB7 %d min", t.cpaNm, (int)lround(t.tcpaMin)); comma(b); lv_label_set_text(rowLbl[r][4], b); }
        lv_obj_set_style_border_color(rowBox[r], lv_color_hex(t.dangerous ? C_WARN : C_PANEL), 0);
        lv_obj_set_style_bg_color(rowBox[r], lv_color_hex(t.dangerous ? 0x2A2A1C : C_PANEL), 0);
        lv_obj_set_style_text_color(rowLbl[r][4], lv_color_hex(t.dangerous ? C_WARN : (t.lost ? C_DIM : C_TEXT)), 0);
        lv_obj_set_style_text_color(rowLbl[r][0], lv_color_hex(t.dangerous ? C_WARN : C_ACC), 0);
    }
}

// ---------- Ventana centrada ----------
static void closeModal() {
    modalMode = M_NONE;
    modalMmsi = 0;
    if (modalBack) lv_obj_set_hidden(modalBack, true);
}

static void onModalA(lv_event_t*) {                       // silenciar la alarma de ese buque (o volver a activarla)
    AisTargets& tg = links::targets();
    int i = tg.find(modalMmsi);
    if (i >= 0) tg.silence(modalMmsi, modalMode == M_COLLISION ? true : !tg.at(i).silenced);
    if (modalMode == M_COLLISION) closeModal();
}

static void onModalB(lv_event_t*) {                       // cerrar
    if (modalMode == M_COLLISION) {
        int slot = 0;
        uint32_t now = millis();
        for (int i = 0; i < 8; i++) {
            if (closedAlerts[i].mmsi == modalMmsi) { slot = i; break; }
            if ((int32_t)(closedAlerts[i].untilMs - now) < (int32_t)(closedAlerts[slot].untilMs - now)) slot = i;
        }
        closedAlerts[slot].mmsi = modalMmsi;
        closedAlerts[slot].untilMs = now + 10 * 60000u;
    }
    closeModal();
}

static lv_obj_t* modalButton(lv_obj_t* parent, uint32_t border, uint32_t text, lv_event_cb_t cb, lv_obj_t** lblOut) {
    lv_obj_t* b = lv_button_create(parent);
    lv_obj_remove_style_all(b);
    lv_obj_set_size(b, 238, 50);
    lv_obj_set_style_bg_color(b, lv_color_hex(0x1E2A36), 0);
    lv_obj_set_style_bg_opa(b, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(b, 10, 0);
    lv_obj_set_style_border_color(b, lv_color_hex(border), 0);
    lv_obj_set_style_border_width(b, 1, 0);
    *lblOut = label(b, "", &dejavu_bold_20, text);
    lv_obj_center(*lblOut);
    lv_obj_add_event_cb(b, cb, LV_EVENT_CLICKED, nullptr);
    return b;
}

static void makeModal() {
    modalBack = lv_obj_create(lv_layer_top());
    lv_obj_remove_style_all(modalBack);
    lv_obj_set_size(modalBack, board::W, board::H);
    lv_obj_set_style_bg_color(modalBack, lv_color_hex(0x06090D), 0);
    lv_obj_set_style_bg_opa(modalBack, 190, 0);
    lv_obj_set_scrollable(modalBack, false);
    lv_obj_set_clickable(modalBack, true);                 // lo de debajo no recibe toques mientras esta abierta
    lv_obj_t* win = box(modalBack, 0x18222D, 18);
    lv_obj_set_size(win, 540, 330);
    lv_obj_center(win);
    lv_obj_set_style_clip_corner(win, true, 0);
    modalHead = box(win, C_WARN);
    lv_obj_set_size(modalHead, 540, 60);
    lv_obj_align(modalHead, LV_ALIGN_TOP_MID, 0, 0);
    modalTitle = label(modalHead, "", &dejavu_bold_20, C_INK);
    lv_obj_align(modalTitle, LV_ALIGN_TOP_LEFT, 16, 8);
    modalSub = label(modalHead, "", &dejavu_14, C_INK);
    lv_obj_align(modalSub, LV_ALIGN_BOTTOM_LEFT, 16, -8);
    modalName = label(win, "", &dejavu_bold_20, C_TEXT);
    lv_obj_align(modalName, LV_ALIGN_TOP_LEFT, 16, 72);
    modalBody = label(win, "", &dejavu_16, C_SOFT);
    lv_obj_align(modalBody, LV_ALIGN_TOP_LEFT, 16, 104);
    lv_obj_set_width(modalBody, 508);
    lv_obj_set_style_text_line_space(modalBody, 6, 0);
    modalBtnA = modalButton(win, 0xFFB347, 0xFFB347, onModalA, &modalBtnALbl);
    lv_obj_align(modalBtnA, LV_ALIGN_BOTTOM_LEFT, 16, -14);
    lv_obj_t* b = modalButton(win, 0x2E3E4F, C_TEXT, onModalB, &modalBtnBLbl);
    lv_obj_align(b, LV_ALIGN_BOTTOM_RIGHT, -16, -14);
    lv_obj_set_hidden(modalBack, true);
}

static void openModal(ModalMode mode, uint32_t mmsi) {
    modalMode = mode;
    modalMmsi = mmsi;
    lv_obj_set_hidden(modalBack, false);
}

// Rellena la ventana con el estado actual del blanco; la cierra si el blanco ya no existe o (alerta) ya no hay riesgo
static void updateModal() {
    if (modalMode == M_NONE) return;
    AisTargets& tg = links::targets();
    int i = tg.find(modalMmsi);
    if (i < 0) { closeModal(); return; }
    const AisTarget& t = tg.at(i);
    if (modalMode == M_COLLISION && (!t.dangerous || t.silenced)) { closeModal(); return; }
    char b[420], dist[32] = "--", cpaTxt[48] = "--";
    if (t.hasRange) { snprintf(dist, sizeof(dist), "%.1f mn \xC2\xB7 %03d\xC2\xB0", t.rangeNm, (int)lround(t.bearing) % 360); comma(dist); }
    if (t.lost) snprintf(cpaTxt, sizeof(cpaTxt), "Perdido");
    else if (t.hasCpa && t.tcpaMin >= 0) { snprintf(cpaTxt, sizeof(cpaTxt), "%.2f mn en %d min", t.cpaNm, (int)lround(t.tcpaMin)); comma(cpaTxt); }
    else if (t.hasCpa) snprintf(cpaTxt, sizeof(cpaTxt), "Se aleja");
    if (t.name[0]) lv_label_set_text(modalName, t.name);
    else { snprintf(b, sizeof(b), "MMSI %09u", (unsigned)t.mmsi); lv_label_set_text(modalName, b); }
    char sog[24] = "--", cog[16] = "--", len[24] = "--";
    if (t.hasSog) { snprintf(sog, sizeof(sog), "%.1f kn", t.sogKn); comma(sog); }
    if (t.hasCog) snprintf(cog, sizeof(cog), "%03d\xC2\xB0", (int)lround(t.cogDeg) % 360);
    if (t.lengthM > 0) snprintf(len, sizeof(len), "%d x %d m", t.lengthM, t.beamM);
    if (modalMode == M_COLLISION) {
        // Textos en ingles, como la ventana de emergencias de la app Android
        lv_obj_set_style_bg_color(modalHead, lv_color_hex(C_WARN), 0);
        lv_label_set_text(modalTitle, "COLLISION RISK");
        lv_label_set_text(modalSub, "AIS \xC2\xB7 closest point of approach");
        snprintf(b, sizeof(b), "Distance / bearing:  %s\nCPA:  %s\nSpeed / course:  %s \xC2\xB7 %s\nMMSI %09u \xC2\xB7 %s", dist, cpaTxt, sog, cog,
                 (unsigned)t.mmsi, t.classB ? "class B" : "class A");
        lv_label_set_text(modalBtnALbl, "SILENCE ALARM");
        lv_label_set_text(modalBtnBLbl, "CLOSE");
    } else {
        lv_obj_set_style_bg_color(modalHead, lv_color_hex(t.dangerous ? C_WARN : C_ACC), 0);
        lv_label_set_text(modalTitle, t.kind == AisKind::Aton ? "Ayuda a la navegaci\xC3\xB3n" : aisShipTypeNameEs(t.shipType));
        snprintf(b, sizeof(b), "MMSI %09u \xC2\xB7 AIS %s%s%s", (unsigned)t.mmsi, t.classB ? "B" : "A", t.callsign[0] ? " \xC2\xB7 " : "", t.callsign);
        lv_label_set_text(modalSub, b);
        snprintf(b, sizeof(b), "Distancia y demora:  %s\nM\xC3\xA1xima aproximaci\xC3\xB3n:  %s\nVelocidad y rumbo:  %s \xC2\xB7 %s\nEslora y manga:  %s\n\xC3\x9Altima se\xC3\xB1" "al:  hace %u s",
                 dist, cpaTxt, sog, cog, len, (unsigned)((millis() - t.lastMs) / 1000));
        lv_label_set_text(modalBtnALbl, t.silenced ? "Activar alarma" : "Silenciar alarma");
        lv_label_set_text(modalBtnBLbl, "Cerrar");
    }
    lv_label_set_text(modalBody, b);
}

// Abre la alerta del primer blanco con riesgo que no este silenciado ni cerrado hace poco
static void checkCollision() {
    if (modalMode == M_COLLISION) return;
    AisTargets& tg = links::targets();
    if (tg.dangerousCount() == 0) return;
    uint32_t now = millis();
    int idx[4];
    int n = tg.sorted(idx, 4);
    for (int k = 0; k < n; k++) {
        const AisTarget& t = tg.at(idx[k]);
        if (!t.dangerous || t.silenced) continue;
        bool closed = false;
        for (const Closed& c : closedAlerts)
            if (c.mmsi == t.mmsi && (int32_t)(c.untilMs - now) > 0) closed = true;
        if (closed) continue;
        openModal(M_COLLISION, t.mmsi);
        return;
    }
}

static void onRow(lv_event_t* e) {
    int r = (int)(intptr_t)lv_event_get_user_data(e);
    if (r >= 0 && r < kRows && rowMmsi[r]) openModal(M_DETAIL, rowMmsi[r]);
}

// ---------- Carta ----------
static void onZoom(lv_event_t* e) {
    chZoom += (int)(intptr_t)lv_event_get_user_data(e);
    if (chZoom < 5) chZoom = 5;
    if (chZoom > 18) chZoom = 18;
}

static void onCenter(lv_event_t*) { chFollow = true; }

static void onChartDrag(lv_event_t*) {                    // arrastrar mueve la carta y deja de seguir al barco
    lv_indev_t* in = lv_indev_active();
    if (!in) return;
    lv_point_t v;
    lv_indev_get_vect(in, &v);
    if (v.x == 0 && v.y == 0) return;
    double px, py;
    lonLatToWorldPx(chCenter, chZoom, px, py);
    chCenter = worldPxToLonLat(px - v.x, py - v.y, chZoom);
    chFollow = false;
}

static bool tileExists(int z, int x, int y, char* path, int size) {
    for (const Missing& m : missing)
        if (m.z == z && m.x == x && m.y == y) return false;
    if (!tilePath(path, size, chartFolder, z, x, y, "png")) return false;
    lv_fs_file_t f;
    if (lv_fs_open(&f, path, LV_FS_MODE_RD) == LV_FS_RES_OK) {
        lv_fs_close(&f);
        return true;
    }
    missing[missingNext] = {z, x, y};
    missingNext = (missingNext + 1) % (int)(sizeof(missing) / sizeof(missing[0]));
    return false;
}

// Coloca las teselas que cubren la vista. Devuelve cuantas hay en pantalla.
static int layoutTiles(double cx, double cy) {
    uint32_t now = millis();
    if (now - missingClearedMs > 30000) {                 // por si se cambia la tarjeta o se copian cartas nuevas
        missingClearedMs = now;
        for (Missing& m : missing) m.z = -1;
    }
    int max = (1 << chZoom) - 1;
    int tx0 = (int)floor((cx - CH_W / 2.0) / kTileSize), tx1 = (int)floor((cx + CH_W / 2.0) / kTileSize);
    int ty0 = (int)floor((cy - CH_H / 2.0) / kTileSize), ty1 = (int)floor((cy + CH_H / 2.0) / kTileSize);
    for (TileImg& t : tileImgs) t.used = false;
    int shown = 0;
    // Primero las que ya estan cargadas; despues las nuevas, en los huecos que queden
    for (int pass = 0; pass < 2; pass++) {
        for (int ty = ty0; ty <= ty1; ty++) {
            for (int tx = tx0; tx <= tx1; tx++) {
                if (tx < 0 || ty < 0 || tx > max || ty > max) continue;
                TileImg* slot = nullptr;
                for (TileImg& t : tileImgs)
                    if (t.z == chZoom && t.x == tx && t.y == ty) { slot = &t; break; }
                if (pass == 0 && !slot) continue;
                if (pass == 1) {
                    if (slot) continue;
                    char path[64];
                    if (!tileExists(chZoom, tx, ty, path, sizeof(path))) continue;
                    for (TileImg& t : tileImgs)
                        if (!t.used) { slot = &t; break; }
                    if (!slot) continue;
                    lv_image_set_src(slot->img, path);
                    slot->z = chZoom; slot->x = tx; slot->y = ty;
                }
                slot->used = true;
                lv_obj_set_pos(slot->img, (int)lround(CH_W / 2.0 + tx * (double)kTileSize - cx), (int)lround(CH_H / 2.0 + ty * (double)kTileSize - cy));
                lv_obj_set_hidden(slot->img, false);
                shown++;
            }
        }
    }
    for (TileImg& t : tileImgs)
        if (!t.used) { lv_obj_set_hidden(t.img, true); t.z = -1; }
    return shown;
}

static void makeChart(lv_obj_t* page) {
    chart = box(page, 0x0E2A44, 10);
    lv_obj_set_size(chart, CH_W, CH_H);
    lv_obj_center(chart);
    lv_obj_set_style_clip_corner(chart, true, 0);
    lv_obj_set_clickable(chart, true);
    lv_obj_add_event_cb(chart, onChartDrag, LV_EVENT_PRESSING, nullptr);
    for (TileImg& t : tileImgs) {                         // lo primero: quedan debajo de todo lo demas
        t.img = lv_image_create(chart);
        lv_obj_set_size(t.img, kTileSize, kTileSize);
        lv_obj_set_hidden(t.img, true);
    }
    chTrack = lv_line_create(chart);
    lv_obj_set_style_line_color(chTrack, lv_color_hex(0xFF9F43), 0);
    lv_obj_set_style_line_width(chTrack, 3, 0);
    chVec = lv_line_create(chart);
    lv_obj_set_style_line_color(chVec, lv_color_hex(C_TEXT), 0);
    lv_obj_set_style_line_width(chVec, 2, 0);
    for (int i = 0; i < kChartDots; i++) {
        chDots[i].vec = lv_line_create(chart);
        lv_obj_set_style_line_width(chDots[i].vec, 2, 0);
        chDots[i].dot = box(chart, C_ACC, LV_RADIUS_CIRCLE);
        lv_obj_set_size(chDots[i].dot, 14, 14);
        chDots[i].num = label(chart, "", &dejavu_12, C_TEXT);
        lv_obj_set_style_bg_color(chDots[i].num, lv_color_hex(C_BAR), 0);
        lv_obj_set_style_bg_opa(chDots[i].num, 200, 0);
        lv_obj_set_style_pad_hor(chDots[i].num, 4, 0);
        lv_obj_set_style_pad_ver(chDots[i].num, 1, 0);
        lv_obj_set_style_radius(chDots[i].num, 4, 0);
        lv_obj_set_style_border_color(chDots[i].dot, lv_color_hex(C_INK), 0);
        lv_obj_set_style_border_width(chDots[i].dot, 2, 0);
        lv_obj_set_hidden(chDots[i].dot, true);
        lv_obj_set_hidden(chDots[i].vec, true);
        lv_obj_set_hidden(chDots[i].num, true);
    }
    chBoat = box(chart, C_TEXT, LV_RADIUS_CIRCLE);
    lv_obj_set_size(chBoat, 16, 16);
    lv_obj_set_style_border_color(chBoat, lv_color_hex(C_INK), 0);
    lv_obj_set_style_border_width(chBoat, 2, 0);

    lv_obj_t* plus = textButton(chart, "+", 52, 52, onZoom, 1);
    lv_obj_align(plus, LV_ALIGN_TOP_RIGHT, -8, 8);
    lv_obj_t* minus = textButton(chart, "-", 52, 52, onZoom, -1);
    lv_obj_align(minus, LV_ALIGN_TOP_RIGHT, -8, 68);
    chCenterBtn = textButton(chart, "Centrar barco", 170, 46, onCenter, 0);
    lv_obj_align(chCenterBtn, LV_ALIGN_BOTTOM_RIGHT, -8, -8);
    chScaleBar = box(chart, C_TEXT);
    lv_obj_set_height(chScaleBar, 3);
    lv_obj_align(chScaleBar, LV_ALIGN_BOTTOM_LEFT, 16, -14);
    chScaleLbl = label(chart, "", &dejavu_14, C_TEXT);
    lv_obj_align(chScaleLbl, LV_ALIGN_BOTTOM_LEFT, 12, -22);
    lv_obj_set_style_bg_color(chScaleLbl, lv_color_hex(C_BAR), 0);
    lv_obj_set_style_bg_opa(chScaleLbl, 200, 0);
    lv_obj_set_style_pad_hor(chScaleLbl, 5, 0);
    lv_obj_set_style_pad_ver(chScaleLbl, 2, 0);
    lv_obj_set_style_radius(chScaleLbl, 5, 0);
    chInfo = label(chart, "", &dejavu_12, C_TEXT);
    lv_obj_align(chInfo, LV_ALIGN_TOP_LEFT, 8, 8);
    lv_obj_set_style_bg_color(chInfo, lv_color_hex(C_BAR), 0);
    lv_obj_set_style_bg_opa(chInfo, 200, 0);
    lv_obj_set_style_pad_hor(chInfo, 6, 0);
    lv_obj_set_style_pad_ver(chInfo, 3, 0);
    lv_obj_set_style_radius(chInfo, 6, 0);
}

static void updateChart() {
    uint32_t now = millis();
    double la, lo, sog = 0, cog = 0;
    bool own = store->get(Key::Lat, now, la) && store->get(Key::Lon, now, lo);
    if (own && chFollow) chCenter = {la, lo};
    double cx, cy;
    lonLatToWorldPx(chCenter, chZoom, cx, cy);
    auto toScreen = [&](LatLon p, double& x, double& y) {
        double px, py;
        lonLatToWorldPx(p, chZoom, px, py);
        x = CH_W / 2.0 + (px - cx);
        y = CH_H / 2.0 + (py - cy);
    };
    auto inside = [](double x, double y) { return x > -20 && x < CH_W + 20 && y > -20 && y < CH_H + 20; };
    double mpp = metersPerPixel(chCenter.lat, chZoom);
    int tilesShown = layoutTiles(cx, cy);

    // Barco y vector de 6 minutos
    bool boatVis = false;
    double bx = 0, by = 0;
    if (own) {
        toScreen({la, lo}, bx, by);
        boatVis = inside(bx, by);
        lv_obj_set_pos(chBoat, (int)lround(bx) - 8, (int)lround(by) - 8);
    }
    lv_obj_set_hidden(chBoat, !boatVis);
    bool vec = boatVis && store->get(Key::Sog, now, sog) && store->get(Key::Cog, now, cog) && sog > 0.3;
    if (vec) {
        double len = sog / 10.0 * kMetersPerNm / mpp;
        chVecPts[0] = {(lv_value_precise_t)bx, (lv_value_precise_t)by};
        chVecPts[1] = {(lv_value_precise_t)(bx + len * sin(deg2rad(cog))), (lv_value_precise_t)(by - len * cos(deg2rad(cog)))};
        lv_line_set_points(chVec, chVecPts, 2);
    }
    lv_obj_set_hidden(chVec, !vec);
    // Sobre un mapa claro, lineas oscuras
    lv_obj_set_style_line_color(chVec, lv_color_hex(tilesShown ? C_INK : C_TEXT), 0);
    lv_obj_set_style_line_width(chVec, tilesShown ? 3 : 2, 0);
    lv_obj_set_style_bg_color(chScaleBar, lv_color_hex(tilesShown ? C_INK : C_TEXT), 0);

    // Derrota
    int np = 0;
    for (int i = 0; i < trackN; i++) {
        double x, y;
        toScreen(trackPts[i], x, y);
        chTrackPts[np++] = {(lv_value_precise_t)x, (lv_value_precise_t)y};
    }
    if (own && np > 0 && np < kTrack) chTrackPts[np++] = {(lv_value_precise_t)bx, (lv_value_precise_t)by};
    if (np >= 2) lv_line_set_points(chTrack, chTrackPts, (uint32_t)np);
    lv_obj_set_hidden(chTrack, np < 2);

    // Blancos AIS
    AisTargets& tg = links::targets();
    int idx[kChartDots];
    int n = tg.sorted(idx, kChartDots);
    for (int i = 0; i < kChartDots; i++) {
        Dot& d = chDots[i];
        bool vis = false;
        if (i < n) {
            const AisTarget& t = tg.at(idx[i]);
            double x, y;
            if (t.hasPos) {
                toScreen({t.lat, t.lon}, x, y);
                if (inside(x, y)) {
                    vis = true;
                    uint32_t col = t.dangerous ? C_WARN : (t.lost ? 0x5B6B7A : (t.kind == AisKind::Aton ? 0xF2D43D : C_ACC));
                    uint32_t lineCol = !tilesShown ? col : (t.dangerous ? 0xD9480F : 0x1F5FBF);
                    lv_obj_set_style_bg_color(d.dot, lv_color_hex(col), 0);
                    lv_obj_set_pos(d.dot, (int)lround(x) - 7, (int)lround(y) - 7);
                    char b[40];
                    if (t.name[0]) snprintf(b, sizeof(b), "%.18s", t.name);
                    else snprintf(b, sizeof(b), "%09u", (unsigned)t.mmsi);
                    lv_label_set_text(d.num, b);
                    lv_obj_set_style_text_color(d.num, lv_color_hex(col), 0);
                    lv_obj_set_pos(d.num, (int)lround(x) + 10, (int)lround(y) - 18);
                    bool tv = t.hasSog && t.hasCog && t.sogKn > 0.3 && !t.lost;
                    if (tv) {
                        double len = t.sogKn / 10.0 * kMetersPerNm / mpp;
                        d.pts[0] = {(lv_value_precise_t)x, (lv_value_precise_t)y};
                        d.pts[1] = {(lv_value_precise_t)(x + len * sin(deg2rad(t.cogDeg))), (lv_value_precise_t)(y - len * cos(deg2rad(t.cogDeg)))};
                        lv_line_set_points(d.vec, d.pts, 2);
                        lv_obj_set_style_line_color(d.vec, lv_color_hex(lineCol), 0);
                    }
                    lv_obj_set_hidden(d.vec, !tv);
                }
            }
        }
        lv_obj_set_hidden(d.dot, !vis);
        lv_obj_set_hidden(d.num, !vis);
        if (!vis) lv_obj_set_hidden(d.vec, true);
    }

    // Escala: la distancia redonda mas larga que quepa en 180 px
    static const double kSteps[] = {0.05, 0.1, 0.2, 0.5, 1, 2, 5, 10, 20, 50, 100, 200};
    double best = kSteps[0];
    for (double st : kSteps)
        if (st * kMetersPerNm / mpp <= 180) best = st;
    lv_obj_set_width(chScaleBar, (int)lround(best * kMetersPerNm / mpp));
    char b[112];
    snprintf(b, sizeof(b), best < 1 ? "%.2g mn" : "%.0f mn", best);
    comma(b);
    lv_label_set_text(chScaleLbl, b);
    snprintf(b, sizeof(b), "Norte arriba \xC2\xB7 zoom %d \xC2\xB7 %s%s", chZoom,
             tilesShown ? "OpenSeaMap \xC2\xB7 \xC2\xA9 OpenStreetMap" : "sin cartas en la tarjeta para esta zona", chFollow ? "" : " \xC2\xB7 carta movida");
    lv_label_set_text(chInfo, b);
    lv_obj_set_hidden(chCenterBtn, chFollow);
}

// La derrota se guarda aunque la carta no este a la vista
static void sampleTrack() {
    uint32_t now = millis();
    double la, lo;
    if (!store->get(Key::Lat, now, la) || !store->get(Key::Lon, now, lo)) return;
    if (!trackSampler.sample({la, lo}, now)) return;
    if (trackN == kTrack) {                               // llena: se quita un punto de cada dos de la mitad antigua
        int w = 0;
        for (int i = 0; i < kTrack; i++)
            if (i >= kTrack / 2 || i % 2 == 0) trackPts[w++] = trackPts[i];
        trackN = w;
    }
    trackPts[trackN++] = {la, lo};
}

static void placeholder(lv_obj_t* page, const char* name) {
    lv_obj_t* p = box(page, C_PANEL, 10);
    lv_obj_set_size(p, lv_pct(100), lv_pct(100));
    lv_obj_t* a = label(p, name, &dejavu_bold_20, C_TEXT);
    lv_obj_align(a, LV_ALIGN_CENTER, 0, -14);
    lv_obj_t* b = label(p, "En preparaci\xC3\xB3n", &dejavu_16, C_DIM);
    lv_obj_align(b, LV_ALIGN_CENTER, 0, 16);
}

void begin(DataStore* s) {
    store = s;
    lv_obj_t* scr = lv_screen_active();
    lv_obj_set_style_bg_color(scr, lv_color_hex(C_BG), 0);
    lv_obj_set_scrollable(scr, false);

    // Barra superior
    lv_obj_t* top = box(scr, C_BAR);
    lv_obj_set_size(top, board::W, TOP_H);
    lv_obj_align(top, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_set_style_border_color(top, lv_color_hex(C_LINE), 0);
    lv_obj_set_style_border_width(top, 1, 0);
    lv_obj_set_style_border_side(top, LV_BORDER_SIDE_BOTTOM, 0);
    lv_obj_t* title = lblTitle = label(top, "Instrumentos", &dejavu_bold_20, C_TEXT);
    lv_obj_align(title, LV_ALIGN_LEFT_MID, 14, 0);
    lv_obj_t* badge = badgeBox = box(top, C_ACC, 11);
    lv_obj_set_size(badge, LV_SIZE_CONTENT, 22);
    lv_obj_set_style_pad_hor(badge, 10, 0);
    lv_obj_align_to(badge, title, LV_ALIGN_OUT_RIGHT_MID, 12, 0);
    lblBadge = label(badge, "Simulador", &dejavu_14, C_INK);
    lv_obj_center(lblBadge);
    lv_obj_t* mob = lv_button_create(top);
    lv_obj_remove_style_all(mob);
    lv_obj_set_size(mob, 70, TOP_H - 1);
    lv_obj_align(mob, LV_ALIGN_RIGHT_MID, 0, 0);
    lv_obj_set_style_bg_color(mob, lv_color_hex(C_RED), 0);
    lv_obj_set_style_bg_opa(mob, LV_OPA_COVER, 0);
    lv_obj_t* mobL = label(mob, "MOB", &dejavu_bold_16, C_INK);
    lv_obj_center(mobL);
    lblPos = label(top, "Sin posici\xC3\xB3n", &dejavu_16, C_SOFT);
    lv_obj_align(lblPos, LV_ALIGN_RIGHT_MID, -84, 0);

    // Cuerpo: una pagina por pestana
    for (int i = 0; i < 8; i++) {
        pages[i] = lv_obj_create(scr);
        lv_obj_remove_style_all(pages[i]);
        lv_obj_set_size(pages[i], board::W, board::H - TOP_H - NAV_H);
        lv_obj_align(pages[i], LV_ALIGN_TOP_LEFT, 0, TOP_H);
        lv_obj_set_style_pad_all(pages[i], PAD, 0);
        lv_obj_set_scrollable(pages[i], false);
        if (i == 1) makeChart(pages[i]);
        else if (i == 3) makeRadar(pages[i]);
        else if (i > 0) placeholder(pages[i], kTabs[i]);
    }

    // Instrumentos: rejilla de 4 x 2
    static int32_t cols[] = {LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
    static int32_t rows[] = {LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
    lv_obj_t* grid = lv_obj_create(pages[0]);
    lv_obj_remove_style_all(grid);
    lv_obj_set_size(grid, lv_pct(100), lv_pct(100));
    lv_obj_set_layout(grid, LV_LAYOUT_GRID);
    lv_obj_set_grid_dsc_array(grid, cols, rows);
    lv_obj_set_style_pad_gap(grid, PAD, 0);
    lv_obj_set_scrollable(grid, false);
    makeTile(grid, T_SOG, 0, 0, "SOG", "kn");
    makeTile(grid, T_COG, 1, 0, "COG", "\xC2\xB0T");
    makeTile(grid, T_HDG, 2, 0, "RUMBO", "\xC2\xB0T");
    makeTile(grid, T_DEPTH, 3, 0, "PROFUNDIDAD", "m", C_ACC);
    makeTile(grid, T_AWS, 0, 1, "VIENTO AP.", "kn");
    makeTile(grid, T_AWA, 1, 1, "\xC3\x81NGULO AP.", "\xC2\xB0");
    makeTile(grid, T_TWS, 2, 1, "VIENTO REAL", "kn");
    makeTile(grid, T_WATER, 3, 1, "TEMP. AGUA", "\xC2\xB0" "C");

    // Pestanas
    lv_obj_t* nav = box(scr, C_BAR);
    lv_obj_set_size(nav, board::W, NAV_H);
    lv_obj_align(nav, LV_ALIGN_BOTTOM_LEFT, 0, 0);
    lv_obj_set_style_border_color(nav, lv_color_hex(C_LINE), 0);
    lv_obj_set_style_border_width(nav, 1, 0);
    lv_obj_set_style_border_side(nav, LV_BORDER_SIDE_TOP, 0);
    for (int i = 0; i < 8; i++) {
        lv_obj_t* b = lv_button_create(nav);
        lv_obj_remove_style_all(b);
        lv_obj_set_size(b, board::W / 8, NAV_H - 1);
        lv_obj_set_pos(b, i * (board::W / 8), 1);
        lv_obj_set_style_bg_opa(b, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(b, 3, 0);
        lv_obj_set_style_border_side(b, LV_BORDER_SIDE_TOP, 0);
        lv_obj_t* l = label(b, kTabs[i], &dejavu_16, C_SOFT);
        lv_obj_center(l);
        lv_obj_add_event_cb(b, onTab, LV_EVENT_CLICKED, (void*)(intptr_t)i);
        tabBtn[i] = b;
    }
    makeModal();
    showTab(0);
}

static const char* sourceName(Source s) {
    switch (s) {
        case Source::Sensor: return links::status().simulator ? "Simulador" : "Sensor";
        case Source::Can: return "NMEA 2000";
        case Source::Uart: return "Puerto serie";
        case Source::Tcp: return "Red local";
        case Source::Ble: return "Tracker";
        case Source::Internet: return "Internet";
        default: return "Sin datos";
    }
}

// Pone el numero con coma decimal y debajo de donde viene y hace cuanto
static void show(int idx, Key k, int decimals, bool degrees = false) {
    uint32_t now = millis();
    double v;
    if (!store->get(k, now, v)) {
        lv_label_set_text(tiles[idx].value, "--");
        lv_label_set_text(tiles[idx].sub, "Sin datos");
        return;
    }
    char b[24];
    if (degrees) snprintf(b, sizeof(b), "%03d\xC2\xB0", (int)lround(norm360(v)) % 360);
    else {
        snprintf(b, sizeof(b), "%.*f", decimals, v);
        for (char* p = b; *p; p++) if (*p == '.') *p = ',';
    }
    lv_label_set_text(tiles[idx].value, b);
    char s[48];
    snprintf(s, sizeof(s), "%s \xC2\xB7 hace %u s", sourceName(store->source(k, now)), (unsigned)(store->ageMs(k, now) / 1000));
    lv_label_set_text(tiles[idx].sub, s);
}

void update() {
    if (!store) return;
    uint32_t now = millis();
    sampleTrack();
    if (active == 1) updateChart();
    if (active == 3) updateRadar();
    checkCollision();
    updateModal();
    show(T_SOG, Key::Sog, 1);
    show(T_COG, Key::Cog, 0, true);
    show(T_HDG, Key::Heading, 0, true);
    show(T_DEPTH, Key::Depth, 1);
    show(T_AWS, Key::Aws, 1);
    show(T_WATER, Key::WaterTemp, 1);

    // Angulo aparente: babor / estribor
    double awa, aws;
    if (store->get(Key::Awa, now, awa)) {
        char b[24];
        snprintf(b, sizeof(b), "%d\xC2\xB0", (int)lround(fabs(awa)));
        lv_label_set_text(tiles[T_AWA].value, b);
        lv_label_set_text(tiles[T_AWA].sub, awa < 0 ? "Por babor" : "Por estribor");
    } else {
        lv_label_set_text(tiles[T_AWA].value, "--");
        lv_label_set_text(tiles[T_AWA].sub, "Sin datos");
    }

    // Viento real: calculado con el aparente, la velocidad y el rumbo
    double hdg, spd;
    bool haveSpd = store->get(Key::Stw, now, spd) || store->get(Key::Sog, now, spd);
    if (store->get(Key::Awa, now, awa) && store->get(Key::Aws, now, aws) && haveSpd && store->get(Key::Heading, now, hdg)) {
        double twa, tws, twd;
        trueWind(awa, aws, spd, hdg, twa, tws, twd);
        char b[24], s[48];
        snprintf(b, sizeof(b), "%.1f", tws);
        for (char* p = b; *p; p++) if (*p == '.') *p = ',';
        lv_label_set_text(tiles[T_TWS].value, b);
        snprintf(s, sizeof(s), "De %03d\xC2\xB0 \xC2\xB7 fuerza %d \xC2\xB7 calculado", (int)lround(twd) % 360, beaufort(tws));
        lv_label_set_text(tiles[T_TWS].sub, s);
    } else {
        lv_label_set_text(tiles[T_TWS].value, "--");
        lv_label_set_text(tiles[T_TWS].sub, "Falta viento, velocidad o rumbo");
    }

    double la, lo;
    if (store->get(Key::Lat, now, la) && store->get(Key::Lon, now, lo)) {
        char p[48];
        // minutos con apostrofo: la fuente no lleva el signo de minuto
        int n = formatLatLon(p, sizeof(p), LatLon{la, lo}, 2, true);
        if (n) {
            String t(p);
            t.replace("\xE2\x80\xB2", "'");
            lv_label_set_text(lblPos, t.c_str());
        }
    } else {
        lv_label_set_text(lblPos, "Sin posici\xC3\xB3n");
    }
    const links::Status& ls = links::status();
    bool serialAlive = ls.serialLines > 0 && now - ls.serialLastMs < 5000;
    lv_label_set_text(lblBadge, serialAlive ? (ls.simulator ? "Serie + simulador" : "Puerto serie") : (ls.simulator ? "Simulador" : "Sin datos"));
}

void showPage(int tab) {
    if (tab >= 0 && tab < 8) showTab(tab);
}

void closeWindow() {
    if (modalMode != M_NONE) onModalB(nullptr);
}

}
