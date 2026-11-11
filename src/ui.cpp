/*
 * Copyright (C) 2026 Manuel - EA7JRS
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "ui.h"

#include <Arduino.h>
#include <lvgl.h>

#include "board.h"
#include "anchor.h"
#include "units.h"
#include "lights.h"
#include "aistargets.h"
#include "chartui.h"
#include "gpx.h"
#include "links.h"
#include "motion.h"
#include "route.h"
#include "settings.h"
#include "net.h"
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
static lv_obj_t* tileUnit[8] = {};
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
enum ModalMode { M_NONE, M_DETAIL, M_COLLISION, M_MOB, M_ANCHOR };
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
struct BaseMap { const char* name; const char* folder; const char* ext; const char* credit; };
static const BaseMap kBaseMaps[] = {
    {"OpenSeaMap", "S:/charts/osm", "png", "OpenSeaMap \xC2\xB7 \xC2\xA9 OpenStreetMap"},
    {"IHM", "S:/charts/ihm", "png", "IHM \xC2\xB7 no v\xC3\xA1lida para la navegaci\xC3\xB3n"},
    {"IGN", "S:/charts/ign", "jpeg", "IGN base \xC2\xB7 CC BY 4.0 ign.es"},
    {"Ortofoto", "S:/charts/pnoa", "jpeg", "PNOA \xC2\xB7 CC BY 4.0 scne.es"},
};
static const int kBaseMapN = (int)(sizeof(kBaseMaps) / sizeof(kBaseMaps[0]));
static int baseMap = 0;

// Navegacion: punto tocado, waypoints, "ir a" y hombre al agua
static const int kWps = 50, kWpMarks = 20;
struct Wp { LatLon p{0, 0}; char name[20] = ""; };
static Wp wps[kWps];
static int wpN = 0;
static lv_obj_t* wpDot[kWpMarks] = {};
static lv_obj_t* wpLbl[kWpMarks] = {};
static const char* kWpFile = "S:/waypoints.gpx";
static bool selOn = false;
static LatLon selPos{0, 0};
static lv_obj_t* selMark = nullptr;
static RouteFollower nav;
// Fondeo: vigilancia del ancla, panel de la carta y circulo de alarma
static AnchorWatch anchorW;
static bool anchorPanel = false;
static double anchorRode = 30;                            // cadena filada, m
static uint32_t anchorQuietMs = 0;                        // la alerta no vuelve a abrirse hasta entonces
static bool anchorQuiet = false;
static lv_obj_t* anchorCircle = nullptr;
static lv_obj_t* anchorMark = nullptr;
static lv_obj_t* anchorBtnLbl = nullptr;
static const int kRoute = 12;
static LatLon navPts[kRoute];
static char navNames[kRoute][20];
static int navCount = 0;
static bool navOn = false, navMob = false;
// Ruta en preparacion (pestana Rutas o medida pasada a ruta) y medida sobre la carta
static LatLon routePts[kRoute];
static char routeNames[kRoute][20];
static int routeN = 0;
static LatLon measPts[kRoute];
static char measNames[kRoute][20];
static int measN = 0;
static bool measOn = false;
static lv_obj_t* routeLine = nullptr;
static lv_point_precise_t routeLinePts[kRoute];
static lv_obj_t* measLine = nullptr;
static lv_point_precise_t measLinePts[kRoute];
static lv_obj_t* measDot[kRoute] = {};
static char navName[16] = "";
static uint32_t mobMs = 0;
static lv_obj_t* navLine = nullptr;
static lv_point_precise_t navLinePts[2];
static lv_obj_t* navMark = nullptr;
static lv_obj_t* chPanel = nullptr;
static lv_obj_t* chPanelTxt = nullptr;
static lv_obj_t* chPanelBtn[4] = {};
static lv_obj_t* chPanelBtnLbl[4] = {};
static int dragDist = 0;

// Pestana Rutas
static const int kWpRows = 7, kRtRows = 8;
static lv_obj_t* wpRow[kWpRows] = {};
static lv_obj_t* wpRowLbl[kWpRows][3] = {};
static lv_obj_t* wpTitle = nullptr;
static lv_obj_t* rtTitle = nullptr;
static lv_obj_t* rtRowLbl[kRtRows] = {};
static lv_obj_t* rtNavLbl = nullptr;
static int wpPage = 0, wpSel = -1;

// Ajustes
static lv_obj_t* setSimLbl = nullptr;
static lv_obj_t* setUnitsLbl = nullptr;
static lv_obj_t* setCanLbl = nullptr;
// Pantalla de red (WiFi, TCP, UDP) con teclado
static lv_obj_t* netScr = nullptr;
static lv_obj_t* netTa[5] = {};                        // red, clave, servidor, puerto TCP, puerto UDP
static lv_obj_t* netKb = nullptr;
static lv_obj_t* netKindLbl = nullptr;
static lv_obj_t* netStatus = nullptr;
static int netKind = 0;
static lv_obj_t* setMapLbl = nullptr;
static lv_obj_t* setBaudLbl = nullptr;
static lv_obj_t* setBrightLbl = nullptr;
static lv_obj_t* setInfo = nullptr;
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
    tileUnit[idx] = u;
    lv_obj_align(u, LV_ALIGN_TOP_RIGHT, 0, 2);
    tiles[idx].value = label(t, "--", &dejavu_num_60, valueColor);
    lv_obj_align(tiles[idx].value, LV_ALIGN_LEFT_MID, 0, 6);
    tiles[idx].sub = label(t, "Sin datos", &dejavu_12, C_DIM);
    lv_obj_align(tiles[idx].sub, LV_ALIGN_BOTTOM_LEFT, 0, 0);
}

static void comma(char* b);

// Unidades elegidas en Ajustes (nautico / metrico / imperial). Por dentro todo va en mn, kn, m y grados C.
static Units U = unitsFor(UnitSystem::Nautical, Units{});
static const char* distLabel() { return U.dist == DistUnit::Nm ? "mn" : plotter::label(U.dist); }
static void fmtDist(char* b, size_t n, double nm, int decimals) {
    snprintf(b, n, "%.*f %s", decimals, distFromNm(nm, U.dist), distLabel());
    comma(b);
}
static void fmtSpeed(char* b, size_t n, double kn) {
    snprintf(b, n, "%.1f %s", speedFromKn(kn, U.boatSpeed), plotter::label(U.boatSpeed));
    comma(b);
}
static void applyUnits() {
    int u = settings::get().units;
    U = unitsFor(u == 1 ? UnitSystem::Metric : (u == 2 ? UnitSystem::Imperial : UnitSystem::Nautical), Units{});
}

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
        snprintf(b, sizeof(b), i == 2 ? "%.2g %s" : "%.2g", distFromNm(range * (i + 1) / 3, U.dist), distLabel());
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
            fmtDist(b, sizeof(b), t.rangeNm, 1);
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
        else { char c[20]; fmtDist(c, sizeof(c), t.cpaNm, 1); snprintf(b, sizeof(b), "%s \xC2\xB7 %d min", c, (int)lround(t.tcpaMin)); lv_label_set_text(rowLbl[r][4], b); }
        lv_obj_set_style_border_color(rowBox[r], lv_color_hex(t.dangerous ? C_WARN : C_PANEL), 0);
        lv_obj_set_style_bg_color(rowBox[r], lv_color_hex(t.dangerous ? 0x2A2A1C : C_PANEL), 0);
        lv_obj_set_style_text_color(rowLbl[r][4], lv_color_hex(t.dangerous ? C_WARN : (t.lost ? C_DIM : C_TEXT)), 0);
        lv_obj_set_style_text_color(rowLbl[r][0], lv_color_hex(t.dangerous ? C_WARN : C_ACC), 0);
    }
}

// ---------- Ventana centrada ----------
static void updateMobModal();
static void updateAnchorModal();

static void closeModal() {
    modalMode = M_NONE;
    modalMmsi = 0;
    if (modalBack) lv_obj_set_hidden(modalBack, true);
}

static void onModalA(lv_event_t*) {                       // silenciar la alarma de ese buque (o volver a activarla)
    if (modalMode == M_MOB) { closeModal(); return; }      // la navegacion al punto sigue en la carta
    if (modalMode == M_ANCHOR) {                           // silenciar: 10 min sin volver a abrirse
        anchorQuiet = true;
        anchorQuietMs = millis() + 10 * 60000u;
        closeModal();
        return;
    }
    AisTargets& tg = links::targets();
    int i = tg.find(modalMmsi);
    if (i >= 0) tg.silence(modalMmsi, modalMode == M_COLLISION ? true : !tg.at(i).silenced);
    if (modalMode == M_COLLISION) closeModal();
}

static void onModalB(lv_event_t*) {                       // cerrar
    if (modalMode == M_ANCHOR) {                           // cerrar: si sigue garreando vuelve en 2 min
        anchorQuiet = true;
        anchorQuietMs = millis() + 2 * 60000u;
        closeModal();
        return;
    }
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

// Hombre al agua: distancia y demora al punto donde se pulso MOB, y el tiempo transcurrido
static void updateMobModal() {
    if (!navMob) { closeModal(); return; }
    uint32_t now = millis();
    char b[260], pos[48] = "", dist[40] = "--";
    formatLatLon(pos, sizeof(pos), navPts[0], 3, true);
    String p(pos);
    p.replace("\xE2\x80\xB2", "'");
    double la, lo;
    if (store->get(Key::Lat, now, la) && store->get(Key::Lon, now, lo)) {
        Measure m = measure({la, lo}, navPts[0], true);
        if (m.distanceNm < 0.1) snprintf(dist, sizeof(dist), "%d m \xC2\xB7 %03d\xC2\xB0", (int)lround(m.distanceNm * kMetersPerNm), (int)lround(m.bearing) % 360);
        else { char c[20]; fmtDist(c, sizeof(c), m.distanceNm, 2); snprintf(dist, sizeof(dist), "%s \xC2\xB7 %03d\xC2\xB0", c, (int)lround(m.bearing) % 360); }
    }
    uint32_t secs = (now - mobMs) / 1000;
    lv_obj_set_style_bg_color(modalHead, lv_color_hex(C_RED), 0);
    lv_label_set_text(modalTitle, "MAN OVERBOARD");
    lv_label_set_text(modalSub, "MOB \xC2\xB7 steer back to the marked position");
    lv_label_set_text(modalName, dist);
    snprintf(b, sizeof(b), "Distance / bearing to the MOB position\nElapsed:  %u min %02u s\nPosition:  %s", (unsigned)(secs / 60), (unsigned)(secs % 60), p.c_str());
    lv_label_set_text(modalBody, b);
    lv_label_set_text(modalBtnALbl, "SILENCE ALARM");
    lv_label_set_text(modalBtnBLbl, "CLOSE");
}

// Garreo: el barco ha salido del circulo de fondeo (o no hay posicion)
static void updateAnchorModal() {
    if (!anchorW.alarm()) { closeModal(); return; }
    const AnchorState& a = anchorW.state();
    char b[260], head[48];
    lv_obj_set_style_bg_color(modalHead, lv_color_hex(C_RED), 0);
    if (a.noFix && !a.dragging) {
        lv_label_set_text(modalTitle, "ANCHOR WATCH");
        lv_label_set_text(modalSub, "No position \xC2\xB7 the anchor watch cannot see the boat");
        lv_label_set_text(modalName, "NO GPS FIX");
        snprintf(b, sizeof(b), "Last distance to the anchor:  %d m\nAlarm radius:  %d m", (int)lround(a.distanceM), (int)lround(anchorW.radiusM()));
    } else {
        lv_label_set_text(modalTitle, "ANCHOR DRAG");
        lv_label_set_text(modalSub, "Anchor watch \xC2\xB7 boat outside the alarm circle");
        snprintf(head, sizeof(head), "%d m \xC2\xB7 %03d\xC2\xB0", (int)lround(a.distanceM), (int)lround(a.bearing) % 360);
        lv_label_set_text(modalName, head);
        char sogTxt[24] = "--";
        double sog;
        if (store->get(Key::Sog, millis(), sog)) { fmtSpeed(sogTxt, sizeof(sogTxt), sog); }
        snprintf(b, sizeof(b), "Distance / bearing from the anchor\nAlarm radius:  %d m\nMaximum distance:  %d m\nSpeed:  %s",
                 (int)lround(anchorW.radiusM()), (int)lround(a.maxDistanceM), sogTxt);
    }
    lv_label_set_text(modalBody, b);
    lv_label_set_text(modalBtnALbl, "SILENCE 10 MIN");
    lv_label_set_text(modalBtnBLbl, "CLOSE");
}

// Rellena la ventana con el estado actual del blanco; la cierra si el blanco ya no existe o (alerta) ya no hay riesgo
static void updateModal() {
    if (modalMode == M_NONE) return;
    if (modalMode == M_MOB) { updateMobModal(); return; }
    if (modalMode == M_ANCHOR) { updateAnchorModal(); return; }
    AisTargets& tg = links::targets();
    int i = tg.find(modalMmsi);
    if (i < 0) { closeModal(); return; }
    const AisTarget& t = tg.at(i);
    if (modalMode == M_COLLISION && (!t.dangerous || t.silenced)) { closeModal(); return; }
    char b[420], dist[32] = "--", cpaTxt[48] = "--";
    if (t.hasRange) { char c[20]; fmtDist(c, sizeof(c), t.rangeNm, 1); snprintf(dist, sizeof(dist), "%s \xC2\xB7 %03d\xC2\xB0", c, (int)lround(t.bearing) % 360); }
    if (t.lost) snprintf(cpaTxt, sizeof(cpaTxt), "Perdido");
    else if (t.hasCpa && t.tcpaMin >= 0) { char c[20]; fmtDist(c, sizeof(c), t.cpaNm, 2); snprintf(cpaTxt, sizeof(cpaTxt), "%s en %d min", c, (int)lround(t.tcpaMin)); }
    else if (t.hasCpa) snprintf(cpaTxt, sizeof(cpaTxt), "Se aleja");
    if (t.name[0]) lv_label_set_text(modalName, t.name);
    else { snprintf(b, sizeof(b), "MMSI %09u", (unsigned)t.mmsi); lv_label_set_text(modalName, b); }
    char sog[24] = "--", cog[16] = "--", len[24] = "--";
    if (t.hasSog) { fmtSpeed(sog, sizeof(sog), t.sogKn); }
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
    if (modalMode == M_COLLISION || modalMode == M_MOB || modalMode == M_ANCHOR) return;
    if (anchorQuiet && (int32_t)(anchorQuietMs - millis()) <= 0) anchorQuiet = false;
    if (!anchorW.alarm()) anchorQuiet = false;             // al volver al circulo, la siguiente salida avisa otra vez
    else if (!anchorQuiet) { openModal(M_ANCHOR, 0); return; }
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
    dragDist += abs(v.x) + abs(v.y);
    if (dragDist <= 12) return;                           // un toque con el dedo algo movido no arrastra
    double px, py;
    lonLatToWorldPx(chCenter, chZoom, px, py);
    chCenter = worldPxToLonLat(px - v.x, py - v.y, chZoom);
    chFollow = false;
}

static bool tileExists(int z, int x, int y, char* path, int size) {
    for (const Missing& m : missing)
        if (m.z == z && m.x == x && m.y == y) return false;
    if (!tilePath(path, size, kBaseMaps[baseMap].folder, z, x, y, kBaseMaps[baseMap].ext)) return false;
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

// ---------- Waypoints, "ir a" y hombre al agua ----------
static void saveWaypoints() {
    GpxData d;
    for (int i = 0; i < wpN; i++) {
        GpxPoint g;
        g.lat = wps[i].p.lat; g.lon = wps[i].p.lon; g.name = wps[i].name;
        d.waypoints.push_back(g);
    }
    if (routeN > 0) {
        GpxPath r;
        r.name = "Ruta";
        for (int i = 0; i < routeN; i++) {
            GpxPoint g;
            g.lat = routePts[i].lat; g.lon = routePts[i].lon; g.name = routeNames[i];
            r.pts.push_back(g);
        }
        d.routes.push_back(r);
    }
    std::string xml = gpxWrite(d);
    xml.append(256, ' ');                                 // por si el fichero anterior era mas largo y no se trunca
    lv_fs_file_t f;
    if (lv_fs_open(&f, kWpFile, LV_FS_MODE_WR) != LV_FS_RES_OK) return;        // sin tarjeta: se quedan en memoria
    uint32_t bw = 0;
    lv_fs_write(&f, xml.data(), (uint32_t)xml.size(), &bw);
    lv_fs_close(&f);
}

static void loadWaypoints() {
    lv_fs_file_t f;
    if (lv_fs_open(&f, kWpFile, LV_FS_MODE_RD) != LV_FS_RES_OK) return;
    std::string xml;
    char buf[512];
    uint32_t br = 0;
    while (xml.size() < 64000 && lv_fs_read(&f, buf, sizeof(buf), &br) == LV_FS_RES_OK && br > 0) xml.append(buf, br);
    lv_fs_close(&f);
    GpxData d;
    if (!gpxParse(xml, d)) return;
    wpN = 0;
    for (const GpxPoint& g : d.waypoints) {
        if (wpN >= kWps) break;
        wps[wpN].p = {g.lat, g.lon};
        snprintf(wps[wpN].name, sizeof(wps[wpN].name), "%s", g.name.c_str());
        wpN++;
    }
    routeN = 0;
    if (!d.routes.empty())
        for (const GpxPoint& g : d.routes[0].pts) {
            if (routeN >= kRoute) break;
            routePts[routeN] = {g.lat, g.lon};
            snprintf(routeNames[routeN], sizeof(routeNames[routeN]), "%s", g.name.c_str());
            routeN++;
        }
}

static void addWaypoint(LatLon p) {
    if (wpN >= kWps) return;
    int num = 1;                                          // primer numero libre: WPT 01, WPT 02...
    for (bool again = true; again;) {
        again = false;
        char n[20];
        snprintf(n, sizeof(n), "WPT %02d", num % 1000);
        for (int i = 0; i < wpN; i++)
            if (!strcmp(wps[i].name, n)) { num++; again = true; break; }
    }
    wps[wpN].p = p;
    snprintf(wps[wpN].name, sizeof(wps[wpN].name), "WPT %02d", num % 1000);
    wpN++;
    saveWaypoints();
}

static void startGoTo(LatLon dest, const char* name, bool mob) {
    uint32_t now = millis();
    double la, lo;
    LatLon here = dest;
    if (store->get(Key::Lat, now, la) && store->get(Key::Lon, now, lo)) here = {la, lo};
    navPts[0] = dest;
    snprintf(navNames[0], sizeof(navNames[0]), "%s", name);
    navCount = 1;
    nav.setRhumbLine(true);
    nav.setArrivalRadiusNm(mob ? 0.01 : 0.05);
    nav.start(navPts, 1, here);
    navOn = true;
    navMob = mob;
    snprintf(navName, sizeof(navName), "%s", name);
}

// Navegar la ruta preparada: pasa al punto siguiente al quedar el actual por el traves
static void startRoute() {
    if (routeN < 1 || navMob) return;
    uint32_t now = millis();
    double la, lo;
    LatLon here = routePts[0];
    if (store->get(Key::Lat, now, la) && store->get(Key::Lon, now, lo)) here = {la, lo};
    for (int i = 0; i < routeN; i++) { navPts[i] = routePts[i]; snprintf(navNames[i], sizeof(navNames[i]), "%s", routeNames[i]); }
    navCount = routeN;
    nav.setRhumbLine(true);
    nav.setArrivalRadiusNm(0.05);
    nav.setMode(WpSwitch::Abeam);
    nav.start(navPts, navCount, here);
    navOn = true;
    navMob = false;
}

static void stopGoTo() {
    nav.stop();
    navOn = navMob = false;
    navCount = 0;
    if (modalMode == M_MOB) closeModal();
}

// Punto tocado: el waypoint que este a menos de 20 px, o la posicion tal cual
static const char* nearWaypoint(LatLon p);

static void addMeasurePoint(LatLon p) {
    if (measN >= kRoute) return;
    selPos = p;
    const char* n = nearWaypoint(p);                      // sobre un waypoint: se mide (y se rutea) por el waypoint
    measPts[measN] = n ? selPos : p;
    if (n) snprintf(measNames[measN], sizeof(measNames[measN]), "%s", n);
    else snprintf(measNames[measN], sizeof(measNames[measN]), "P%d", measN + 1);
    measN++;
}

static void selectPoint(LatLon p) {
    if (measOn) { addMeasurePoint(p); return; }
    selPos = p;
    selOn = true;
}

static const char* nearWaypoint(LatLon p) {
    double mpp = metersPerPixel(p.lat, chZoom);
    for (int i = 0; i < wpN; i++)
        if (gcDistanceNm(p, wps[i].p) * kMetersPerNm / mpp < 20) { selPos = wps[i].p; return wps[i].name; }
    return nullptr;
}

static double anchorSuggested() {
    double depth = 0;
    if (!store->get(Key::Depth, millis(), depth)) depth = 0;
    return AnchorWatch::suggestedRadiusM(anchorRode, depth);
}

static void saveAnchor() {
    settings::Values& v = settings::get();
    v.anchorOn = anchorW.state().set;
    v.anchorLat = anchorW.anchor().lat;
    v.anchorLon = anchorW.anchor().lon;
    v.anchorRadius = (int)lround(anchorW.radiusM());
    v.anchorRode = (int)lround(anchorRode);
    settings::save();
}

static void onAnchorBtn(lv_event_t*) {
    anchorPanel = !anchorPanel;
    if (anchorPanel) { selOn = false; measOn = false; measN = 0; }
}

static void onPanelBtn(lv_event_t* e) {
    int b = (int)(intptr_t)lv_event_get_user_data(e);
    if (anchorPanel && !measOn && !selOn) {
        if (anchorW.state().set) {                        // - 5 m / + 5 m (radio) / Levar / Cerrar
            if (b == 0) anchorW.setRadius(anchorW.radiusM() - 5);
            else if (b == 1) anchorW.setRadius(anchorW.radiusM() + 5);
            else if (b == 2) { anchorW.weigh(); anchorPanel = false; }
            else anchorPanel = false;
            if (b <= 2) saveAnchor();
        } else {                                          // - 5 m / + 5 m (cadena) / Fondear / Cerrar
            if (b == 0) { if (anchorRode > 5) anchorRode -= 5; }
            else if (b == 1) { if (anchorRode < 200) anchorRode += 5; }
            else if (b == 2) {
                uint32_t now = millis();
                double la, lo;
                if (store->get(Key::Lat, now, la) && store->get(Key::Lon, now, lo)) {
                    anchorW.drop({la, lo}, anchorSuggested(), now);
                    anchorQuiet = false;
                    saveAnchor();
                    if (chZoom < 17) chZoom = 17;          // que se vea el circulo
                    chFollow = true;
                }
            } else anchorPanel = false;
        }
        return;
    }
    if (measOn) {                                         // Deshacer / A ruta / Salir
        if (b == 0) { if (measN > 0) measN--; }
        else if (b == 1) {
            if (measN >= 2) {
                for (int i = 0; i < measN; i++) { routePts[i] = measPts[i]; snprintf(routeNames[i], sizeof(routeNames[i]), "%s", measNames[i]); }
                routeN = measN;
                saveWaypoints();
                measOn = false;
                measN = 0;
                showTab(2);
            }
        } else { measOn = false; measN = 0; }
    } else if (selOn) {                                   // Ir a / Marcar / Medir / Cerrar
        if (b == 0) {
            const char* n = nearWaypoint(selPos);
            startGoTo(selPos, n ? n : "Punto", false);
        } else if (b == 1) {
            if (!nearWaypoint(selPos)) addWaypoint(selPos);
        } else if (b == 2) {
            measOn = true;
            measN = 0;
            addMeasurePoint(selPos);
        }
        selOn = false;
    } else if (navOn) {                                   // Parar / Destino / Siguiente
        if (b == 0) stopGoTo();
        else if (b == 1) { chCenter = navPts[nav.status().target]; chFollow = false; }
        else if (b == 2) {
            uint32_t now = millis();
            double la, lo;
            if (store->get(Key::Lat, now, la) && store->get(Key::Lon, now, lo)) nav.skip({la, lo});
        }
    }
}

static void onChartPressed(lv_event_t*) { dragDist = 0; }

static void onChartClicked(lv_event_t*) {
    if (dragDist > 12) return;                            // era un arrastre, no un toque
    lv_indev_t* in = lv_indev_active();
    if (!in) return;
    lv_point_t pt;
    lv_indev_get_point(in, &pt);
    lv_area_t a;
    lv_obj_get_coords(chart, &a);
    double cx, cy;
    lonLatToWorldPx(chCenter, chZoom, cx, cy);
    selectPoint(worldPxToLonLat(cx + (pt.x - a.x1) - CH_W / 2.0, cy + (pt.y - a.y1) - CH_H / 2.0, chZoom));
}

static void makeChart(lv_obj_t* page) {
    chart = box(page, 0x0E2A44, 10);
    lv_obj_set_size(chart, CH_W, CH_H);
    lv_obj_center(chart);
    lv_obj_set_style_clip_corner(chart, true, 0);
    lv_obj_set_clickable(chart, true);
    lv_obj_add_event_cb(chart, onChartDrag, LV_EVENT_PRESSING, nullptr);
    lv_obj_add_event_cb(chart, onChartPressed, LV_EVENT_PRESSED, nullptr);
    lv_obj_add_event_cb(chart, onChartClicked, LV_EVENT_CLICKED, nullptr);
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
    routeLine = lv_line_create(chart);
    lv_obj_set_style_line_color(routeLine, lv_color_hex(0xFF9F43), 0);
    lv_obj_set_style_line_width(routeLine, 2, 0);
    measLine = lv_line_create(chart);
    lv_obj_set_style_line_color(measLine, lv_color_hex(C_WARN), 0);
    lv_obj_set_style_line_width(measLine, 3, 0);
    lv_obj_set_style_line_dash_width(measLine, 4, 0);
    lv_obj_set_style_line_dash_gap(measLine, 6, 0);
    for (int i = 0; i < kRoute; i++) {
        measDot[i] = box(chart, C_INK, LV_RADIUS_CIRCLE);
        lv_obj_set_size(measDot[i], 16, 16);
        lv_obj_set_style_border_color(measDot[i], lv_color_hex(C_WARN), 0);
        lv_obj_set_style_border_width(measDot[i], 3, 0);
        lv_obj_set_hidden(measDot[i], true);
    }
    navLine = lv_line_create(chart);
    lv_obj_set_style_line_color(navLine, lv_color_hex(0xFF9F43), 0);
    lv_obj_set_style_line_width(navLine, 3, 0);
    lv_obj_set_style_line_dash_width(navLine, 10, 0);
    lv_obj_set_style_line_dash_gap(navLine, 6, 0);
    for (int i = 0; i < kWpMarks; i++) {
        wpDot[i] = box(chart, 0xFF9F43, 3);
        lv_obj_set_size(wpDot[i], 12, 12);
        lv_obj_set_style_border_color(wpDot[i], lv_color_hex(C_INK), 0);
        lv_obj_set_style_border_width(wpDot[i], 2, 0);
        wpLbl[i] = label(chart, "", &dejavu_12, 0xFFD3A1);
        lv_obj_set_style_bg_color(wpLbl[i], lv_color_hex(C_BAR), 0);
        lv_obj_set_style_bg_opa(wpLbl[i], 200, 0);
        lv_obj_set_style_pad_hor(wpLbl[i], 4, 0);
        lv_obj_set_style_radius(wpLbl[i], 4, 0);
        lv_obj_set_hidden(wpDot[i], true);
        lv_obj_set_hidden(wpLbl[i], true);
    }
    navMark = box(chart, 0xFF9F43, LV_RADIUS_CIRCLE);
    lv_obj_set_size(navMark, 20, 20);
    lv_obj_set_style_border_color(navMark, lv_color_hex(C_INK), 0);
    lv_obj_set_style_border_width(navMark, 3, 0);
    selMark = lv_obj_create(chart);
    lv_obj_remove_style_all(selMark);
    lv_obj_set_size(selMark, 26, 26);
    lv_obj_set_style_radius(selMark, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_color(selMark, lv_color_hex(C_WARN), 0);
    lv_obj_set_style_border_width(selMark, 3, 0);
    lv_obj_set_scrollable(selMark, false);
    chBoat = box(chart, C_TEXT, LV_RADIUS_CIRCLE);
    lv_obj_set_size(chBoat, 16, 16);
    lv_obj_set_style_border_color(chBoat, lv_color_hex(C_INK), 0);
    lv_obj_set_style_border_width(chBoat, 2, 0);

    lv_obj_t* plus = textButton(chart, "+", 52, 52, onZoom, 1);
    lv_obj_align(plus, LV_ALIGN_TOP_RIGHT, -8, 8);
    lv_obj_t* minus = textButton(chart, "-", 52, 52, onZoom, -1);
    lv_obj_align(minus, LV_ALIGN_TOP_RIGHT, -8, 68);
    anchorCircle = lv_obj_create(chart);
    lv_obj_remove_style_all(anchorCircle);
    lv_obj_set_style_radius(anchorCircle, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_width(anchorCircle, 3, 0);
    lv_obj_set_style_bg_color(anchorCircle, lv_color_hex(0x2ECC71), 0);
    lv_obj_set_style_bg_opa(anchorCircle, 30, 0);
    lv_obj_set_hidden(anchorCircle, true);
    anchorMark = box(chart, C_INK, 3);
    lv_obj_set_size(anchorMark, 12, 12);
    lv_obj_set_style_border_color(anchorMark, lv_color_hex(0x2ECC71), 0);
    lv_obj_set_style_border_width(anchorMark, 3, 0);
    lv_obj_set_hidden(anchorMark, true);
    lv_obj_set_clickable(anchorCircle, false);            // los toques pasan a la carta
    lv_obj_set_scrollable(anchorCircle, false);
    lv_obj_t* ab = textButton(chart, "Fondeo", 110, 46, onAnchorBtn, 0);
    lv_obj_align(ab, LV_ALIGN_TOP_RIGHT, -68, 68);
    anchorBtnLbl = lv_obj_get_child(ab, 0);
    chCenterBtn = textButton(chart, "Centrar barco", 170, 46, onCenter, 0);
    lv_obj_align(chCenterBtn, LV_ALIGN_TOP_RIGHT, -68, 8);
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
    chPanel = box(chart, C_PANEL, 10);
    lv_obj_set_size(chPanel, 744, 62);
    lv_obj_align(chPanel, LV_ALIGN_BOTTOM_RIGHT, -8, -8);
    lv_obj_set_style_border_width(chPanel, 2, 0);
    lv_obj_set_clickable(chPanel, true);                  // tocar el panel no marca un punto en la carta
    chPanelTxt = label(chPanel, "", &dejavu_14, C_TEXT);
    lv_obj_align(chPanelTxt, LV_ALIGN_LEFT_MID, 12, 0);
    lv_obj_set_width(chPanelTxt, 308);
    lv_obj_set_style_text_line_space(chPanelTxt, 4, 0);
    for (int i = 0; i < 4; i++) {
        chPanelBtn[i] = textButton(chPanel, "Cerrar", 96, 46, onPanelBtn, i);
        lv_obj_align(chPanelBtn[i], LV_ALIGN_RIGHT_MID, -8 - (3 - i) * 102, 0);
        chPanelBtnLbl[i] = lv_obj_get_child(chPanelBtn[i], 0);
    }
    lv_obj_set_hidden(chPanel, true);
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

    // Waypoints a la vista
    int wm = 0;
    for (int i = 0; i < wpN && wm < kWpMarks; i++) {
        double x, y;
        toScreen(wps[i].p, x, y);
        if (!inside(x, y)) continue;
        lv_obj_set_pos(wpDot[wm], (int)lround(x) - 6, (int)lround(y) - 6);
        lv_label_set_text(wpLbl[wm], wps[i].name);
        lv_obj_set_pos(wpLbl[wm], (int)lround(x) + 9, (int)lround(y) - 8);
        lv_obj_set_hidden(wpDot[wm], false);
        lv_obj_set_hidden(wpLbl[wm], false);
        wm++;
    }
    for (int i = wm; i < kWpMarks; i++) { lv_obj_set_hidden(wpDot[i], true); lv_obj_set_hidden(wpLbl[i], true); }

    // Destino y linea desde el barco
    if (navOn) {
        double x, y;
        toScreen(navPts[nav.status().target], x, y);
        lv_obj_set_pos(navMark, (int)lround(x) - 10, (int)lround(y) - 10);
        lv_obj_set_style_bg_color(navMark, lv_color_hex(navMob ? C_RED : 0xFF9F43), 0);
        lv_obj_set_style_line_color(navLine, lv_color_hex(navMob ? C_RED : 0xFF9F43), 0);
        if (own) {
            navLinePts[0] = {(lv_value_precise_t)bx, (lv_value_precise_t)by};
            navLinePts[1] = {(lv_value_precise_t)x, (lv_value_precise_t)y};
            lv_line_set_points(navLine, navLinePts, 2);
        }
        lv_obj_set_hidden(navMark, !inside(x, y));
        lv_obj_set_hidden(navLine, !own);
    } else {
        lv_obj_set_hidden(navMark, true);
        lv_obj_set_hidden(navLine, true);
    }

    // Ruta: la que se navega o, si no, la preparada
    {
        const LatLon* rp = (navOn && navCount > 1) ? navPts : routePts;
        int rn = (navOn && navCount > 1) ? navCount : (navOn ? 0 : routeN);
        for (int i = 0; i < rn; i++) {
            double x, y;
            toScreen(rp[i], x, y);
            routeLinePts[i] = {(lv_value_precise_t)x, (lv_value_precise_t)y};
        }
        if (rn >= 2) lv_line_set_points(routeLine, routeLinePts, (uint32_t)rn);
        lv_obj_set_hidden(routeLine, rn < 2);
    }
    // Medida
    for (int i = 0; i < kRoute; i++) {
        bool vis = measOn && i < measN;
        if (vis) {
            double x, y;
            toScreen(measPts[i], x, y);
            measLinePts[i] = {(lv_value_precise_t)x, (lv_value_precise_t)y};
            lv_obj_set_pos(measDot[i], (int)lround(x) - 8, (int)lround(y) - 8);
        }
        lv_obj_set_hidden(measDot[i], !vis);
    }
    if (measOn && measN >= 2) lv_line_set_points(measLine, measLinePts, (uint32_t)measN);
    lv_obj_set_hidden(measLine, !(measOn && measN >= 2));

    // Punto tocado y panel de abajo
    if (selOn) {
        double x, y;
        toScreen(selPos, x, y);
        lv_obj_set_pos(selMark, (int)lround(x) - 13, (int)lround(y) - 13);
    }
    lv_obj_set_hidden(selMark, !selOn);
    // Fondeo: circulo de alarma y ancla
    {
        const AnchorState& a = anchorW.state();
        if (a.set) {
            double x, y;
            toScreen(anchorW.anchor(), x, y);
            int r = (int)lround(anchorW.radiusM() / mpp);
            bool vis = r >= 4 && r <= 1500 && x > -r && y > -r && x < 1100 + r && y < 600 + r;
            uint32_t col = anchorW.alarm() ? C_RED : 0x2ECC71;
            lv_obj_set_size(anchorCircle, 2 * r, 2 * r);
            lv_obj_set_pos(anchorCircle, (int)lround(x) - r, (int)lround(y) - r);
            lv_obj_set_style_border_color(anchorCircle, lv_color_hex(col), 0);
            lv_obj_set_style_bg_color(anchorCircle, lv_color_hex(col), 0);
            lv_obj_set_hidden(anchorCircle, !vis);
            lv_obj_set_pos(anchorMark, (int)lround(x) - 6, (int)lround(y) - 6);
            lv_obj_set_style_border_color(anchorMark, lv_color_hex(col), 0);
            lv_obj_set_hidden(anchorMark, !inside(x, y));
        } else {
            lv_obj_set_hidden(anchorCircle, true);
            lv_obj_set_hidden(anchorMark, true);
        }
        lv_obj_set_style_text_color(anchorBtnLbl, lv_color_hex(anchorW.alarm() ? C_RED : (a.set ? 0x2ECC71 : C_TEXT)), 0);
    }
    bool anchorShow = anchorPanel && !measOn && !selOn;
    char pt[200];
    if (anchorShow) {
        const AnchorState& a = anchorW.state();
        if (a.set) {
            const char* stTxt = a.dragging ? "GARREO" : (a.noFix ? "SIN POSICI\xC3\x93N" : "Fondeado");
            snprintf(pt, sizeof(pt), "%s \xC2\xB7 a %d m del ancla \xC2\xB7 %03d\xC2\xB0\nRadio de alarma %d m \xC2\xB7 m\xC3\xA1ximo %d m", stTxt,
                     (int)lround(a.distanceM), (int)lround(a.bearing) % 360, (int)lround(anchorW.radiusM()), (int)lround(a.maxDistanceM));
            lv_label_set_text(chPanelBtnLbl[2], "Levar");
        } else {
            double depth = 0;
            bool hasDepth = store->get(Key::Depth, now, depth) && depth > 0;
            char l1[96];
            if (hasDepth) {
                char dp[16], sc[16];
                snprintf(dp, sizeof(dp), "%.1f", depth); comma(dp);
                snprintf(sc, sizeof(sc), "%.1f", scopeRatio(anchorRode, depth, 1.0)); comma(sc);
                snprintf(l1, sizeof(l1), "Cadena %d m \xC2\xB7 fondo %s m \xC2\xB7 relaci\xC3\xB3n %s", (int)lround(anchorRode), dp, sc);
            } else snprintf(l1, sizeof(l1), "Cadena %d m \xC2\xB7 sin sonda", (int)lround(anchorRode));
            snprintf(pt, sizeof(pt), "%s\nRadio de alarma %d m \xC2\xB7 pulsa al largar el ancla", l1, (int)lround(anchorSuggested()));
            lv_label_set_text(chPanelBtnLbl[2], "Fondear");
        }
        lv_label_set_text(chPanelTxt, pt);
        lv_label_set_text(chPanelBtnLbl[0], "- 5 m");
        lv_label_set_text(chPanelBtnLbl[1], "+ 5 m");
        lv_label_set_text(chPanelBtnLbl[3], "Cerrar");
        for (int i = 0; i < 4; i++) lv_obj_set_hidden(chPanelBtn[i], false);
        lv_obj_set_style_border_color(chPanel, lv_color_hex(anchorW.alarm() ? C_RED : 0x2ECC71), 0);
    } else if (measOn) {
        double total = pathLengthNm(measPts, measN, true);
        char tt[24], l2[96] = "Toca la carta para a\xC3\xB1" "adir puntos";
        fmtDist(tt, sizeof(tt), total, 2);
        if (measN >= 2) {
            Measure m = measure(measPts[measN - 2], measPts[measN - 1], true, sog);
            char d[24], t[24] = "";
            fmtDist(d, sizeof(d), m.distanceNm, 2);
            if (sog > 0.5) formatDuration(t, sizeof(t), total / sog);
            snprintf(l2, sizeof(l2), "Tramo %s \xC2\xB7 %03d\xC2\xB0%s%s", d, (int)lround(m.bearing) % 360, t[0] ? " \xC2\xB7 total " : "", t);
        }
        snprintf(pt, sizeof(pt), "Medida \xC2\xB7 %d puntos \xC2\xB7 %s\n%s", measN, tt, l2);
        lv_label_set_text(chPanelTxt, pt);
        lv_label_set_text(chPanelBtnLbl[0], "Deshacer");
        lv_label_set_text(chPanelBtnLbl[1], "A ruta");
        lv_label_set_text(chPanelBtnLbl[2], "Salir");
        lv_obj_set_hidden(chPanelBtn[1], measN < 2);
        lv_obj_set_hidden(chPanelBtn[2], false);
        lv_obj_set_hidden(chPanelBtn[3], true);
        lv_obj_set_style_border_color(chPanel, lv_color_hex(C_WARN), 0);
    } else if (selOn) {
        char pos[48] = "";
        formatLatLon(pos, sizeof(pos), selPos, 3, true);
        String p(pos);
        p.replace("\xE2\x80\xB2", "'");
        const char* wn = nullptr;
        for (int i = 0; i < wpN; i++)
            if (fabs(wps[i].p.lat - selPos.lat) < 1e-9 && fabs(wps[i].p.lon - selPos.lon) < 1e-9) wn = wps[i].name;
        if (!wn) {                                        // el toque cae sobre un waypoint: se elige el waypoint
            LatLon keep = selPos;
            wn = nearWaypoint(selPos);
            if (!wn) selPos = keep;
        }
        if (own) {
            Measure m = measure({la, lo}, selPos, true, sog);
            char d[24], t[24] = "";
            fmtDist(d, sizeof(d), m.distanceNm, 2);
            if (m.hours >= 0) formatDuration(t, sizeof(t), m.hours);
            snprintf(pt, sizeof(pt), "%s%s%s\n%s \xC2\xB7 %03d\xC2\xB0%s%s", wn ? wn : "", wn ? " \xC2\xB7 " : "", p.c_str(), d, (int)lround(m.bearing) % 360,
                     t[0] ? " \xC2\xB7 " : "", t);
        } else {
            snprintf(pt, sizeof(pt), "%s%s%s\nSin posici\xC3\xB3n propia", wn ? wn : "", wn ? " \xC2\xB7 " : "", p.c_str());
        }
        lv_label_set_text(chPanelTxt, pt);
        lv_label_set_text(chPanelBtnLbl[0], "Ir a");
        lv_label_set_text(chPanelBtnLbl[1], "Marcar");
        lv_label_set_text(chPanelBtnLbl[2], "Medir");
        lv_label_set_text(chPanelBtnLbl[3], "Cerrar");
        lv_obj_set_hidden(chPanelBtn[1], wn != nullptr);
        lv_obj_set_hidden(chPanelBtn[2], false);
        lv_obj_set_hidden(chPanelBtn[3], false);
        lv_obj_set_style_border_color(chPanel, lv_color_hex(C_WARN), 0);
    } else if (navOn) {
        const RouteStatus& st = nav.status();
        char d[24], t[24] = "--", x[64];
        if (st.distanceNm < 0.1) snprintf(d, sizeof(d), "%d m", (int)lround(st.distanceNm * kMetersPerNm));
        else { fmtDist(d, sizeof(d), st.distanceNm, 2); }
        if (sog > 0.5) formatDuration(t, sizeof(t), st.distanceNm / sog);
        if (navMob) {
            uint32_t secs = (now - mobMs) / 1000;
            snprintf(x, sizeof(x), "Hombre al agua hace %u min %02u s", (unsigned)(secs / 60), (unsigned)(secs % 60));
        } else if (fabs(st.xteNm) < 0.005) snprintf(x, sizeof(x), "En la derrota");
        else { char c[20]; fmtDist(c, sizeof(c), fabs(st.xteNm), 2); snprintf(x, sizeof(x), "Desv\xC3\xADo %s a %s", c, st.xteNm > 0 ? "estribor" : "babor"); }
        char who[40];
        if (navCount > 1) snprintf(who, sizeof(who), "%s (%d de %d)", navNames[st.target], st.target + 1, navCount);
        else snprintf(who, sizeof(who), "%s", navName);
        bool arrived = !navMob && (navCount > 1 ? st.finished : st.inArrival);
        snprintf(pt, sizeof(pt), "%s%s \xC2\xB7 %s \xC2\xB7 %03d\xC2\xB0 \xC2\xB7 %s\n%s", arrived ? "LLEGADA \xC2\xB7 " : "A ", who, d,
                 (int)lround(st.bearing) % 360, t, x);
        lv_label_set_text(chPanelTxt, pt);
        lv_label_set_text(chPanelBtnLbl[0], "Parar");
        lv_label_set_text(chPanelBtnLbl[1], "Destino");
        lv_label_set_text(chPanelBtnLbl[2], "Siguiente");
        lv_obj_set_hidden(chPanelBtn[1], false);
        lv_obj_set_hidden(chPanelBtn[2], !(navCount > 1 && st.target < navCount - 1));
        lv_obj_set_hidden(chPanelBtn[3], true);
        lv_obj_set_style_border_color(chPanel, lv_color_hex(navMob ? C_RED : 0xFF9F43), 0);
    }
    {                                                     // los botones visibles, pegados a la derecha
        int slot = 0;
        for (int i = 3; i >= 0; i--) {
            if (lv_obj_is_hidden(chPanelBtn[i])) continue;
            lv_obj_align(chPanelBtn[i], LV_ALIGN_RIGHT_MID, -8 - slot * 102, 0);
            slot++;
        }
        lv_obj_set_width(chPanelTxt, 744 - 24 - slot * 102 - 8);
    }
    lv_obj_set_hidden(chPanel, !(anchorShow || measOn || selOn || navOn));

    // Escala: la distancia redonda mas larga que quepa en 180 px
    static const double kSteps[] = {0.05, 0.1, 0.2, 0.5, 1, 2, 5, 10, 20, 50, 100, 200};
    double best = kSteps[0];
    for (double st : kSteps)
        if (st / distFromNm(1, U.dist) * kMetersPerNm / mpp <= 180) best = st;       // pasos redondos en la unidad elegida
    lv_obj_set_width(chScaleBar, (int)lround(best / distFromNm(1, U.dist) * kMetersPerNm / mpp));
    char b[112];
    snprintf(b, sizeof(b), best < 1 ? "%.2g %s" : "%.0f %s", best, distLabel());
    comma(b);
    lv_label_set_text(chScaleLbl, b);
    snprintf(b, sizeof(b), "Norte arriba \xC2\xB7 zoom %d \xC2\xB7 %s%s", chZoom,
             tilesShown ? kBaseMaps[baseMap].credit : "sin cartas en la tarjeta para esta zona", chFollow ? "" : " \xC2\xB7 carta movida");
    lv_label_set_text(chInfo, b);
    lv_obj_set_hidden(chCenterBtn, chFollow);
}

// El seguimiento del destino va aunque la carta no este a la vista
static void updateNav() {
    uint32_t now = millis();
    double la, lo;
    bool own = store->get(Key::Lat, now, la) && store->get(Key::Lon, now, lo);
    if (anchorW.state().set) {
        if (own) anchorW.update({la, lo}, now);
        anchorW.tick(now);
    }
    if (navOn && own) nav.update({la, lo}, now);
}

// ---------- Pestana Rutas ----------
static void onWpRow(lv_event_t* e) {
    int i = wpPage * kWpRows + (int)(intptr_t)lv_event_get_user_data(e);
    wpSel = (i < wpN && i != wpSel) ? i : -1;
}

static void onRoutes(lv_event_t* e) {
    int a = (int)(intptr_t)lv_event_get_user_data(e);
    int pages = (wpN + kWpRows - 1) / kWpRows;
    switch (a) {
        case 0: if (wpPage > 0) wpPage--; break;
        case 1: if (wpPage < pages - 1) wpPage++; break;
        case 2: if (wpSel >= 0) { startGoTo(wps[wpSel].p, wps[wpSel].name, false); showTab(1); } break;          // Ir a
        case 3: if (wpSel >= 0) { chCenter = wps[wpSel].p; chFollow = false; showTab(1); } break;                // Ver en la carta
        case 4:                                                                                                   // A la ruta
            if (wpSel >= 0 && routeN < kRoute) {
                routePts[routeN] = wps[wpSel].p;
                snprintf(routeNames[routeN], sizeof(routeNames[routeN]), "%s", wps[wpSel].name);
                routeN++;
                saveWaypoints();
            }
            break;
        case 5:                                                                                                   // Borrar
            if (wpSel >= 0) {
                for (int i = wpSel; i < wpN - 1; i++) wps[i] = wps[i + 1];
                wpN--;
                wpSel = -1;
                if (wpPage > 0 && wpPage * kWpRows >= wpN) wpPage--;
                saveWaypoints();
            }
            break;
        case 6:                                                                                                   // Navegar / Parar
            if (navOn && navCount > 1) stopGoTo();
            else if (routeN >= 1) { startRoute(); showTab(1); }
            break;
        case 7: if (routeN > 0) { routeN--; saveWaypoints(); } break;                                             // Quitar el ultimo
        case 8: routeN = 0; saveWaypoints(); break;                                                               // Vaciar
    }
}

static void makeRoutes(lv_obj_t* page) {
    // Izquierda: waypoints
    lv_obj_t* L = box(page, C_PANEL, 10);
    lv_obj_set_size(L, 496, 476);
    lv_obj_set_pos(L, 0, 0);
    wpTitle = label(L, "Waypoints", &dejavu_bold_20, C_TEXT);
    lv_obj_align(wpTitle, LV_ALIGN_TOP_LEFT, 14, 12);
    lv_obj_t* b = textButton(L, "<", 52, 40, onRoutes, 0);
    lv_obj_align(b, LV_ALIGN_TOP_RIGHT, -70, 6);
    b = textButton(L, ">", 52, 40, onRoutes, 1);
    lv_obj_align(b, LV_ALIGN_TOP_RIGHT, -12, 6);
    static const int cx[3] = {12, 250, 360};
    for (int r = 0; r < kWpRows; r++) {
        wpRow[r] = box(L, C_BAR, 8);
        lv_obj_set_size(wpRow[r], 472, 42);
        lv_obj_set_pos(wpRow[r], 12, 54 + r * 48);
        lv_obj_set_style_border_width(wpRow[r], 2, 0);
        lv_obj_set_clickable(wpRow[r], true);
        lv_obj_add_event_cb(wpRow[r], onWpRow, LV_EVENT_CLICKED, (void*)(intptr_t)r);
        for (int c = 0; c < 3; c++) {
            wpRowLbl[r][c] = label(wpRow[r], "", c == 0 ? &dejavu_bold_16 : &dejavu_16, C_TEXT);
            lv_obj_align(wpRowLbl[r][c], LV_ALIGN_LEFT_MID, cx[c], 0);
        }
    }
    static const char* const wb[4] = {"Ir a", "Ver", "A la ruta", "Borrar"};
    for (int i = 0; i < 4; i++) {
        b = textButton(L, wb[i], 112, 48, onRoutes, 2 + i);
        lv_obj_set_pos(b, 12 + i * 120, 416);
    }
    // Derecha: la ruta
    lv_obj_t* R = box(page, C_PANEL, 10);
    lv_obj_set_size(R, 498, 476);
    lv_obj_set_pos(R, 506, 0);
    rtTitle = label(R, "Ruta", &dejavu_bold_20, C_TEXT);
    lv_obj_align(rtTitle, LV_ALIGN_TOP_LEFT, 14, 12);
    for (int r = 0; r < kRtRows; r++) {
        rtRowLbl[r] = label(R, "", &dejavu_16, C_SOFT);
        lv_obj_set_pos(rtRowLbl[r], 14, 54 + r * 42);
    }
    b = textButton(R, "Navegar", 150, 48, onRoutes, 6);
    lv_obj_set_pos(b, 12, 416);
    rtNavLbl = lv_obj_get_child(b, 0);
    b = textButton(R, "Quitar \xC3\xBAltimo", 160, 48, onRoutes, 7);
    lv_obj_set_pos(b, 170, 416);
    b = textButton(R, "Vaciar", 150, 48, onRoutes, 8);
    lv_obj_set_pos(b, 338, 416);
}

static void updateRoutes() {
    uint32_t now = millis();
    double la, lo;
    bool own = store->get(Key::Lat, now, la) && store->get(Key::Lon, now, lo);
    char b[96];
    int pages = (wpN + kWpRows - 1) / kWpRows;
    if (pages > 1) snprintf(b, sizeof(b), "Waypoints \xC2\xB7 %d \xC2\xB7 p\xC3\xA1gina %d de %d", wpN, wpPage + 1, pages);
    else snprintf(b, sizeof(b), "Waypoints \xC2\xB7 %d", wpN);
    lv_label_set_text(wpTitle, b);
    for (int r = 0; r < kWpRows; r++) {
        int i = wpPage * kWpRows + r;
        if (i >= wpN) { lv_obj_set_hidden(wpRow[r], true); continue; }
        lv_obj_set_hidden(wpRow[r], false);
        lv_label_set_text(wpRowLbl[r][0], wps[i].name);
        if (own) {
            Measure m = measure({la, lo}, wps[i].p, true);
            fmtDist(b, sizeof(b), m.distanceNm, 2);
            lv_label_set_text(wpRowLbl[r][1], b);
            snprintf(b, sizeof(b), "%03d\xC2\xB0", (int)lround(m.bearing) % 360);
            lv_label_set_text(wpRowLbl[r][2], b);
        } else {
            lv_label_set_text(wpRowLbl[r][1], "--");
            lv_label_set_text(wpRowLbl[r][2], "--");
        }
        lv_obj_set_style_border_color(wpRow[r], lv_color_hex(i == wpSel ? C_ACC : C_BAR), 0);
    }
    bool running = navOn && navCount > 1;
    if (routeN == 0) snprintf(b, sizeof(b), "Ruta \xC2\xB7 vac\xC3\xAD" "a");
    else {
        char t[24];
        fmtDist(t, sizeof(t), pathLengthNm(routePts, routeN, true), 2);
        snprintf(b, sizeof(b), "Ruta \xC2\xB7 %d puntos \xC2\xB7 %s%s", routeN, t, running ? " \xC2\xB7 navegando" : "");
    }
    lv_label_set_text(rtTitle, b);
    int first = routeN > kRtRows ? routeN - kRtRows : 0;       // si no caben, los ultimos
    for (int r = 0; r < kRtRows; r++) {
        int i = first + r;
        if (i >= routeN) {
            lv_label_set_text(rtRowLbl[r], (r == 0 && routeN == 0) ? "Elige un waypoint y pulsa \xC2\xAB" "A la ruta\xC2\xBB,\no mide en la carta y pulsa \xC2\xAB" "A ruta\xC2\xBB." : "");
            continue;
        }
        if (i == 0) snprintf(b, sizeof(b), "1   %s", routeNames[0]);
        else {
            Measure m = measure(routePts[i - 1], routePts[i], true);
            char d[24];
            fmtDist(d, sizeof(d), m.distanceNm, 2);
            snprintf(b, sizeof(b), "%d   %s   \xC2\xB7   %s   \xC2\xB7   %03d\xC2\xB0", i + 1, routeNames[i], d, (int)lround(m.bearing) % 360);
        }
        lv_label_set_text(rtRowLbl[r], b);
        bool cur = running && i == nav.status().target;
        lv_obj_set_style_text_color(rtRowLbl[r], lv_color_hex(cur ? 0xFF9F43 : C_SOFT), 0);
    }
    lv_label_set_text(rtNavLbl, running ? "Parar" : "Navegar");
}

// ---------- Red: WiFi, servidor TCP y escucha UDP ----------
static const char* const kTcpKinds[] = {"TCP apagado", "TCP: NMEA 0183", "TCP: gpsd"};

static void onNetField(lv_event_t* e) { lv_keyboard_set_textarea(netKb, (lv_obj_t*)lv_event_get_target(e)); }

static void openNet() {
    const settings::Values& v = settings::get();
    char b[12];
    lv_textarea_set_text(netTa[0], v.wifiSsid);
    lv_textarea_set_text(netTa[1], v.wifiPass);
    lv_textarea_set_text(netTa[2], v.tcpHost);
    snprintf(b, sizeof(b), "%d", v.tcpPort);
    lv_textarea_set_text(netTa[3], b);
    snprintf(b, sizeof(b), "%d", v.udpPort);
    lv_textarea_set_text(netTa[4], v.udpPort ? b : "");
    netKind = v.tcpKind;
    lv_keyboard_set_textarea(netKb, netTa[0]);
    lv_obj_set_hidden(netScr, false);
}

static void onNetBtn(lv_event_t* e) {
    int a = (int)(intptr_t)lv_event_get_user_data(e);
    if (a == 0) {                                         // tipo de servidor; gpsd lleva su puerto
        netKind = (netKind + 1) % net::TCP_KINDS;
        if (netKind == net::TCP_GPSD) lv_textarea_set_text(netTa[3], "2947");
        else if (netKind == net::TCP_NMEA) lv_textarea_set_text(netTa[3], "10110");
    } else if (a == 1) {                                  // guardar y conectar
        settings::Values& v = settings::get();
        snprintf(v.wifiSsid, sizeof(v.wifiSsid), "%s", lv_textarea_get_text(netTa[0]));
        snprintf(v.wifiPass, sizeof(v.wifiPass), "%s", lv_textarea_get_text(netTa[1]));
        snprintf(v.tcpHost, sizeof(v.tcpHost), "%s", lv_textarea_get_text(netTa[2]));
        v.tcpKind = netKind;
        int port = atoi(lv_textarea_get_text(netTa[3]));
        v.tcpPort = (port >= 1 && port <= 65535) ? port : 10110;
        port = atoi(lv_textarea_get_text(netTa[4]));
        v.udpPort = (port >= 1 && port <= 65535) ? port : 0;
        settings::save();
        links::applyNetwork();
    } else lv_obj_set_hidden(netScr, true);
}

static lv_obj_t* netField(int idx, int x, int y, int w, const char* hint, int maxLen, bool digits) {
    lv_obj_t* ta = lv_textarea_create(netScr);
    lv_obj_remove_style_all(ta);
    lv_obj_set_pos(ta, x, y);
    lv_obj_set_size(ta, w, 48);
    lv_textarea_set_one_line(ta, true);
    lv_textarea_set_max_length(ta, (uint32_t)maxLen);
    lv_textarea_set_placeholder_text(ta, hint);
    if (digits) lv_textarea_set_accepted_chars(ta, "0123456789");
    lv_obj_set_style_bg_color(ta, lv_color_hex(C_BAR), 0);
    lv_obj_set_style_bg_opa(ta, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(ta, 8, 0);
    lv_obj_set_style_border_color(ta, lv_color_hex(C_LINE), 0);
    lv_obj_set_style_border_width(ta, 1, 0);
    lv_obj_set_style_border_color(ta, lv_color_hex(C_ACC), LV_STATE_FOCUSED);
    lv_obj_set_style_border_width(ta, 2, LV_STATE_FOCUSED);
    lv_obj_set_style_pad_hor(ta, 12, 0);
    lv_obj_set_style_pad_ver(ta, 12, 0);
    lv_obj_set_style_text_font(ta, &dejavu_16, 0);
    lv_obj_set_style_text_color(ta, lv_color_hex(C_TEXT), 0);
    lv_obj_set_style_text_color(ta, lv_color_hex(C_DIM), LV_PART_TEXTAREA_PLACEHOLDER);
    lv_obj_set_style_bg_color(ta, lv_color_hex(C_TEXT), LV_PART_CURSOR | LV_STATE_FOCUSED);
    lv_obj_set_style_bg_opa(ta, LV_OPA_COVER, LV_PART_CURSOR | LV_STATE_FOCUSED);
    lv_obj_set_style_width(ta, 2, LV_PART_CURSOR);
    lv_obj_add_event_cb(ta, onNetField, LV_EVENT_FOCUSED, nullptr);
    lv_obj_add_event_cb(ta, onNetField, LV_EVENT_CLICKED, nullptr);
    netTa[idx] = ta;
    return ta;
}

static void makeNet() {
    netScr = lv_obj_create(lv_layer_top());
    lv_obj_remove_style_all(netScr);
    lv_obj_set_size(netScr, board::W, board::H);
    lv_obj_set_style_bg_color(netScr, lv_color_hex(C_BG), 0);
    lv_obj_set_style_bg_opa(netScr, LV_OPA_COVER, 0);
    lv_obj_set_scrollable(netScr, false);
    lv_obj_set_clickable(netScr, true);
    lv_obj_t* t = label(netScr, "Red", &dejavu_bold_20, C_TEXT);
    lv_obj_set_pos(t, 16, 18);
    netStatus = label(netScr, "", &dejavu_14, C_SOFT);
    lv_obj_set_pos(netStatus, 80, 12);
    lv_obj_set_width(netStatus, 540);
    lv_obj_t* b = textButton(netScr, "Guardar y conectar", 220, 48, onNetBtn, 1);
    lv_obj_set_pos(b, 648, 8);
    b = textButton(netScr, "Cerrar", 124, 48, onNetBtn, 2);
    lv_obj_set_pos(b, 884, 8);
    netField(0, 16, 68, 488, "Red WiFi", 32, false);
    netField(1, 520, 68, 488, "Clave de la red", 64, false);
    b = textButton(netScr, "", 200, 48, onNetBtn, 0);
    lv_obj_set_pos(b, 16, 126);
    netKindLbl = lv_obj_get_child(b, 0);
    netField(2, 232, 126, 420, "Servidor (direcci\xC3\xB3n IP o nombre)", 39, false);
    netField(3, 668, 126, 140, "Puerto", 5, true);
    netField(4, 824, 126, 184, "Puerto UDP", 5, true);
    lv_obj_t* h = label(netScr, "TCP: NMEA 0183 de un multiplexor o de Signal K (puerto 10110), o gpsd (2947). UDP: AIS o NMEA que otro equipo env\xC3\xAD" "e a este puerto.", &dejavu_14, C_DIM);
    lv_obj_set_pos(h, 16, 184);
    lv_obj_set_width(h, 992);
    netKb = lv_keyboard_create(netScr);
    lv_obj_set_size(netKb, board::W, 360);
    lv_obj_align(netKb, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_color(netKb, lv_color_hex(C_BG), 0);
    lv_obj_set_style_bg_opa(netKb, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(netKb, 6, 0);
    lv_obj_set_style_pad_gap(netKb, 6, 0);
    lv_obj_set_style_border_width(netKb, 0, 0);
    lv_obj_set_style_bg_color(netKb, lv_color_hex(C_PANEL), LV_PART_ITEMS);
    lv_obj_set_style_bg_opa(netKb, LV_OPA_COVER, LV_PART_ITEMS);
    lv_obj_set_style_bg_color(netKb, lv_color_hex(C_LINE), LV_PART_ITEMS | LV_STATE_CHECKED);
    lv_obj_set_style_bg_color(netKb, lv_color_hex(C_ACC), LV_PART_ITEMS | LV_STATE_PRESSED);
    lv_obj_set_style_text_color(netKb, lv_color_hex(C_TEXT), LV_PART_ITEMS);
    lv_obj_set_style_text_color(netKb, lv_color_hex(C_TEXT), LV_PART_ITEMS | LV_STATE_CHECKED);   // las teclas de control
    lv_obj_set_style_radius(netKb, 8, LV_PART_ITEMS);
    lv_obj_set_style_border_width(netKb, 0, LV_PART_ITEMS);
    lv_obj_set_style_shadow_width(netKb, 0, LV_PART_ITEMS);
    lv_obj_set_hidden(netScr, true);
}

static void netSummary(char* b, size_t n, bool full) {
    const settings::Values& v = settings::get();
    const net::Status& ns = net::status();
    const links::Status& ls = links::status();
    if (!v.wifiSsid[0]) { snprintf(b, n, full ? "WiFi sin configurar" : "sin configurar"); return; }
    if (!ns.wifi) { snprintf(b, n, full ? "WiFi: conectando a %s..." : "conectando a %s", v.wifiSsid); return; }
    if (!full) { snprintf(b, n, "%s \xC2\xB7 %u l\xC3\xADneas", ns.ip, (unsigned)(ls.tcpLines + ls.udpLines)); return; }
    char tcp[64] = "", udp[40] = "";
    if (v.tcpKind) snprintf(tcp, sizeof(tcp), " \xC2\xB7 TCP %s, %u l\xC3\xADneas", ns.tcp ? "conectado" : "sin conexi\xC3\xB3n", (unsigned)ls.tcpLines);
    if (v.udpPort) snprintf(udp, sizeof(udp), " \xC2\xB7 UDP %u l\xC3\xADneas", (unsigned)ls.udpLines);
    snprintf(b, n, "WiFi: %s \xC2\xB7 %s \xC2\xB7 %d dBm\n%s%s", v.wifiSsid, ns.ip, ns.rssi, tcp[0] ? tcp + 4 : "", tcp[0] ? udp : (udp[0] ? udp + 4 : ""));
}

static void updateNet() {
    if (!netScr || lv_obj_is_hidden(netScr)) return;
    char b[220];
    netSummary(b, sizeof(b), true);
    lv_label_set_text(netStatus, b);
    lv_label_set_text(netKindLbl, kTcpKinds[netKind]);
}

// ---------- Ajustes ----------
static void setBaseMap(int i) {
    baseMap = (i >= 0 && i < kBaseMapN) ? i : 0;
    for (Missing& m : missing) m = Missing();             // otra carpeta: lo que faltaba ya no vale
    for (TileImg& t : tileImgs) t.z = -1;                 // y las teselas puestas se vuelven a buscar
}

static void onSetting(lv_event_t* e) {
    int what = (int)(intptr_t)lv_event_get_user_data(e);
    settings::Values& v = settings::get();
    if (what == 0) { v.simulator = !v.simulator; links::setSimulator(v.simulator); }
    else if (what == 1) { v.serialBaud = v.serialBaud == 38400 ? 4800 : 38400; links::setSerialBaud(v.serialBaud); }
    else if (what == 4) { v.units = (v.units + 1) % 3; applyUnits(); }
    else if (what == 6) { v.can = !v.can; links::setCan(v.can); }
    else if (what == 7) { openNet(); return; }
    else if (what == 5) { v.baseMap = (v.baseMap + 1) % kBaseMapN; setBaseMap(v.baseMap); }
    else {
        v.brightness += what == 2 ? -10 : 10;
        if (v.brightness < 10) v.brightness = 10;
        if (v.brightness > 100) v.brightness = 100;
        board::setBacklight(v.brightness);
    }
    settings::save();
}

static lv_obj_t* settingRow(lv_obj_t* page, int row, const char* title, const char* help) {
    lv_obj_t* r = box(page, C_PANEL, 10);
    lv_obj_set_size(r, 620, 72);
    lv_obj_set_pos(r, 0, row * 80);
    lv_obj_t* t = label(r, title, &dejavu_bold_20, C_TEXT);
    lv_obj_align(t, LV_ALIGN_TOP_LEFT, 14, 9);
    lv_obj_t* h = label(r, help, &dejavu_14, C_DIM);
    lv_obj_align(h, LV_ALIGN_BOTTOM_LEFT, 14, -9);
    return r;
}

static void makeSettings(lv_obj_t* page) {
    lv_obj_t* r = settingRow(page, 0, "Simulador", "Datos de prueba mientras no haya nada conectado");
    lv_obj_t* b = textButton(r, "", 170, 52, onSetting, 0);
    lv_obj_align(b, LV_ALIGN_RIGHT_MID, -14, 0);
    setSimLbl = lv_obj_get_child(b, 0);
    r = settingRow(page, 1, "Puerto serie RS485", "38400 para un receptor AIS, 4800 para NMEA cl\xC3\xA1sico");
    b = textButton(r, "", 170, 52, onSetting, 1);
    lv_obj_align(b, LV_ALIGN_RIGHT_MID, -14, 0);
    setBaudLbl = lv_obj_get_child(b, 0);
    r = settingRow(page, 2, "Brillo", "Luz de fondo de la pantalla");
    b = textButton(r, "+", 56, 52, onSetting, 3);
    lv_obj_align(b, LV_ALIGN_RIGHT_MID, -14, 0);
    setBrightLbl = label(r, "", &dejavu_bold_20, C_TEXT);
    lv_obj_align(setBrightLbl, LV_ALIGN_RIGHT_MID, -84, 0);
    b = textButton(r, "-", 56, 52, onSetting, 2);
    lv_obj_align(b, LV_ALIGN_RIGHT_MID, -150, 0);
    r = settingRow(page, 3, "Unidades", "mn, kn, m \xC2\xB7 km, km/h, m/s \xC2\xB7 mi, mph, ft, \xC2\xB0" "F");
    b = textButton(r, "", 170, 52, onSetting, 4);
    lv_obj_align(b, LV_ALIGN_RIGHT_MID, -14, 0);
    setUnitsLbl = lv_obj_get_child(b, 0);
    r = settingRow(page, 4, "Mapa base", "Teselas de la tarjeta: /charts/osm, ihm, ign o pnoa");
    b = textButton(r, "", 170, 52, onSetting, 5);
    lv_obj_align(b, LV_ALIGN_RIGHT_MID, -14, 0);
    setMapLbl = lv_obj_get_child(b, 0);
    r = settingRow(page, 5, "NMEA 2000", "CAN de la placa, 250 kbit/s, solo escucha");
    b = textButton(r, "", 170, 52, onSetting, 6);
    lv_obj_align(b, LV_ALIGN_RIGHT_MID, -14, 0);
    setCanLbl = lv_obj_get_child(b, 0);
    lv_obj_t* info = box(page, C_PANEL, 10);
    lv_obj_set_size(info, 374, 404);
    b = textButton(page, "Red: WiFi, TCP y UDP", 374, 60, onSetting, 7);
    lv_obj_set_pos(b, 630, 412);
    lv_obj_set_pos(info, 630, 0);
    lv_obj_t* t = label(info, "Estado", &dejavu_bold_20, C_TEXT);
    lv_obj_align(t, LV_ALIGN_TOP_LEFT, 14, 12);
    setInfo = label(info, "", &dejavu_16, C_SOFT);
    lv_obj_align(setInfo, LV_ALIGN_TOP_LEFT, 14, 48);
    lv_obj_set_width(setInfo, 346);
    lv_obj_set_style_text_line_space(setInfo, 6, 0);
    lv_obj_t* about = label(info, "APRS LoRa+ \xC2\xB7 \xC2\xA9 2026 Manuel - EA7JRS\nGPL-3.0 \xC2\xB7 mapas \xC2\xA9 OpenStreetMap", &dejavu_14, C_DIM);
    lv_obj_align(about, LV_ALIGN_BOTTOM_LEFT, 14, -12);
}

static void updateSettings() {
    const settings::Values& v = settings::get();
    lv_label_set_text(setSimLbl, v.simulator ? "Encendido" : "Apagado");
    lv_label_set_text(setBaudLbl, v.serialBaud == 38400 ? "38400 (AIS)" : "4800 (NMEA)");
    lv_label_set_text(setUnitsLbl, v.units == 1 ? "M\xC3\xA9trico" : (v.units == 2 ? "Imperial" : "N\xC3\xA1utico"));
    lv_label_set_text(setMapLbl, kBaseMaps[baseMap].name);
    lv_label_set_text(setCanLbl, v.can ? "Encendido" : "Apagado");
    char b[520], netTxt[80];
    netSummary(netTxt, sizeof(netTxt), false);
    snprintf(b, sizeof(b), "%d %%", v.brightness);
    lv_label_set_text(setBrightLbl, b);
    const links::Status& ls = links::status();
    const NmeaStats& ns = links::serialStats();
    uint32_t now = millis();
    snprintf(b, sizeof(b),
             "Versi\xC3\xB3n %s\nPuerto serie: %u l\xC3\xADneas, %u con error\n\xC3\x9Altima l\xC3\xADnea: %s\nNMEA 2000: %u tramas, %u mensajes\n\xC3\x9Altima trama: %s\nRed: %s\nMensajes AIS: %u\nBlancos AIS: %d\nWaypoints: %d de %d\nDerrota: %d puntos",
#ifdef PLOTTER_VERSION
             PLOTTER_VERSION,
#else
             "(PC)",
#endif
             (unsigned)ls.serialLines, (unsigned)(ns.badChecksum + ns.malformed),
             ls.serialLines ? (now - ls.serialLastMs < 5000 ? "ahora" : "hace rato") : "ninguna", (unsigned)ls.canFrames, (unsigned)ls.canMessages,
             !ls.can ? "apagado" : (ls.canFrames ? (now - ls.canLastMs < 5000 ? "ahora" : "hace rato") : "ninguna"), netTxt, (unsigned)ls.aisMessages, links::targets().count(), wpN, kWps,
             trackN);
    lv_label_set_text(setInfo, b);
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

// MOB: marca donde esta el barco, pone rumbo de vuelta y abre la ventana. Si ya hay uno marcado, solo la reabre.
static void onMob(lv_event_t*) {
    uint32_t now = millis();
    double la, lo;
    if (!navMob) {
        if (!store->get(Key::Lat, now, la) || !store->get(Key::Lon, now, lo)) return;      // sin posicion no hay que marcar
        mobMs = now;
        startGoTo({la, lo}, "MOB", true);
        selOn = false;
        chFollow = true;
    }
    openModal(M_MOB, 0);
}

// Marcar: waypoint en la posicion del barco
static void onMark(lv_event_t*) {
    uint32_t now = millis();
    double la, lo;
    if (store->get(Key::Lat, now, la) && store->get(Key::Lon, now, lo)) addWaypoint({la, lo});
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
    lv_obj_add_event_cb(mob, onMob, LV_EVENT_CLICKED, nullptr);
    lv_obj_t* mark = lv_button_create(top);
    lv_obj_remove_style_all(mark);
    lv_obj_set_size(mark, 84, TOP_H - 1);
    lv_obj_align(mark, LV_ALIGN_RIGHT_MID, -70, 0);
    lv_obj_set_style_bg_color(mark, lv_color_hex(C_PANEL), 0);
    lv_obj_set_style_bg_opa(mark, LV_OPA_COVER, 0);
    lv_obj_t* markL = label(mark, "Marcar", &dejavu_bold_16, C_TEXT);
    lv_obj_center(markL);
    lv_obj_add_event_cb(mark, onMark, LV_EVENT_CLICKED, nullptr);
    lblPos = label(top, "Sin posici\xC3\xB3n", &dejavu_16, C_SOFT);
    lv_obj_align(lblPos, LV_ALIGN_RIGHT_MID, -168, 0);

    // Cuerpo: una pagina por pestana
    for (int i = 0; i < 8; i++) {
        pages[i] = lv_obj_create(scr);
        lv_obj_remove_style_all(pages[i]);
        lv_obj_set_size(pages[i], board::W, board::H - TOP_H - NAV_H);
        lv_obj_align(pages[i], LV_ALIGN_TOP_LEFT, 0, TOP_H);
        lv_obj_set_style_pad_all(pages[i], PAD, 0);
        lv_obj_set_scrollable(pages[i], false);
        if (i == 1) makeChart(pages[i]);
        else if (i == 2) makeRoutes(pages[i]);
        else if (i == 3) makeRadar(pages[i]);
        else if (i == 7) makeSettings(pages[i]);
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
    makeNet();                                            // antes que las alertas: estas quedan por encima
    makeModal();
    loadWaypoints();
    applyUnits();
    setBaseMap(settings::get().baseMap);
    {                                                     // fondeo que estaba vigilado antes de apagar o reiniciar
        const settings::Values& v = settings::get();
        anchorRode = v.anchorRode;
        if (v.anchorOn) anchorW.drop({v.anchorLat, v.anchorLon}, v.anchorRadius, millis());
    }
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
    if (!degrees) {                                       // la unidad de la casilla, por si se ha cambiado en Ajustes
        const char* u = k == Key::Depth ? plotter::label(U.depth) : (k == Key::WaterTemp ? plotter::label(U.temp) : plotter::label(k == Key::Aws ? U.windSpeed : U.boatSpeed));
        lv_label_set_text(tileUnit[idx], u);
    }
    if (!store->get(k, now, v)) {
        lv_label_set_text(tiles[idx].value, "--");
        lv_label_set_text(tiles[idx].sub, "Sin datos");
        return;
    }
    char b[24];
    if (degrees) snprintf(b, sizeof(b), "%03d\xC2\xB0", (int)lround(norm360(v)) % 360);
    else {
        if (k == Key::Depth) v = depthFromM(v, U.depth);
        else if (k == Key::WaterTemp) v = tempFromC(v, U.temp);
        else if (k == Key::Aws) v = speedFromKn(v, U.windSpeed);
        else v = speedFromKn(v, U.boatSpeed);
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
    updateNav();
    if (active == 1) updateChart();
    if (active == 2) updateRoutes();
    if (active == 7) updateSettings();
    updateNet();
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
        snprintf(b, sizeof(b), "%.1f", speedFromKn(tws, U.windSpeed));
        for (char* p = b; *p; p++) if (*p == '.') *p = ',';
        lv_label_set_text(tiles[T_TWS].value, b);
        lv_label_set_text(tileUnit[T_TWS], plotter::label(U.windSpeed));
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

// Para las pruebas en el PC: lo mismo que tocar la pantalla
void testTapChart(int x, int y) {
    double cx, cy;
    lonLatToWorldPx(chCenter, chZoom, cx, cy);
    selectPoint(worldPxToLonLat(cx + x - CH_W / 2.0, cy + y - CH_H / 2.0, chZoom));
}
void testPanelButton(int b) {
    lv_obj_send_event(chPanelBtn[b], LV_EVENT_CLICKED, nullptr);
}
void testMob() { onMob(nullptr); }
void testRoutes(int action) {                              // 2 ir a, 3 ver, 4 a la ruta, 5 borrar, 6 navegar, 7 quitar, 8 vaciar
    switch (action) {
        case 4: if (wpSel >= 0 && routeN < kRoute) { routePts[routeN] = wps[wpSel].p; snprintf(routeNames[routeN], sizeof(routeNames[routeN]), "%s", wps[wpSel].name); routeN++; } break;
        case 6: if (navOn && navCount > 1) stopGoTo(); else if (routeN >= 1) { startRoute(); showTab(1); } break;
        case 8: routeN = 0; break;
        default: break;
    }
}
void testSelectWaypoint(int i) { wpSel = (i >= 0 && i < wpN) ? i : -1; }
void testMark() { onMark(nullptr); }
void testAnchor() { onAnchorBtn(nullptr); }
void testNet() { openNet(); }
void testSetting(int what) {                               // 4 unidades, 5 mapa base
    settings::Values& v = settings::get();
    if (what == 4) { v.units = (v.units + 1) % 3; applyUnits(); }
    else if (what == 5) { v.baseMap = (v.baseMap + 1) % kBaseMapN; setBaseMap(v.baseMap); }
}

}
