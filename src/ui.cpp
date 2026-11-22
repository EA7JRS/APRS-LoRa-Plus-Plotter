/*
 * Copyright (C) 2026 Manuel - EA7JRS
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "ui.h"

#include <Arduino.h>
#include <lvgl.h>

#include <cctype>
#include <cmath>
#include <ctime>
#include <algorithm>
#include <vector>

#include "board.h"
#include "anchor.h"
#include "signalk.h"
#include "units.h"
#include "about_text.h"
#include "ui_assets.h"
#include "lights.h"
#include "aistargets.h"
#include "chartui.h"
#include "gpx.h"
#include "links.h"
#include "motion.h"
#include "route.h"
#include "vessel_link.h"
#include "licence.h"
#include "maritime.h"
#include "mtcheck.h"
#include "settings.h"
#include "net.h"
#include "tsync.h"
#include "ble.h"
#include "alerts_net.h"
#include "tiles.h"
#include "wind.h"
#include "astro.h"
#include "celnav.h"
#include "current.h"
#include "bearingfix.h"
#include "radiotel.h"
#include "vessel.h"
#include "wmm.h"
#include "wxcalc.h"

LV_FONT_DECLARE(dejavu_12)
LV_FONT_DECLARE(dejavu_14)
LV_FONT_DECLARE(dejavu_16)
LV_FONT_DECLARE(dejavu_bold_16)
LV_FONT_DECLARE(dejavu_bold_20)
LV_FONT_DECLARE(dejavu_bold_28)
LV_FONT_DECLARE(dejavu_num_60)
LV_FONT_DECLARE(dejavu_num_44)

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
enum { T_SOG, T_COG, T_HDG, T_DEPTH, T_AWS, T_AWA, T_TWS, T_WATER,
       T_RPM, T_COOL, T_OILP, T_FUEL, T_BATT, T_RATE, T_OILT, T_HOURS, T_COUNT };     // la segunda fila: pagina Motor
static lv_obj_t* tileUnit[T_COUNT] = {};
static lv_obj_t* gridNav = nullptr;
static lv_obj_t* gridEngine = nullptr;
static bool instEngine = false;                          // Instrumentos: navegacion (false) o motor (true)

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
enum ModalMode { M_NONE, M_DETAIL, M_COLLISION, M_MOB, M_ANCHOR, M_RS485, M_SIGNALK, M_OFFICIAL, M_NAVTEX, M_DSC };
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
// Luces de la lista de la tarjeta (S:/luces.txt: una por linea, nombre;lat;lon;caracteristica;altura;alcance; por ejemplo la
// que saca tools/faros/libro_faros.py del Libro de Faros del IHM, para uso personal)
static std::vector<LightRecord> lightList;
static const char* kLightFile = "S:/luces.txt";
static const int kLightMarks = 40;
static lv_obj_t* lightDot[kLightMarks] = {};
static lv_obj_t* lightLbl[kLightMarks] = {};
static int lightShown[kLightMarks];
static int lightShownN = 0;
static int selLight = -1;                                 // luz elegida al tocar la carta (-1 = ninguna)
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
// Rectas de altura (navegacion astronomica): hasta 4 en la carta, y la situacion si hay dos o mas
static const int kLops = 4;
struct AstroLop { plotter::Sight s; char name[20]; char hhmm[8]; LatLon at; };   // at: posicion del barco al tomarla
static AstroLop lops[kLops];
static int lopN = 0;
static bool astroFixOn = false;
static LatLon astroFix = {0, 0};
static lv_obj_t* lopLine[kLops] = {};
static lv_point_precise_t lopPts[kLops][2];
static lv_obj_t* lopLbl[kLops] = {};
static lv_obj_t* astroFixMark = nullptr;
static bool brgOn = false;                               // situacion por demoras: lineas y marca en la carta
static LatLon brgFix{0, 0}, brgFirst{0, 0}, brgMark2{0, 0};   // brgFirst: punto 1 trasladado por el camino
static lv_obj_t* brgLine[2] = {};
static lv_point_precise_t brgPts[2][2];
static lv_obj_t* brgFixMark = nullptr;
static lv_obj_t* astroScr = nullptr;
static lv_obj_t* astroTa[5] = {};                       // grados, minutos, error de indice, elevacion del ojo, hora UTC
static lv_obj_t* astroKb = nullptr;
static lv_obj_t* astroBodyLbl = nullptr;
static lv_obj_t* astroStarLbl = nullptr;
static lv_obj_t* astroOut = nullptr;
static lv_obj_t* astroListScr = nullptr;                 // Astro > Visibles
static lv_obj_t* astroListBox = nullptr;
static lv_obj_t* astroListNote = nullptr;
static int astroBody = 0;                               // 0 / 1 Sol limbo inferior / superior, 2 / 3 Luna, 4 a 7 Venus, Marte, Jupiter y Saturno, 8 estrella
static int astroStar = 0;
static void openAstro();
static void onAstroOpen(lv_event_t*) { openAstro(); }
static void openCurrent();                               // Carta > Corriente
static void onCurrentOpen(lv_event_t*) { openCurrent(); }
static void openBearings();                              // Carta > Demoras
static void onBearingsOpen(lv_event_t*) { openBearings(); }
static void openCt();                                    // Astro > Desvio (Ct): correccion total y tablilla de desvios
static lv_obj_t* ctScr = nullptr;
static lv_obj_t* ctTa[3] = {};                           // demora de aguja, rumbo de aguja, hora UTC
static lv_obj_t* ctKb = nullptr;
static lv_obj_t* ctBodyLbl = nullptr;
static lv_obj_t* ctOut = nullptr;
static lv_obj_t* ctCardScr = nullptr;                    // la tablilla
static lv_obj_t* ctCardBox = nullptr;
static lv_obj_t* ctCardNote = nullptr;
static lv_obj_t* ctApplyBtn = nullptr;
static bool ctHave = false;                              // ultimo calculo valido: rumbo de aguja y desvio
static double ctLastRa = 0, ctLastDev = 0;
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
static lv_obj_t* setBleLbl = nullptr;
// Pantalla de red (WiFi, TCP, UDP) con teclado
static lv_obj_t* netScr = nullptr;
static lv_obj_t* aboutScr = nullptr;
static lv_obj_t* netTa[9] = {};                        // red, clave, servidor, puerto TCP, puerto UDP, clave de OpenWeatherMap
static lv_obj_t* netKb = nullptr;
static lv_obj_t* netKindLbl = nullptr;
static lv_obj_t* vesScr = nullptr;                      // Ajustes > Embarcacion
static lv_obj_t* vesTa[9] = {};                         // nombre, MMSI, CALLSIGN, eslora, manga, calado, DMR ID / CCS7, distintivo maritimo, clave MarineTraffic
static lv_obj_t* vesMar = nullptr;                      // distintivo maritimo comprobado
static lv_obj_t* vesMarsLbl = nullptr;
static lv_obj_t* vesMtLbl = nullptr;
static lv_obj_t* vesLic = nullptr;                      // licencia (RadioID)
static lv_obj_t* vesKb = nullptr;
static lv_obj_t* vesTypeLbl = nullptr;
static lv_obj_t* vesStatus = nullptr;
static int vesType = 0;
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
    if (i == 0 && active == 0 && gridEngine) instEngine = !instEngine;   // tocar otra vez la pestana: navegacion <-> motor
    active = i;
    if (gridEngine) { lv_obj_set_hidden(gridNav, instEngine); lv_obj_set_hidden(gridEngine, !instEngine); }
    if (lblTitle) {
        lv_label_set_text(lblTitle, i == 0 && instEngine ? "Instrumentos \xC2\xB7 Motor" : kTabs[i]);
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
static void updateRs485Modal();
static void updateSkModal();
static uint32_t skSeen = 0;                               // ultimo aviso de Signal K ya atendido
static bool rs485Warned = false;                          // el aviso de choque en el RS485 ya se ha mostrado

static void closeModal() {
    modalMode = M_NONE;
    modalMmsi = 0;
    if (modalBack) lv_obj_set_hidden(modalBack, true);
}

static void showTab(int i);
static const int kMeteoTab = 6;
static void closeOfficial();
static void closeNavtexModal();
static void openNavtexList();
static void updateNavtexModal();
static void closeDscModal(bool goTo);
static void updateDscModal();
static void startGoTo(LatLon dest, const char* name, bool mob);
static void openRadio(int kind, int nature);            // Radio: mensajes de socorro, urgencia y seguridad
static void onModalA(lv_event_t*) {                       // silenciar la alarma de ese buque (o volver a activarla)
    if (modalMode == M_OFFICIAL) { closeOfficial(); showTab(kMeteoTab); return; }   // LIST: la pestana Meteo
    if (modalMode == M_NAVTEX) { closeNavtexModal(); openNavtexList(); return; }    // LIST: los mensajes NAVTEX
    if (modalMode == M_DSC) { closeDscModal(true); return; }                         // IR A la posicion (o LISTA)
    if (modalMode == M_MOB) { closeModal(); openRadio(0, 10); return; }   // RADIO: el MAYDAY de hombre al agua para leer
    if (modalMode == M_SIGNALK) { closeModal(); return; }  // el aviso sigue en el servidor; aqui solo se cierra
    if (modalMode == M_RS485) {                            // apagar la salida al piloto
        settings::get().pilotOut = 0;
        settings::save();
        closeModal();
        return;
    }
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
    if (modalMode == M_OFFICIAL) { closeOfficial(); return; }
    if (modalMode == M_NAVTEX) { closeNavtexModal(); return; }
    if (modalMode == M_DSC) { closeDscModal(false); return; }
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
    lv_label_set_text(modalBtnALbl, "RADIO (MAYDAY)");
    lv_label_set_text(modalBtnBLbl, "CLOSE");
}

// ---- Avisos oficiales (meteorologicos, tsunamis, terremotos, tiempo espacial) por la WiFi (alerts_net) ----
static plotter::AlertList oaList;
static plotter::SpaceWx oaSpace;
static std::vector<std::string> oaUnseen;               // nuevos o que han subido de nivel, para la ventana
static std::vector<std::string> oaShown;                // "id#nivel" ya vistos (cerrados)
static std::string oaModalId;

static void pollOfficial() {
    std::vector<plotter::Alert> v;
    plotter::SpaceWx sw;
    if (!alertsnet::take(v, sw)) return;
    if (sw.valid()) oaSpace = sw;
    for (const auto& a : v) {
        std::string key = a.id + "#" + std::to_string((int)a.level);
        bool seen = false;
        for (const auto& k : oaShown) if (k == key) seen = true;
        if (seen || a.level < 1) continue;
        // MeteoAlarm sin zonas: el pais entero, solo los rojos
        if (a.src == plotter::AlertSrc::MeteoAlarm && !settings::get().alertZones[0] && a.level < 3) continue;
        bool queued = false;
        for (const auto& u : oaUnseen) if (u == a.id) queued = true;
        if (!queued) oaUnseen.push_back(a.id);
    }
    oaList.clear();
    for (const auto& a : v) oaList.upsert(a);
    if (oaShown.size() > 80) oaShown.erase(oaShown.begin(), oaShown.begin() + 20);
}

static const plotter::Alert* oaNext() {
    const plotter::Alert* best = nullptr;
    for (const auto& id : oaUnseen) { const plotter::Alert* a = oaList.find(id); if (a && (!best || a->level > best->level)) best = a; }
    return best;
}

static uint32_t oaColor(uint8_t lv) { return lv >= 3 ? C_RED : lv == 2 ? 0xFF9A3C : lv == 1 ? 0xFFD84D : C_DIM; }

static void oaWhen(char* o, size_t n, int64_t t) {
    if (t <= 0) { o[0] = 0; return; }
    time_t tt = (time_t)t;
    struct tm tmv;
    gmtime_r(&tt, &tmv);
    static const char* const D[] = {"dom", "lun", "mar", "mi\xC3\xA9", "jue", "vie", "s\xC3\xA1" "b"};
    snprintf(o, n, "%s %02d:%02d UTC", D[tmv.tm_wday], tmv.tm_hour, tmv.tm_min);
}

static void updateOfficialModal() {
    const plotter::Alert* a = oaList.find(oaModalId);
    if (!a) { closeModal(); return; }
    const bool quake = plotter::alertIsQuake(a->src);
    const char* title = quake ? "EARTHQUAKE" : a->src == plotter::AlertSrc::Tsunami ? (a->level >= 3 ? "TSUNAMI WARNING" : "TSUNAMI")
                      : a->src == plotter::AlertSrc::Swpc ? "SPACE WEATHER"
                      : a->level >= 3 ? "RED WARNING" : a->level == 2 ? "ORANGE WARNING" : "YELLOW WARNING";
    lv_obj_set_style_bg_color(modalHead, lv_color_hex(oaColor(a->level)), 0);
    lv_label_set_text(modalTitle, title);
    char sub[96], b[420], f[32], t[32];
    snprintf(sub, sizeof(sub), "%s \xC2\xB7 %s", plotter::alertSrcName(a->src), plotter::alertLevelName(a->level));
    lv_label_set_text(modalSub, sub);
    lv_label_set_text(modalName, a->event.c_str());
    oaWhen(f, sizeof f, a->onset);
    oaWhen(t, sizeof t, a->expires);
    snprintf(b, sizeof(b), "%s\n%s%s%s%s%s", a->area.c_str(), a->text.c_str(), f[0] || t[0] ? "\n" : "", f, t[0] ? " \xE2\x80\x93 " : "", t);
    lv_label_set_text(modalBody, b);
    lv_label_set_text(modalBtnALbl, "LIST");
    lv_label_set_text(modalBtnBLbl, "CLOSE");
}

static void closeOfficial() {
    const plotter::Alert* a = oaList.find(oaModalId);
    if (a) oaShown.push_back(a->id + "#" + std::to_string((int)a->level));
    for (size_t i = 0; i < oaUnseen.size(); i++) if (oaUnseen[i] == oaModalId) { oaUnseen.erase(oaUnseen.begin() + (long)i); break; }
    oaModalId.clear();
    closeModal();
}

// Aviso del servidor Signal K (notifications.*) en estado de alarma o emergencia
static bool skAlarmState(const char* s) { return !strcmp(s, "alarm") || !strcmp(s, "emergency"); }

static void updateSkModal() {
    const links::Status& ls = links::status();
    if (!skAlarmState(ls.skState)) { closeModal(); return; }       // el servidor lo ha dado por resuelto
    const char* title = skNotificationTitleEs(ls.skKey);
    char b[220];
    lv_obj_set_style_bg_color(modalHead, lv_color_hex(!strcmp(ls.skState, "emergency") ? C_RED : C_WARN), 0);
    lv_label_set_text(modalTitle, !strcmp(ls.skState, "emergency") ? "SIGNAL K EMERGENCY" : "SIGNAL K ALARM");
    lv_label_set_text(modalSub, "Signal K \xC2\xB7 notification from the server");
    lv_label_set_text(modalName, title ? title : ls.skKey);
    snprintf(b, sizeof(b), "%s\n%s", ls.skMessage[0] ? ls.skMessage : "(sin mensaje)", ls.skKey);
    lv_label_set_text(modalBody, b);
    lv_label_set_text(modalBtnALbl, "SILENCE ALARM");
    lv_label_set_text(modalBtnBLbl, "CLOSE");
}

// La salida al piloto y la entrada de datos comparten el RS485: avisar de que pueden pisarse
static bool rs485Busy() {
    const links::Status& ls = links::status();
    return ls.serialLines > 0 && millis() - ls.serialLastMs < 60000;
}

static void updateRs485Modal() {
    if (settings::get().pilotOut != 1) { closeModal(); return; }
    const links::Status& ls = links::status();
    char b[320];
    lv_obj_set_style_bg_color(modalHead, lv_color_hex(C_WARN), 0);
    lv_label_set_text(modalTitle, "RS485 COMPARTIDO");
    lv_label_set_text(modalSub, "Salida al piloto y entrada de datos por el mismo puerto");
    lv_label_set_text(modalName, "Los datos pueden chocar");
    snprintf(b, sizeof(b), "Por el RS485 est\xC3\xA1n entrando datos (%u l\xC3\xADneas, AIS o NMEA). Si el plotter emite por el mismo par, las tramas pueden pisarse y "
             "perderse blancos AIS o datos del piloto.\nAlternativa: enviar al piloto por la red.", (unsigned)ls.serialLines);
    lv_label_set_text(modalBody, b);
    lv_label_set_text(modalBtnALbl, "Apagar salida");
    lv_label_set_text(modalBtnBLbl, "Seguir as\xC3\xAD");
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
    if (modalMode == M_RS485) { updateRs485Modal(); return; }
    if (modalMode == M_SIGNALK) { updateSkModal(); return; }
    if (modalMode == M_OFFICIAL) { updateOfficialModal(); return; }
    if (modalMode == M_NAVTEX) { updateNavtexModal(); return; }
    if (modalMode == M_DSC) { updateDscModal(); return; }
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
    {                                                     // aviso nuevo de Signal K en alarma o emergencia
        const links::Status& ls = links::status();
        if (ls.skSeq != skSeen) {
            skSeen = ls.skSeq;
            if (skAlarmState(ls.skState) && (modalMode == M_NONE || modalMode == M_DETAIL || modalMode == M_RS485)) { openModal(M_SIGNALK, 0); return; }
        }
    }
    // Aviso (una vez) si la salida al piloto va por el RS485 y por ese puerto estan entrando datos
    if (settings::get().pilotOut != 1) rs485Warned = false;
    else if (!rs485Warned && rs485Busy() && modalMode == M_NONE) { rs485Warned = true; openModal(M_RS485, 0); return; }
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


// ---- NAVTEX: mensajes por NRX (receptor de a bordo o la app); los de asunto D (salvamento) abren la ventana ----
static uint32_t nvSeq = 0;
static std::vector<std::string> nvShown;                 // "B1B2B3B4@kHz" ya vistos en la ventana
static std::string nvModalKey;
static lv_obj_t* nvScr = nullptr;
static lv_obj_t* nvBox = nullptr;
static lv_obj_t* nvNote = nullptr;

static std::string nvKey(const plotter::NavtexMessage& m) { return m.id() + "@" + std::to_string((int)m.khz); }
static const plotter::NavtexMessage* nvFind(const std::string& k) {
    for (const plotter::NavtexMessage& m : links::navtex().items()) if (nvKey(m) == k) return &m;
    return nullptr;
}
static void nvFreq(char* o, size_t n, double khz) {
    if (khz <= 0) snprintf(o, n, "?");
    else if (khz > 4000) snprintf(o, n, "4209,5 kHz");
    else snprintf(o, n, "%d kHz", (int)khz);
}

static uint32_t nvModalMs = 0;
static void updateNavtexModal() {
    const plotter::NavtexMessage* m = nvFind(nvModalKey);
    if (!m) { closeModal(); return; }
    const links::NavtexAlarm al = links::navtexAlarm();   // reconocida en el receptor o en otro equipo: aqui tambien
    if (al.seen && !al.active && al.ageMs < 5000u && millis() - nvModalMs > 35000u) {
        nvShown.push_back(nvModalKey);
        nvModalKey.clear();
        closeModal();
        return;
    }
    char sub[96], f[16];
    nvFreq(f, sizeof f, m->khz);
    lv_obj_set_style_bg_color(modalHead, lv_color_hex(C_RED), 0);
    lv_label_set_text(modalTitle, "NAVTEX SAR / DISTRESS");
    snprintf(sub, sizeof sub, "NAVTEX \xC2\xB7 %s \xC2\xB7 %s", f, plotter::navtexSubjectEs(m->subject));
    lv_label_set_text(modalSub, sub);
    lv_label_set_text(modalName, m->id().c_str());
    std::string t = m->text.size() > 380 ? m->text.substr(0, 380) + "..." : m->text;
    lv_label_set_text(modalBody, t.c_str());
    lv_label_set_text(modalBtnALbl, "LIST");
    lv_label_set_text(modalBtnBLbl, "CLOSE");
}

static void closeNavtexModal() {
    links::navtexAck();                                  // reconoce la alarma tambien en el receptor ($IIACK,003), si hay por donde
    nvShown.push_back(nvModalKey);
    if (nvShown.size() > 40) nvShown.erase(nvShown.begin());
    nvModalKey.clear();
    closeModal();
}

// La lista se guarda en la tarjeta (S:/navtex.txt) a los 5 s del ultimo cambio y se lee al arrancar: asi los conservados y
// lo ya visto en las ultimas 72 h siguen tras apagar. Sin tarjeta se queda en memoria.
static const char* kNavtexFile = "S:/navtex.txt";
static uint32_t nvSavedSeq = 0, nvDirtyMs = 0, nvDirtySeq = 0;
static void loadNavtex() {
    lv_fs_file_t f;
    if (lv_fs_open(&f, kNavtexFile, LV_FS_MODE_RD) != LV_FS_RES_OK) return;
    std::string all;
    char buf[512];
    uint32_t br = 0;
    while (all.size() < 200000 && lv_fs_read(&f, buf, sizeof(buf), &br) == LV_FS_RES_OK && br > 0) all.append(buf, br);
    lv_fs_close(&f);
    plotter::NavtexLog& log = links::navtex();
    log.load(all, tsync::now());
    for (const plotter::NavtexMessage& m : log.items())     // lo que ya estaba guardado no vuelve a abrir la ventana
        if (m.urgent() && nvShown.size() < 40) nvShown.push_back(m.id() + "@" + std::to_string((int)m.khz));
    nvSavedSeq = nvDirtySeq = nvSeq = log.seq();
}
static void saveNavtexIfDue() {
    const plotter::NavtexLog& log = links::navtex();
    if (log.seq() == nvSavedSeq) return;
    if (log.seq() != nvDirtySeq) { nvDirtySeq = log.seq(); nvDirtyMs = millis(); return; }
    if (millis() - nvDirtyMs < 5000u) return;
    nvSavedSeq = log.seq();
    lv_fs_file_t f;
    if (lv_fs_open(&f, kNavtexFile, LV_FS_MODE_WR) != LV_FS_RES_OK) return;      // sin tarjeta: se queda en memoria
    const std::string s = log.save();
    uint32_t bw = 0;
    lv_fs_write(&f, s.data(), (uint32_t)s.size(), &bw);
    lv_fs_close(&f);
}

static void refreshNavtexList();
static void nfPaint();
static bool navtexFiltersOpen();
static void checkNavtex() {
    saveNavtexIfDue();
    static uint32_t nfLast = 0;
    if (navtexFiltersOpen() && millis() - nfLast > 1000u) { nfLast = millis(); nfPaint(); }
    const plotter::NavtexLog& log = links::navtex();
    if (log.seq() == nvSeq) return;
    nvSeq = log.seq();
    if (nvScr && !lv_obj_has_flag(nvScr, LV_OBJ_FLAG_HIDDEN)) refreshNavtexList();
    if (modalMode != M_NONE) { nvSeq--; return; }        // otra ventana abierta: se vuelve a mirar luego
    for (const plotter::NavtexMessage& m : log.items()) {
        if (!m.urgent()) continue;
        const std::string k = nvKey(m);
        bool seen = false;
        for (const std::string& x : nvShown) if (x == k) seen = true;
        if (seen) continue;
        nvModalKey = k;
        nvModalMs = millis();
        openModal(M_NAVTEX, 0);
        return;
    }
}

static void onNavtexKeep(lv_event_t* e) {               // conservar / soltar (borrar no se puede: MSC.508(105), 6.1.1)
    const int i = (int)(intptr_t)lv_event_get_user_data(e);
    plotter::NavtexLog& log = links::navtex();
    if (i >= 0 && (size_t)i < log.items().size()) log.keep((size_t)i, !log.items()[(size_t)i].kept);
    nvSeq = log.seq();
    refreshNavtexList();
}

// ---- Filtros del receptor (NRM): estaciones y asuntos que guarda, por frecuencia; se leen y se cambian por el enlace TCP ----
static lv_obj_t* nfScr = nullptr;
static lv_obj_t* nfNote = nullptr;
static lv_obj_t* nfFreqBtn[3] = {};
static lv_obj_t* nfSt[26] = {};
static lv_obj_t* nfSu[26] = {};
static int nfK = 1;                                      // 0 = 490, 1 = 518, 2 = 4209,5 kHz
static plotter::NavtexMask nfEdit[3];
static bool nfTouched[3] = {false, false, false};
static uint32_t nfAskMs = 0;

static void nfPaint() {
    if (!nfScr) return;
    const links::NavtexFilters f = links::navtexFilters();
    for (int k = 0; k < 3; k++) {
        if (!nfTouched[k] && f.have[k]) nfEdit[k] = f.m[k];
        lv_obj_set_style_bg_color(nfFreqBtn[k], lv_color_hex(k == nfK ? C_ACC : C_BAR), 0);
    }
    const plotter::NavtexMask& m = nfEdit[nfK];
    for (int i = 0; i < 26; i++) {
        const bool st = (m.stations >> i) & 1u, fixed = (plotter::kNavtexFixedSubjects >> i) & 1u, su = fixed || ((m.subjects >> i) & 1u);
        lv_obj_set_style_bg_color(nfSt[i], lv_color_hex(st ? 0x1F7A4D : C_BAR), 0);
        lv_obj_set_style_bg_color(nfSu[i], lv_color_hex(fixed ? 0x5B6B7A : su ? 0x1F7A4D : C_BAR), 0);
    }
    char n[300];
    if (!f.link) snprintf(n, sizeof n, "Sin enlace con el receptor: hace falta el programa Navtex (u otro receptor que entienda NRM) como servidor \xC2\xABTCP: NMEA\xC2\xBB en Ajustes > Red.");
    else if (!f.have[nfK]) snprintf(n, sizeof n, "%s", millis() - nfAskMs < 15000u ? "Preguntando al receptor..." : "El receptor no ha contestado sus filtros de esta frecuencia. Toca \xC2\xABLeer del receptor\xC2\xBB.");
    else snprintf(n, sizeof n, "%s Verde: se guarda. Los asuntos A, B, D y L (avisos a la navegaci\xC3\xB3n, meteorol\xC3\xB3gicos y salvamento) no se pueden quitar.",
                  nfTouched[nfK] ? "Cambios sin enviar." : "Filtros del receptor.");
    lv_label_set_text(nfNote, n);
}

static void onNfLetter(lv_event_t* e) {                  // 0-25 estacion, 100-125 asunto
    const int v = (int)(intptr_t)lv_event_get_user_data(e);
    plotter::NavtexMask& m = nfEdit[nfK];
    if (v >= 100) { const int i = v - 100; if ((plotter::kNavtexFixedSubjects >> i) & 1u) return; m.subjects ^= 1u << i; }
    else m.stations ^= 1u << v;
    nfTouched[nfK] = true;
    nfPaint();
}

static void onNfButton(lv_event_t* e) {                  // 0-2 frecuencia, 10 leer, 11 enviar, 12 todas, -1 cerrar
    const int v = (int)(intptr_t)lv_event_get_user_data(e);
    if (v < 0) { lv_obj_set_hidden(nfScr, true); return; }
    if (v <= 2) nfK = v;
    else if (v == 10) { for (int k = 0; k < 3; k++) nfTouched[k] = false; if (links::navtexAskFilters()) nfAskMs = millis(); }
    else if (v == 11) {
        plotter::NavtexMask m = nfEdit[nfK];
        m.khz = nfK == 0 ? 490 : nfK == 1 ? 518 : 4209.5;
        if (links::navtexSetFilter(m)) { nfTouched[nfK] = false; nfAskMs = millis(); }
    }
    else if (v == 12) { nfEdit[nfK].stations = plotter::kNavtexAllLetters; nfEdit[nfK].subjects = plotter::kNavtexAllLetters; nfTouched[nfK] = true; }
    nfPaint();
}

static void makeNavtexFilters() {
    nfScr = lv_obj_create(lv_layer_top());
    lv_obj_remove_style_all(nfScr);
    lv_obj_set_size(nfScr, board::W, board::H);
    lv_obj_set_style_bg_color(nfScr, lv_color_hex(C_BG), 0);
    lv_obj_set_style_bg_opa(nfScr, LV_OPA_COVER, 0);
    lv_obj_set_scrollable(nfScr, false);
    lv_obj_set_clickable(nfScr, true);
    lv_obj_t* t = label(nfScr, "NAVTEX \xC2\xB7 Filtros del receptor", &dejavu_bold_20, C_TEXT);
    lv_obj_set_pos(t, 16, 18);
    lv_obj_t* b = textButton(nfScr, "Cerrar", 124, 48, onNfButton, -1);
    lv_obj_set_pos(b, 884, 8);
    static const char* const fq[3] = {"490 kHz", "518 kHz", "4209,5 kHz"};
    for (int k = 0; k < 3; k++) { nfFreqBtn[k] = textButton(nfScr, fq[k], 150, 46, onNfButton, k); lv_obj_set_pos(nfFreqBtn[k], 16 + k * 160, 70); }
    t = label(nfScr, "Estaciones (letra B1)", &dejavu_bold_16, C_TEXT);
    lv_obj_set_pos(t, 16, 136);
    t = label(nfScr, "Asuntos (letra B2)", &dejavu_bold_16, C_TEXT);
    lv_obj_set_pos(t, 16, 244);
    for (int i = 0; i < 26; i++) {
        const char l[2] = {(char)('A' + i), 0};
        nfSt[i] = textButton(nfScr, l, 34, 48, onNfLetter, i);
        lv_obj_set_pos(nfSt[i], 16 + i * 38, 164);
        nfSu[i] = textButton(nfScr, l, 34, 48, onNfLetter, 100 + i);
        lv_obj_set_pos(nfSu[i], 16 + i * 38, 272);
    }
    t = label(nfScr, "A navegaci\xC3\xB3n \xC2\xB7 B meteo (aviso) \xC2\xB7 C hielos \xC2\xB7 D salvamento \xC2\xB7 E previsi\xC3\xB3n \xC2\xB7 F pr\xC3\xA1" "cticos \xC2\xB7 G AIS \xC2\xB7 H LORAN \xC2\xB7 J sat\xC3\xA9lite \xC2\xB7 K radioayudas \xC2\xB7 L navegaci\xC3\xB3n (adicional) \xC2\xB7 T prueba \xC2\xB7 V pesca \xC2\xB7 W medio ambiente",
              &dejavu_14, C_SOFT);
    lv_obj_set_pos(t, 16, 330);
    lv_obj_set_width(t, 990);
    b = textButton(nfScr, "Leer del receptor", 200, 48, onNfButton, 10);
    lv_obj_set_pos(b, 16, 400);
    b = textButton(nfScr, "Todas", 110, 48, onNfButton, 12);
    lv_obj_set_pos(b, 226, 400);
    b = textButton(nfScr, "Enviar al receptor", 210, 48, onNfButton, 11);
    lv_obj_set_pos(b, 346, 400);
    nfNote = label(nfScr, "", &dejavu_14, C_SOFT);
    lv_obj_set_pos(nfNote, 16, 470);
    lv_obj_set_width(nfNote, 990);
    lv_obj_set_hidden(nfScr, true);
}

static bool navtexFiltersOpen() { return nfScr && !lv_obj_has_flag(nfScr, LV_OBJ_FLAG_HIDDEN); }

static void onNavtexFilters(lv_event_t*) {
    if (!nfScr) makeNavtexFilters();
    for (int k = 0; k < 3; k++) nfTouched[k] = false;
    if (links::navtexAskFilters()) nfAskMs = millis();
    nfPaint();
    lv_obj_set_hidden(nfScr, false);
}

static void onNavtexPick(lv_event_t* e) {
    const int i = (int)(intptr_t)lv_event_get_user_data(e);
    if (i < 0) { lv_obj_set_hidden(nvScr, true); return; }
    const std::vector<plotter::NavtexMessage>& v = links::navtex().items();
    if (i >= (int)v.size() || v[(size_t)i].positions.empty()) return;
    char name[16];
    snprintf(name, sizeof name, "NAVTEX %s", v[(size_t)i].id().c_str());
    lv_obj_set_hidden(nvScr, true);
    startGoTo(v[(size_t)i].positions[0], name, false);
    showTab(1);
}

static void refreshNavtexList() {
    lv_obj_clean(nvBox);
    const std::vector<plotter::NavtexMessage>& v = links::navtex().items();
    char note[260];
    snprintf(note, sizeof note, "%u mensajes, %u conservados de %u (NRX de un receptor NAVTEX o de la app APRS LoRa+).%s",
             (unsigned)v.size(), (unsigned)links::navtex().keptCount(), (unsigned)plotter::NavtexLog::kMaxKept,
             v.empty() ? "" : " Toca uno con posici\xC3\xB3n para ir a la primera. La repetici\xC3\xB3n de uno bien recibido no vuelve a salir en 72 h. No se borran a mano: sale el m\xC3\xA1s antiguo sin conservar.");
    lv_label_set_text(nvNote, note);
    for (size_t i = 0; i < v.size(); i++) {
        const plotter::NavtexMessage& m = v[i];
        lv_obj_t* b = lv_button_create(nvBox);
        lv_obj_remove_style_all(b);
        lv_obj_set_width(b, 980);
        lv_obj_set_height(b, LV_SIZE_CONTENT);
        lv_obj_set_style_bg_color(b, lv_color_hex(C_PANEL), 0);
        lv_obj_set_style_bg_opa(b, LV_OPA_COVER, 0);
        lv_obj_set_style_radius(b, 8, 0);
        lv_obj_set_style_pad_all(b, 10, 0);
        lv_obj_set_style_border_color(b, lv_color_hex(m.urgent() ? C_RED : C_LINE), 0);
        lv_obj_set_style_border_width(b, m.urgent() ? 2 : 1, 0);
        lv_obj_set_flex_flow(b, LV_FLEX_FLOW_COLUMN);
        lv_obj_add_event_cb(b, onNavtexPick, LV_EVENT_CLICKED, (void*)(intptr_t)i);
        char h[160], f[16], when[24] = "";
        nvFreq(f, sizeof f, m.khz);
        if (m.utc > 0) { int y, mo, d; double hh; plotter::unixToUtc((long long)m.utc, y, mo, d, hh); const int sod = (int)(m.utc % 86400); snprintf(when, sizeof when, " \xC2\xB7 %02d %02d:%02d UTC", d, sod / 3600, sod / 60 % 60); }
        snprintf(h, sizeof h, "%s%s \xC2\xB7 %s \xC2\xB7 %s%s%s%s", m.kept ? "CONSERVADO \xC2\xB7 " : "", m.id().c_str(), f, plotter::navtexSubjectEs(m.subject), when,
                 m.complete ? "" : " \xC2\xB7 incompleto", m.positions.empty() ? "" : " \xC2\xB7 con posici\xC3\xB3n");
        lv_obj_t* l = label(b, h, &dejavu_bold_16, m.urgent() ? C_RED : C_TEXT);
        lv_obj_set_width(l, 958);
        char q[160], er[40] = "errores: no vienen";      // calidad de la copia, como la da el receptor
        if (m.errorPct >= 0) snprintf(er, sizeof er, "errores %.1f %%", m.errorPct);
        for (char* c = er; *c; c++) if (*c == '.') *c = ',';
        const int slot = plotter::navtexInOwnSlot(m.station, m.utc);
        char rp[32] = "";
        if (m.repeats > 0) snprintf(rp, sizeof rp, " \xC2\xB7 repetido %d", m.repeats);
        snprintf(q, sizeof q, "%s \xC2\xB7 %s%s%s%s", er, m.satisfactory() ? "bien recibido" : "mal recibido (4 % o m\xC3\xA1s, o cortado)",
                 slot == 0 ? " \xC2\xB7 fuera de su turno" : slot == 1 ? " \xC2\xB7 en su turno" : "", rp, m.alwaysShown() ? " \xC2\xB7 00: se muestra siempre" : "");
        l = label(b, q, &dejavu_14, m.satisfactory() ? C_SOFT : C_WARN);
        lv_obj_set_width(l, 958);
        std::string t = m.text.size() > 600 ? m.text.substr(0, 600) + "..." : m.text;
        l = label(b, t.c_str(), &dejavu_14, C_SOFT);
        lv_obj_set_width(l, 958);
        lv_obj_t* row = lv_obj_create(b);
        lv_obj_remove_style_all(row);
        lv_obj_set_size(row, 958, 44);
        lv_obj_set_scrollable(row, false);
        lv_obj_t* kb = textButton(row, m.kept ? "Soltar" : "Conservar", 140, 40, onNavtexKeep, (intptr_t)i);
        lv_obj_set_pos(kb, 0, 4);
    }
}

static void makeNavtexList() {
    nvScr = lv_obj_create(lv_layer_top());
    lv_obj_remove_style_all(nvScr);
    lv_obj_set_size(nvScr, board::W, board::H);
    lv_obj_set_style_bg_color(nvScr, lv_color_hex(C_BG), 0);
    lv_obj_set_style_bg_opa(nvScr, LV_OPA_COVER, 0);
    lv_obj_set_scrollable(nvScr, false);
    lv_obj_set_clickable(nvScr, true);
    lv_obj_t* t = label(nvScr, "NAVTEX", &dejavu_bold_20, C_TEXT);
    lv_obj_set_pos(t, 16, 18);
    lv_obj_t* b = textButton(nvScr, "Cerrar", 124, 48, onNavtexPick, -1);
    lv_obj_set_pos(b, 884, 8);
    b = textButton(nvScr, "Filtros", 130, 48, onNavtexFilters, 0);        // estaciones y asuntos que guarda el receptor (NRM)
    lv_obj_set_pos(b, 744, 8);
    nvNote = label(nvScr, "", &dejavu_14, C_SOFT);
    lv_obj_set_pos(nvNote, 16, 64);
    lv_obj_set_width(nvNote, 980);
    nvBox = lv_obj_create(nvScr);
    lv_obj_remove_style_all(nvBox);
    lv_obj_set_pos(nvBox, 16, 116);
    lv_obj_set_size(nvBox, 992, board::H - 126);
    lv_obj_set_flex_flow(nvBox, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_gap(nvBox, 8, 0);
    lv_obj_set_scroll_dir(nvBox, LV_DIR_VER);
    lv_obj_set_hidden(nvScr, true);
}

static void openNavtexList() {
    if (!nvScr) makeNavtexList();
    refreshNavtexList();
    lv_obj_scroll_to_y(nvBox, 0, LV_ANIM_OFF);
    lv_obj_set_hidden(nvScr, false);
}
static void onNavtexOpen(lv_event_t*) { openNavtexList(); }


// ---- LSD / DSC: llamadas por la sentencia DSC (emisora con LSD o la app); las de socorro abren la ventana ----
static uint32_t dscSeq = 0;
static std::vector<std::string> dscShown;                // llamadas ya vistas en la ventana
static std::string dscModalKey;
static lv_obj_t* dscScr = nullptr;
static lv_obj_t* dscBox = nullptr;
static lv_obj_t* dscNote = nullptr;
static void openDscList();

static const plotter::DscCall* dscFind(const std::string& k) {
    for (const plotter::DscCall& c : links::dsc().items()) if (c.key() == k) return &c;
    return nullptr;
}

static void dscWhere(char* o, size_t n, const plotter::DscCall& c) {
    if (!c.hasPos) { snprintf(o, n, "Posici\xC3\xB3n: no la da"); return; }
    char pos[48];
    formatLatLon(pos, sizeof pos, c.pos, 1, true);
    std::string ps(pos);
    for (size_t i; (i = ps.find("\xE2\x80\xB2")) != std::string::npos;) ps.replace(i, 3, "'");
    char dist[48] = "";
    double la, lo;
    const uint32_t now = millis();
    if (store && store->get(Key::Lat, now, la) && store->get(Key::Lon, now, lo)) {
        Measure m = measure({la, lo}, c.pos, false);
        char d[20];
        fmtDist(d, sizeof d, m.distanceNm, 1);
        snprintf(dist, sizeof dist, " \xC2\xB7 %s \xC2\xB7 %03d\xC2\xB0", d, (int)lround(m.bearing) % 360);
    }
    if (c.time.size() == 4) snprintf(o, n, "%s a las %c%c:%c%c UTC%s", ps.c_str(), c.time[0], c.time[1], c.time[2], c.time[3], dist);
    else snprintf(o, n, "%s%s", ps.c_str(), dist);
}

static void updateDscModal() {
    const plotter::DscCall* c = dscFind(dscModalKey);
    if (!c) { closeModal(); return; }
    char sub[160], name[48], body[420], where[160];
    lv_obj_set_style_bg_color(modalHead, lv_color_hex(C_RED), 0);
    lv_label_set_text(modalTitle, "DSC DISTRESS");
    const char* what = c->format == 12 ? "Alerta de socorro" : c->f4 == 10 ? "Acuse de socorro" : c->f4 == 12 ? "Retransmisi\xC3\xB3n de socorro" : "Llamada de socorro";
    snprintf(sub, sizeof sub, "LSD \xC2\xB7 %s \xC2\xB7 %s", what, plotter::dscNatureEs(c->nature()));
    lv_label_set_text(modalSub, sub);
    snprintf(name, sizeof name, "MMSI %s", c->ship().c_str());
    lv_label_set_text(modalName, name);
    dscWhere(where, sizeof where, *c);
    int n = snprintf(body, sizeof body, "%s", where);
    if (c->format != 12) n += snprintf(body + n, sizeof body - (size_t)n, "\nLa env\xC3\xAD" "a %s (%s)", c->mmsi.c_str(), plotter::dscMmsiKindEs(c->mmsi));
    if (c->format == 12 && c->f5 >= 0) n += snprintf(body + n, sizeof body - (size_t)n, "\nDespu\xC3\xA9s: %s", plotter::dscTelecommandEs(c->f5));
    if (c->repeats > 1) snprintf(body + n, sizeof body - (size_t)n, "\nRecibida %d veces", c->repeats);
    lv_label_set_text(modalBody, body);
    lv_label_set_text(modalBtnALbl, c->hasPos ? "IR A" : "LIST");
    lv_label_set_text(modalBtnBLbl, "CLOSE");
}

static void closeDscModal(bool goTo) {
    const plotter::DscCall* c = dscFind(dscModalKey);
    const bool hasPos = c && c->hasPos;
    const LatLon p = c ? c->pos : LatLon{0, 0};
    char name[24] = "";
    if (c) snprintf(name, sizeof name, "LSD %s", c->ship().c_str());
    dscShown.push_back(dscModalKey);
    if (dscShown.size() > 40) dscShown.erase(dscShown.begin());
    dscModalKey.clear();
    closeModal();
    if (!goTo) return;
    if (hasPos) { startGoTo(p, name, false); showTab(1); } else openDscList();
}

static void refreshDscList();
static void checkDsc() {
    const plotter::DscLog& log = links::dsc();
    if (log.seq() == dscSeq) return;
    dscSeq = log.seq();
    if (dscScr && !lv_obj_has_flag(dscScr, LV_OBJ_FLAG_HIDDEN)) refreshDscList();
    if (modalMode == M_DSC) { updateDscModal(); return; }
    if (modalMode != M_NONE) { dscSeq--; return; }
    for (const plotter::DscCall& c : log.items()) {
        if (!c.distress()) continue;
        const std::string k = c.key();
        bool seen = false;
        for (const std::string& x : dscShown) if (x == k) seen = true;
        if (seen) continue;
        dscModalKey = k;
        openModal(M_DSC, 0);
        return;
    }
}

static void onDscPick(lv_event_t* e) {
    const int i = (int)(intptr_t)lv_event_get_user_data(e);
    if (i < 0) { lv_obj_set_hidden(dscScr, true); return; }
    const std::vector<plotter::DscCall>& v = links::dsc().items();
    if (i >= (int)v.size() || !v[(size_t)i].hasPos) return;
    char name[24];
    snprintf(name, sizeof name, "LSD %s", v[(size_t)i].ship().c_str());
    lv_obj_set_hidden(dscScr, true);
    startGoTo(v[(size_t)i].pos, name, false);
    showTab(1);
}

static void refreshDscList() {
    lv_obj_clean(dscBox);
    const std::vector<plotter::DscCall>& v = links::dsc().items();
    char note[200];
    snprintf(note, sizeof note, "%u llamadas (sentencia DSC de una emisora con LSD o de la app APRS LoRa+).%s", (unsigned)v.size(),
             v.empty() ? "" : " Toca una con posici\xC3\xB3n para ir a ella.");
    lv_label_set_text(dscNote, note);
    for (size_t i = 0; i < v.size(); i++) {
        const plotter::DscCall& c = v[i];
        lv_obj_t* b = lv_button_create(dscBox);
        lv_obj_remove_style_all(b);
        lv_obj_set_width(b, 980);
        lv_obj_set_height(b, LV_SIZE_CONTENT);
        lv_obj_set_style_bg_color(b, lv_color_hex(C_PANEL), 0);
        lv_obj_set_style_bg_opa(b, LV_OPA_COVER, 0);
        lv_obj_set_style_radius(b, 8, 0);
        lv_obj_set_style_pad_all(b, 10, 0);
        lv_obj_set_style_border_color(b, lv_color_hex(c.distress() ? C_RED : C_LINE), 0);
        lv_obj_set_style_border_width(b, c.distress() ? 2 : 1, 0);
        lv_obj_set_flex_flow(b, LV_FLEX_FLOW_COLUMN);
        lv_obj_add_event_cb(b, onDscPick, LV_EVENT_CLICKED, (void*)(intptr_t)i);
        char h[200], when[24] = "", rep[16] = "";
        if (c.utc > 0) { const int sod = (int)(c.utc % 86400); snprintf(when, sizeof when, " \xC2\xB7 %02d:%02d UTC", sod / 3600, sod / 60 % 60); }
        if (c.repeats > 1) snprintf(rep, sizeof rep, " \xC2\xB7 \xC3\x97%d", c.repeats);
        snprintf(h, sizeof h, "%s%s%s \xC2\xB7 de %s (%s)%s%s", plotter::dscFormatEs(c.format), c.format == 12 ? "" : " \xC2\xB7 ",
                 c.format == 12 ? "" : plotter::dscCategoryEs(c.category), c.mmsi.c_str(), plotter::dscMmsiKindEs(c.mmsi), when, rep);
        lv_obj_t* l = label(b, h, &dejavu_bold_16, c.distress() ? C_RED : C_TEXT);
        lv_obj_set_width(l, 958);
        char t[300], where[160];
        if (c.distress()) {
            dscWhere(where, sizeof where, c);
            snprintf(t, sizeof t, "%s%s%s \xC2\xB7 %s", c.format == 12 ? "" : plotter::dscTelecommandEs(c.f4), c.format == 12 ? "" : " \xC2\xB7 buque ",
                     c.format == 12 ? plotter::dscNatureEs(c.nature()) : (c.ship() + " \xC2\xB7 " + plotter::dscNatureEs(c.nature())).c_str(), where);
        } else {
            char ch[16] = "";
            if (c.field6.size() == 6 && c.field6[0] == '9') snprintf(ch, sizeof ch, " \xC2\xB7 canal %d", atoi(c.field6.c_str() + 2));
            snprintf(t, sizeof t, "%s \xC2\xB7 %s%s", plotter::dscTelecommandEs(c.f4), plotter::dscTelecommandEs(c.f5), ch);
        }
        l = label(b, t, &dejavu_14, C_SOFT);
        lv_obj_set_width(l, 958);
    }
}

static void makeDscList() {
    dscScr = lv_obj_create(lv_layer_top());
    lv_obj_remove_style_all(dscScr);
    lv_obj_set_size(dscScr, board::W, board::H);
    lv_obj_set_style_bg_color(dscScr, lv_color_hex(C_BG), 0);
    lv_obj_set_style_bg_opa(dscScr, LV_OPA_COVER, 0);
    lv_obj_set_scrollable(dscScr, false);
    lv_obj_set_clickable(dscScr, true);
    lv_obj_t* t = label(dscScr, "LSD / DSC", &dejavu_bold_20, C_TEXT);
    lv_obj_set_pos(t, 16, 18);
    lv_obj_t* b = textButton(dscScr, "Cerrar", 124, 48, onDscPick, -1);
    lv_obj_set_pos(b, 884, 8);
    dscNote = label(dscScr, "", &dejavu_14, C_SOFT);
    lv_obj_set_pos(dscNote, 16, 64);
    lv_obj_set_width(dscNote, 860);
    dscBox = lv_obj_create(dscScr);
    lv_obj_remove_style_all(dscBox);
    lv_obj_set_pos(dscBox, 16, 116);
    lv_obj_set_size(dscBox, 992, board::H - 126);
    lv_obj_set_flex_flow(dscBox, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_gap(dscBox, 8, 0);
    lv_obj_set_scroll_dir(dscBox, LV_DIR_VER);
    lv_obj_set_hidden(dscScr, true);
}

static void openDscList() {
    if (!dscScr) makeDscList();
    refreshDscList();
    lv_obj_scroll_to_y(dscBox, 0, LV_ANIM_OFF);
    lv_obj_set_hidden(dscScr, false);
}
static void onDscOpen(lv_event_t*) { openDscList(); }

// Aviso oficial nuevo: la ventana, si no hay otra de mas prioridad abierta
static void checkOfficial() {
    pollOfficial();
    if (modalMode != M_NONE) return;
    const plotter::Alert* a = oaNext();
    if (!a) return;
    oaModalId = a->id;
    openModal(M_OFFICIAL, 0);
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

static void loadLights() {
    lv_fs_file_t f;
    if (lv_fs_open(&f, kLightFile, LV_FS_MODE_RD) != LV_FS_RES_OK) return;           // sin lista: no se dibujan luces
    lightList.clear();
    std::string line;
    char buf[512];
    uint32_t br = 0;
    LightRecord r;
    while (lightList.size() < 20000 && lv_fs_read(&f, buf, sizeof(buf), &br) == LV_FS_RES_OK && br > 0) {
        for (uint32_t i = 0; i < br; i++) {
            if (buf[i] != '\n') { if (line.size() < 300) line += buf[i]; continue; }
            if (parseLightRecord(line.c_str(), r)) lightList.push_back(r);
            line.clear();
        }
    }
    if (!line.empty() && parseLightRecord(line.c_str(), r)) lightList.push_back(r);
    lv_fs_close(&f);
}

// Color de la marca de una luz
static uint32_t lightColor(const LightCharacter& c) {
    switch (c.color) {
        case LightColor::Red: return 0xFF4D4D;
        case LightColor::Green: return 0x2ECC71;
        case LightColor::Yellow: return 0xF2D43D;
        case LightColor::Blue: return 0x4DA3FF;
        default: return 0xFFF6C8;
    }
}

// Las marcas de luz encendidas o apagadas segun su ritmo (cada 100 ms, solo con la Carta a la vista)
static void lightBlink(lv_timer_t*) {
    if (!chart || lv_obj_is_hidden(chart) || lightShownN == 0) return;
    const uint32_t t = millis();
    for (int i = 0; i < lightShownN; i++) {
        const LightRecord& l = lightList[(size_t)lightShown[i]];
        const bool on = l.character.cls == LightClass::Unknown || lightIsOn(l.character, t);
        lv_obj_set_style_bg_opa(lightDot[i], on ? LV_OPA_COVER : 40, 0);
    }
}

// Luz a menos de 20 px del punto (la mas cercana), o -1
static int nearLight(LatLon p) {
    if (lightList.empty()) return -1;
    double mpp = metersPerPixel(p.lat, chZoom);
    int best = -1;
    double bestPx = 20;
    for (int i = 0; i < lightShownN; i++) {
        const LightRecord& l = lightList[(size_t)lightShown[i]];
        const double px = gcDistanceNm(p, {l.lat, l.lon}) * kMetersPerNm / mpp;
        if (px < bestPx) { bestPx = px; best = lightShown[i]; }
    }
    return best;
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
    selLight = -1;
    if (!nearWaypoint(p)) {                               // un waypoint manda sobre una luz
        selPos = p;
        selLight = nearLight(p);
        if (selLight >= 0) selPos = {lightList[(size_t)selLight].lat, lightList[(size_t)selLight].lon};
    }
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
            if (!n && selLight >= 0) n = lightList[(size_t)selLight].name;
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
    for (int i = 0; i < kLightMarks; i++) {                // luces: debajo de los waypoints
        lightDot[i] = box(chart, 0xFFF6C8, LV_RADIUS_CIRCLE);
        lv_obj_set_size(lightDot[i], 10, 10);
        lv_obj_set_style_border_color(lightDot[i], lv_color_hex(C_INK), 0);
        lv_obj_set_style_border_width(lightDot[i], 1, 0);
        lv_obj_set_style_border_opa(lightDot[i], LV_OPA_COVER, 0);
        lightLbl[i] = label(chart, "", &dejavu_12, 0xFFF6C8);
        lv_obj_set_style_bg_color(lightLbl[i], lv_color_hex(C_BAR), 0);
        lv_obj_set_style_bg_opa(lightLbl[i], 170, 0);
        lv_obj_set_style_pad_hor(lightLbl[i], 3, 0);
        lv_obj_set_style_radius(lightLbl[i], 3, 0);
        lv_obj_set_hidden(lightDot[i], true);
        lv_obj_set_hidden(lightLbl[i], true);
    }
    lv_timer_create(lightBlink, 100, nullptr);
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
    for (int i = 0; i < kLops; i++) {                     // rectas de altura
        lopLine[i] = lv_line_create(chart);
        lv_obj_set_style_line_color(lopLine[i], lv_color_hex(0xF5D547), 0);
        lv_obj_set_style_line_width(lopLine[i], 3, 0);
        lv_obj_set_hidden(lopLine[i], true);
        lopLbl[i] = label(chart, "", &dejavu_14, 0xF5D547);
        lv_obj_set_style_bg_color(lopLbl[i], lv_color_hex(C_INK), 0);
        lv_obj_set_style_bg_opa(lopLbl[i], 180, 0);
        lv_obj_set_style_pad_hor(lopLbl[i], 4, 0);
        lv_obj_set_hidden(lopLbl[i], true);
    }
    astroFixMark = box(chart, C_INK, 9);
    lv_obj_set_size(astroFixMark, 18, 18);
    lv_obj_set_style_border_color(astroFixMark, lv_color_hex(0xF5D547), 0);
    lv_obj_set_style_border_width(astroFixMark, 3, 0);
    lv_obj_set_clickable(astroFixMark, false);
    lv_obj_set_hidden(astroFixMark, true);
    lv_obj_t* asb = textButton(chart, "Astro", 110, 46, onAstroOpen, 0);
    lv_obj_align(asb, LV_ALIGN_TOP_RIGHT, -68, 128);
    lv_obj_t* cub = textButton(chart, "Corriente", 110, 46, onCurrentOpen, 0);
    lv_obj_align(cub, LV_ALIGN_TOP_RIGHT, -68, 188);
    lv_obj_t* dmb = textButton(chart, "Demoras", 110, 46, onBearingsOpen, 0);
    lv_obj_align(dmb, LV_ALIGN_TOP_RIGHT, -68, 248);
    for (int i = 0; i < 2; i++) {                         // lineas de la situacion por demoras
        brgLine[i] = lv_line_create(chart);
        lv_obj_set_style_line_color(brgLine[i], lv_color_hex(0xFF9F43), 0);
        lv_obj_set_style_line_width(brgLine[i], 3, 0);
        if (i == 0) lv_obj_set_style_line_opa(brgLine[i], 150, 0);   // la primera (trasladada), mas tenue
        lv_obj_set_hidden(brgLine[i], true);
    }
    brgFixMark = box(chart, C_INK, 9);
    lv_obj_set_size(brgFixMark, 18, 18);
    lv_obj_set_style_border_color(brgFixMark, lv_color_hex(0xFF9F43), 0);
    lv_obj_set_style_border_width(brgFixMark, 3, 0);
    lv_obj_set_clickable(brgFixMark, false);
    lv_obj_set_hidden(brgFixMark, true);
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

    // Luces de la lista: las principales (alcance >= 15 mn) desde el zoom 8, el resto desde el 11; nombres desde el 13 (las
    // principales desde el 10). Si no caben todas, primero las de mas alcance.
    lightShownN = 0;
    if (!lightList.empty() && chZoom >= 8) {
        struct Cand { int i; double x, y; double rng; };
        static Cand cand[kLightMarks];
        int nc = 0;
        const LatLon nw = worldPxToLonLat(cx - CH_W / 2.0 - 10, cy - CH_H / 2.0 - 10, chZoom);   // recuadro a la vista: lo de fuera
        const LatLon se = worldPxToLonLat(cx + CH_W / 2.0 + 10, cy + CH_H / 2.0 + 10, chZoom);   // ni se proyecta
        for (size_t i = 0; i < lightList.size(); i++) {
            const LightRecord& l = lightList[i];
            if (l.lat > nw.lat || l.lat < se.lat || l.lon < nw.lon || l.lon > se.lon) continue;
            const bool major = l.nominalNm >= 15;
            if (!major && chZoom < 11) continue;
            double x, y;
            toScreen({l.lat, l.lon}, x, y);
            if (x < -6 || y < -6 || x > CH_W + 6 || y > CH_H + 6) continue;
            if (nc < kLightMarks) { cand[nc++] = {(int)i, x, y, l.nominalNm}; continue; }
            int worst = 0;
            for (int k = 1; k < nc; k++) if (cand[k].rng < cand[worst].rng) worst = k;
            if (l.nominalNm > cand[worst].rng) cand[worst] = {(int)i, x, y, l.nominalNm};
        }
        for (int k = 0; k < nc; k++) {
            const LightRecord& l = lightList[(size_t)cand[k].i];
            const int x = (int)lround(cand[k].x), y = (int)lround(cand[k].y);
            const bool major = l.nominalNm >= 15;
            const int sz = major ? 12 : 9;
            lv_obj_set_size(lightDot[k], sz, sz);
            lv_obj_set_pos(lightDot[k], x - sz / 2, y - sz / 2);
            lv_obj_set_style_bg_color(lightDot[k], lv_color_hex(lightColor(l.character)), 0);
            lv_obj_set_hidden(lightDot[k], false);
            const bool named = chZoom >= 13 || (major && chZoom >= 10) || cand[k].i == selLight;
            if (named) {
                lv_label_set_text(lightLbl[k], l.name);
                lv_obj_set_pos(lightLbl[k], x + 8, y + 2);
            }
            lv_obj_set_hidden(lightLbl[k], !named);
            lightShown[k] = cand[k].i;
        }
        lightShownN = nc;
    }
    for (int i = lightShownN; i < kLightMarks; i++) { lv_obj_set_hidden(lightDot[i], true); lv_obj_set_hidden(lightLbl[i], true); }

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

    // Rectas de altura: 30 mn a cada lado del punto determinante, perpendicular al azimut
    for (int i = 0; i < kLops; i++) {
        if (i >= lopN) { lv_obj_set_hidden(lopLine[i], true); lv_obj_set_hidden(lopLbl[i], true); continue; }
        const LatLon ref = lops[lopN - 1].at;               // todas, trasladadas al momento de la ultima
        const plotter::Sight adv = plotter::advanceSight(lops[i].s, lops[i].at.lat, lops[i].at.lon, ref.lat, ref.lon);
        const bool moved = gcDistanceNm(lops[i].at, ref) > 0.05;
        const LatLon c = {adv.lat, adv.lon};
        double x1, y1, xc, yc;
        toScreen(c, xc, yc);
        toScreen(gcDestination(c, norm360(lops[i].s.znDeg + 90), 1.0), x1, y1);     // una milla a lo largo de la recta
        const double ppn = hypot(x1 - xc, y1 - yc);
        if (ppn < 1e-6 || fabs(xc) > 6000 || fabs(yc) > 6000) { lv_obj_set_hidden(lopLine[i], true); lv_obj_set_hidden(lopLbl[i], true); continue; }
        const double half = fmin(30.0 * ppn, 1500.0), ux = (x1 - xc) / ppn, uy = (y1 - yc) / ppn;   // 30 mn a cada lado, sin salirse mucho de la pantalla
        lopPts[i][0] = {(lv_value_precise_t)(xc + ux * half), (lv_value_precise_t)(yc + uy * half)};
        lopPts[i][1] = {(lv_value_precise_t)(xc - ux * half), (lv_value_precise_t)(yc - uy * half)};
        lv_line_set_points(lopLine[i], lopPts[i], 2);
        lv_obj_set_hidden(lopLine[i], false);
        char t[40];
        snprintf(t, sizeof(t), "%s %s%s", lops[i].name, lops[i].hhmm, moved ? " (trasl.)" : "");
        lv_label_set_text(lopLbl[i], t);
        lv_obj_set_pos(lopLbl[i], (int)lround(xc) + 8, (int)lround(yc) - 22);
        lv_obj_set_hidden(lopLbl[i], !inside(xc, yc));
    }
    if (astroFixOn) {
        double x, y;
        toScreen(astroFix, x, y);
        lv_obj_set_pos(astroFixMark, (int)lround(x) - 9, (int)lround(y) - 9);
        lv_obj_set_hidden(astroFixMark, !inside(x, y));
    } else lv_obj_set_hidden(astroFixMark, true);
    if (brgOn) {                                          // demoras: del punto (el 1, trasladado) a la situacion y una milla mas
        double fx, fy;
        toScreen(brgFix, fx, fy);
        const LatLon from[2] = {brgFirst, brgMark2};
        for (int i = 0; i < 2; i++) {
            double px, py;
            toScreen(from[i], px, py);
            const double dx = fx - px, dy = fy - py, len = hypot(dx, dy);
            double ex = fx, ey = fy;
            if (len > 1e-6) { double x1, y1; toScreen(gcDestination(brgFix, rhumbBearing(from[i], brgFix), 1.0), x1, y1); ex = x1; ey = y1; }
            if (fabs(px) > 6000 || fabs(py) > 6000 || fabs(ex) > 6000 || fabs(ey) > 6000) { lv_obj_set_hidden(brgLine[i], true); continue; }
            brgPts[i][0] = {(lv_value_precise_t)px, (lv_value_precise_t)py};
            brgPts[i][1] = {(lv_value_precise_t)ex, (lv_value_precise_t)ey};
            lv_line_set_points(brgLine[i], brgPts[i], 2);
            lv_obj_set_hidden(brgLine[i], false);
        }
        lv_obj_set_pos(brgFixMark, (int)lround(fx) - 9, (int)lround(fy) - 9);
        lv_obj_set_hidden(brgFixMark, !inside(fx, fy));
    } else {
        lv_obj_set_hidden(brgLine[0], true);
        lv_obj_set_hidden(brgLine[1], true);
        lv_obj_set_hidden(brgFixMark, true);
    }

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
        const LightRecord* sl = (!wn && selLight >= 0 && selLight < (int)lightList.size()) ? &lightList[(size_t)selLight] : nullptr;
        if (sl) {                                         // ficha de la luz: caracteristica, altura, alcance y a que distancia se vera
            double vis = 10;
            if (!store->get(Key::Visibility, now, vis) || vis <= 0) vis = 10;
            const double eye = settings::get().astroEye10 / 10.0;
            const double see = lightSightingRangeNm(*sl, vis, eye);
            char l2[160] = "", d[24] = "", sv[24] = "";
            if (see > 0) { fmtDist(sv, sizeof(sv), see, 1); }
            int n2 = snprintf(l2, sizeof(l2), "%s", sl->characterText);
            if (sl->heightM > 0) n2 += snprintf(l2 + n2, sizeof(l2) - (size_t)n2, " \xC2\xB7 %d m", (int)lround(sl->heightM));
            if (sl->nominalNm > 0) n2 += snprintf(l2 + n2, sizeof(l2) - (size_t)n2, " \xC2\xB7 %d M", (int)lround(sl->nominalNm));
            if (sv[0]) n2 += snprintf(l2 + n2, sizeof(l2) - (size_t)n2, " \xC2\xB7 se ver\xC3\xA1 a %s", sv);
            if (own) {
                Measure m = measure({la, lo}, selPos, true, sog);
                fmtDist(d, sizeof(d), m.distanceNm, 1);
                snprintf(l2 + n2, sizeof(l2) - (size_t)n2, " \xC2\xB7 a %s, %03d\xC2\xB0", d, (int)lround(m.bearing) % 360);
            }
            snprintf(pt, sizeof(pt), "%s\n%s", sl->name, l2);
        } else if (own) {
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
static const char* const kTcpKinds[] = {"TCP apagado", "TCP: NMEA 0183", "TCP: gpsd", "TCP: Signal K"};

static void onNetField(lv_event_t* e) { lv_keyboard_set_textarea(netKb, (lv_obj_t*)lv_event_get_target(e)); }

void testAbout() { lv_obj_set_hidden(aboutScr, false); }

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
    lv_textarea_set_text(netTa[5], v.owmKey);
    lv_textarea_set_text(netTa[7], v.alertZones);
    lv_textarea_set_text(netTa[8], v.alertCountry);
    snprintf(b, sizeof(b), "%06d", v.blePin);
    lv_textarea_set_text(netTa[6], b);
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
        else if (netKind == net::TCP_SIGNALK) lv_textarea_set_text(netTa[3], "8375");
    } else if (a == 1) {                                  // guardar y conectar
        settings::Values& v = settings::get();
        snprintf(v.wifiSsid, sizeof(v.wifiSsid), "%s", lv_textarea_get_text(netTa[0]));
        snprintf(v.wifiPass, sizeof(v.wifiPass), "%s", lv_textarea_get_text(netTa[1]));
        snprintf(v.tcpHost, sizeof(v.tcpHost), "%s", lv_textarea_get_text(netTa[2]));
        snprintf(v.owmKey, sizeof(v.owmKey), "%s", lv_textarea_get_text(netTa[5]));
        snprintf(v.alertZones, sizeof(v.alertZones), "%s", lv_textarea_get_text(netTa[7]));
        snprintf(v.alertCountry, sizeof(v.alertCountry), "%s", lv_textarea_get_text(netTa[8]));
        for (char* c = v.alertCountry; *c; c++) *c = (char)toupper((unsigned char)*c);
        if (lv_textarea_get_text(netTa[6])[0]) v.blePin = atoi(lv_textarea_get_text(netTa[6]));
        v.tcpKind = netKind;
        int port = atoi(lv_textarea_get_text(netTa[3]));
        v.tcpPort = (port >= 1 && port <= 65535) ? port : 10110;
        port = atoi(lv_textarea_get_text(netTa[4]));
        v.udpPort = (port >= 1 && port <= 65535) ? port : 0;
        settings::save();
        links::applyNetwork();
        links::applyBle();
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

// "Acerca de": version, copyright, licencia, proyecto, donativo (con QR) y creditos
static void onAboutClose(lv_event_t*) { lv_obj_set_hidden(aboutScr, true); }
// ---- Carta > Astro: recta de altura con el sextante ----
static lv_obj_t* vesField(int idx, int x, int y, int w, const char* hint, int maxLen, const char* chars);
static void onVesField(lv_event_t* e);
static const char* astroBodyName() {
    static const char* n[] = {"Sol (limbo inferior)", "Sol (limbo superior)", "Luna (limbo inferior)", "Luna (limbo superior)",
                              "Venus", "Marte", "J\xC3\xBApiter", "Saturno", "Estrella"};
    return n[astroBody];
}
static const int kAstroStar = 8;
static const char* astroShortName() {
    static const char* n[] = {"Sol", "Sol", "Luna", "Luna", "Venus", "Marte", "J\xC3\xBApiter", "Saturno"};
    return astroBody < kAstroStar ? n[astroBody] : plotter::navStar(astroStar).name;
}

// AHG y declinacion del astro elegido en el instante jd
// sd y hp: semidiametro y paralaje horizontal en minutos de arco
static void astroGhaDec(double jd, double& gha, double& dec, double& sd, double& hp) {
    sd = 0; hp = 0;
    if (astroBody < 2) { plotter::sunGhaDec(jd, gha, dec); sd = plotter::sunSemiDiameterArcmin(jd); hp = 0.15; return; }
    if (astroBody < 4) {
        double km;
        plotter::moonGhaDec(jd, gha, dec, km);
        sd = plotter::moonSdArcmin(km); hp = plotter::moonHpArcmin(km);
        return;
    }
    if (astroBody < kAstroStar) {
        double au;
        plotter::planetGhaDec(plotter::Planet(astroBody - 4), jd, gha, dec, au);
        hp = plotter::planetHpArcmin(au);
        return;
    }
    double sha;
    plotter::starShaDec(plotter::navStar(astroStar), jd, sha, dec);
    gha = norm360(plotter::ariesGha(jd) + sha);
}

static void astroRefresh() {
    char b[96];
    snprintf(b, sizeof(b), "Astro: %s", astroBodyName());
    lv_label_set_text(astroBodyLbl, b);
    if (astroBody < 2) { lv_label_set_text(astroStarLbl, ""); return; }
    char nm[40];
    if (astroBody == kAstroStar) snprintf(nm, sizeof(nm), "%s (mag %.1f)", plotter::navStar(astroStar).name, (double)plotter::navStar(astroStar).mag);
    else snprintf(nm, sizeof(nm), "%s", astroShortName());
    double la, lo;
    const uint32_t now = millis();
    const int64_t t = tsync::now();
    if (store->get(Key::Lat, now, la) && store->get(Key::Lon, now, lo) && t > 1600000000) {   // ahora mismo, para reconocerla
        double gha, dec, alt, az, sd, hp;
        astroGhaDec(t / 86400.0 + 2440587.5, gha, dec, sd, hp);
        plotter::bodyAltAz(gha, dec, la, lo, alt, az);
        if (alt > 0) snprintf(b, sizeof(b), "%s \xC2\xB7 ahora: altura %.0f\xC2\xB0, azimut %03.0f\xC2\xB0", nm, alt, az);
        else snprintf(b, sizeof(b), "%s \xC2\xB7 ahora bajo el horizonte", nm);
    } else snprintf(b, sizeof(b), "%s", nm);
    for (char* p = b; *p; p++) if (*p == '.' && p > b && isdigit((unsigned char)p[-1]) && isdigit((unsigned char)p[1])) *p = ',';
    lv_label_set_text(astroStarLbl, b);
}

static double astroNum(int i) {
    char d[16];
    snprintf(d, sizeof(d), "%s", lv_textarea_get_text(astroTa[i]));
    for (char* c = d; *c; c++) if (*c == ',') *c = '.';
    return atof(d);
}

static void astroNow() {
    const time_t tt = (time_t)tsync::now();
    struct tm g;
    gmtime_r(&tt, &g);
    char b[12];
    snprintf(b, sizeof(b), "%02d%02d%02d", g.tm_hour, g.tm_min, g.tm_sec);
    lv_textarea_set_text(astroTa[4], b);
}

// Lo comun a la recta y a la latitud: estima, instante, altura observada, AHG y declinacion (escribe el error en astroOut)
static bool astroInputs(double& la, double& lo, double& jd, double& ho, double& gha, double& dec, int& hh, int& mm) {
    const uint32_t now = millis();
    if (!store->get(Key::Lat, now, la) || !store->get(Key::Lon, now, lo)) {
        lv_label_set_text(astroOut, "Hace falta una posici\xC3\xB3n de estima (GPS o la \xC3\xBAltima conocida).");
        return false;
    }
    const int64_t t0 = tsync::now();
    const char* hs = lv_textarea_get_text(astroTa[4]);
    if (t0 < 1600000000 || strlen(hs) != 6) { lv_label_set_text(astroOut, "Falta la hora UTC (hhmmss) o el reloj no tiene fecha."); return false; }
    hh = (hs[0] - '0') * 10 + (hs[1] - '0');
    mm = (hs[2] - '0') * 10 + (hs[3] - '0');
    const int ss = (hs[4] - '0') * 10 + (hs[5] - '0');
    if (hh > 23 || mm > 59 || ss > 59) { lv_label_set_text(astroOut, "Hora UTC no v\xC3\xA1lida."); return false; }
    int64_t t = t0 - t0 % 86400 + hh * 3600 + mm * 60 + ss;
    if (t > t0 + 3600) t -= 86400;                        // una hora "del futuro" es de ayer
    jd = t / 86400.0 + 2440587.5;
    const double sext = astroNum(0) + astroNum(1) / 60.0;
    if (sext <= 0 || sext >= 90) { lv_label_set_text(astroOut, "Altura de sextante entre 0 y 90 grados."); return false; }
    double sd, hp;
    astroGhaDec(jd, gha, dec, sd, hp);
    if (astroBody == 2 || astroBody == 3) sd *= 1.0 + sin(deg2rad(sext)) * sin(deg2rad(hp / 60.0));   // la Luna, mas cerca en alto
    const plotter::Limb limb = astroBody >= 4 ? plotter::Limb::Center : (astroBody % 2 == 0 ? plotter::Limb::Lower : plotter::Limb::Upper);
    ho = plotter::observedAltitude(sext, astroNum(2), astroNum(3), limb, sd, hp);
    {
        const int i10 = (int)lround(astroNum(2) * 10), e10 = (int)lround(astroNum(3) * 10);
        settings::Values& sv = settings::get();
        if (abs(i10) <= 600 && e10 >= 0 && e10 <= 500 && (i10 != sv.astroIdx10 || e10 != sv.astroEye10)) {
            sv.astroIdx10 = i10; sv.astroEye10 = e10;
            settings::save();
        }
    }
    return true;
}

static void astroCompute() {
    double la, lo, jd, ho, gha, dec;
    int hh, mm;
    if (!astroInputs(la, lo, jd, ho, gha, dec, hh, mm)) return;
    AstroLop L;
    L.s = plotter::reduceSight(la, lo, gha, dec, ho);
    if (L.s.hcDeg < 0 || fabs(L.s.interceptNm) > 120) {     // astro, hora o altura equivocados: no se traza
        char w[200];
        snprintf(w, sizeof(w), "No cuadra: a esa hora el astro estar\xC3\xAD" "a a %.0f\xC2\xB0 de altura y la medida da %.0f\xC2\xB0 (%.0f mn de diferencia).\nRevisa el astro, la hora UTC y la altura.",
                 L.s.hcDeg, ho, fabs(L.s.interceptNm));
        lv_label_set_text(astroOut, w);
        return;
    }
    snprintf(L.name, sizeof(L.name), "%s", astroShortName());
    snprintf(L.hhmm, sizeof(L.hhmm), "%02d:%02d", hh, mm);
    L.at = {la, lo};
    if (lopN == kLops) { for (int i = 1; i < kLops; i++) lops[i - 1] = lops[i]; lopN--; }
    lops[lopN++] = L;
    plotter::Sight sg[kLops];                             // cada recta, referida a la estima de ahora (se tomaron desde otras)
    for (int i = 0; i < lopN; i++) {
        sg[i] = plotter::advanceSight(lops[i].s, lops[i].at.lat, lops[i].at.lon, la, lo);   // trasladada por lo navegado desde entonces
        const double dN = (sg[i].lat - la) * 60.0, dE = (sg[i].lon - lo) * 60.0 * cos(deg2rad(la));
        sg[i].interceptNm = dN * cos(deg2rad(sg[i].znDeg)) + dE * sin(deg2rad(sg[i].znDeg));
    }
    double fLat, fLon;
    astroFixOn = plotter::fixFromSights(la, lo, sg, lopN, fLat, fLon);
    if (astroFixOn) astroFix = {fLat, fLon};
    auto dm = [](double v, char* o, size_t n) { const double a = fabs(v); const int d = (int)a; snprintf(o, n, "%d\xC2\xB0 %04.1f'", d, (a - d) * 60); };
    char a1[20], a2[20], b[320];
    dm(ho, a1, sizeof(a1));
    dm(L.s.hcDeg, a2, sizeof(a2));
    int n = snprintf(b, sizeof(b), "%s %s UTC \xC2\xB7 altura observada %s \xC2\xB7 calculada %s \xC2\xB7 azimut %03.0f\xC2\xB0\nDeterminante: %.1f mn %s. Recta trazada en la carta (%d de %d).",
                     L.name, L.hhmm, a1, a2, L.s.znDeg, fabs(L.s.interceptNm), L.s.interceptNm >= 0 ? "hacia el astro" : "en contra del astro", lopN, kLops);
    if (astroFixOn && n > 0 && n < (int)sizeof(b)) {
        char p[48];
        formatLatLon(p, sizeof(p), astroFix, 1, true);
        for (char* c = p; *c; c++) if ((unsigned char)c[0] == 0xE2 && (unsigned char)c[1] == 0x80 && (unsigned char)c[2] == 0xB2) { c[0] = '\''; memmove(c + 1, c + 3, strlen(c + 3) + 1); }   // la fuente de 16 no trae el signo de minuto
        snprintf(b + n, sizeof(b) - (size_t)n, "\nSituaci\xC3\xB3n por %d rectas: %s (a %.1f mn de la estima).", lopN, p, gcDistanceNm({la, lo}, astroFix));
    } else if (lopN >= 2 && n > 0 && n < (int)sizeof(b)) snprintf(b + n, sizeof(b) - (size_t)n, "\nRectas casi paralelas: sin situaci\xC3\xB3n.");
    for (char* c = b; *c; c++) if (*c == '.' && c > b && isdigit((unsigned char)c[-1]) && isdigit((unsigned char)c[1])) *c = ',';
    lv_label_set_text(astroOut, b);
}

// ---- Astro > Latitud: por la meridiana, por la Polar o con una altura cerca del meridiano ----
static double astroBodyGha(double jd, void*) { double g, d, sd, hp; astroGhaDec(jd, g, d, sd, hp); return g; }

static void astroLatitude() {
    double la, lo, jd, ho, gha, dec;
    int hh, mm;
    if (!astroInputs(la, lo, jd, ho, gha, dec, hh, mm)) return;
    const bool polaris = astroBody == kAstroStar && astroStar == plotter::navStarFind("Polaris");
    const double lha = remainder(gha + lo, 360.0);
    auto dm = [](double v, char* o, size_t n, bool isLat) {
        const double a = fabs(v); int d = (int)a; double m = (a - d) * 60;
        if (m >= 59.95) { d++; m = 0; }
        snprintf(o, n, "%d\xC2\xB0 %04.1f'%s", d, m, isLat ? (v >= 0 ? " N" : " S") : "");
    };
    char b[420], lt[24], le[24], hc[20];
    int n = 0;
    double lat;
    if (!plotter::latitudeFromAltitude(ho, dec, lha, la, lat)) {
        n = snprintf(b, sizeof(b), "Ninguna latitud da esa altura a esa hora: revisa el astro, la hora UTC y la altura.");
    } else {
        const char* how = polaris ? "por la Polar" : (fabs(lha) <= 2.5 ? "por la meridiana" : "con la altura (fuera del meridiano)");
        dm(lat, lt, sizeof(lt), true);
        dm(la, le, sizeof(le), true);
        const double dn = (lat - la) * 60.0;
        n = snprintf(b, sizeof(b), "Latitud %s: %s \xC2\xB7 estima %s (%.1f mn al %s)", how, lt, le, fabs(dn), dn >= 0 ? "N" : "S");
        if (polaris) { char c[20]; dm(lat - ho, c, sizeof(c), false); n += snprintf(b + n, sizeof(b) - (size_t)n, "\nCorrecci\xC3\xB3n de la Polar a esa hora: %s%s", lat - ho < 0 ? "-" : "+", c); }
        else if (fabs(lha) > 2.5)
            n += snprintf(b + n, sizeof(b) - (size_t)n, "\nHorario local %.1f\xC2\xB0: depende de la longitud de estima. La buena es la del paso por el meridiano.", fabs(lha));
    }
    if (!polaris && n > 0 && n < (int)sizeof(b)) {         // cuando pasa por el meridiano y a que altura (con la estima)
        const double tp = plotter::meridianPassageJd(jd, lo, astroBodyGha, nullptr);
        double g, d, sd, hp;
        astroGhaDec(tp, g, d, sd, hp);
        const double hmax = 90.0 - fabs(la - d);
        const int64_t ut = (int64_t)llround((tp - 2440587.5) * 86400.0);
        const int sod = (int)(((ut % 86400) + 86400) % 86400);
        dm(hmax, hc, sizeof(hc), false);
        if (hmax > 0) n += snprintf(b + n, sizeof(b) - (size_t)n, "\nPaso por el meridiano: %02d:%02d:%02d UTC, altura verdadera %s al %s.",
                                    sod / 3600, sod / 60 % 60, sod % 60, hc, d < la ? "S" : "N");
        else snprintf(b + n, sizeof(b) - (size_t)n, "\nEse astro no sube sobre el horizonte en su paso por el meridiano.");
    }
    for (char* c = b; *c; c++) if (*c == '.' && c > b && isdigit((unsigned char)c[-1]) && isdigit((unsigned char)c[1])) *c = ',';
    lv_label_set_text(astroOut, b);
}

// ---- Astro > Visibles: lo que se puede observar ahora, por azimut, con las tres estrellas que mejor sitúan ----
static plotter::SkyItem astroSky[200];
static int astroSkyN = 0;
static void openAstroList();

static void onAstroPick(lv_event_t* e) {
    const int i = (int)(intptr_t)lv_event_get_user_data(e);
    lv_obj_set_hidden(astroListScr, true);
    if (i < 0 || i >= astroSkyN) return;
    const plotter::SkyItem& s = astroSky[i];
    if (s.kind == 0) astroBody = 0;
    else if (s.kind == 1) astroBody = 2;
    else if (s.kind == 2) astroBody = 4 + s.index;
    else { astroBody = kAstroStar; astroStar = s.index; }
    astroRefresh();
}

static void makeAstroList() {
    astroListScr = lv_obj_create(astroScr);
    lv_obj_remove_style_all(astroListScr);
    lv_obj_set_size(astroListScr, board::W, board::H);
    lv_obj_set_style_bg_color(astroListScr, lv_color_hex(C_BG), 0);
    lv_obj_set_style_bg_opa(astroListScr, LV_OPA_COVER, 0);
    lv_obj_set_scrollable(astroListScr, false);
    lv_obj_set_clickable(astroListScr, true);
    lv_obj_t* t = label(astroListScr, "Astros visibles ahora", &dejavu_bold_20, C_TEXT);
    lv_obj_set_pos(t, 16, 18);
    lv_obj_t* b = textButton(astroListScr, "Cerrar", 124, 48, onAstroPick, -1);
    lv_obj_set_pos(b, 884, 8);
    astroListNote = label(astroListScr, "", &dejavu_16, C_SOFT);
    lv_obj_set_pos(astroListNote, 16, 64);
    lv_obj_set_width(astroListNote, 992);
    astroListBox = lv_obj_create(astroListScr);
    lv_obj_remove_style_all(astroListBox);
    lv_obj_set_pos(astroListBox, 16, 116);
    lv_obj_set_size(astroListBox, 992, board::H - 126);
    lv_obj_set_flex_flow(astroListBox, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_style_pad_gap(astroListBox, 8, 0);
    lv_obj_set_scroll_dir(astroListBox, LV_DIR_VER);
}

static void openAstroList() {
    if (!astroListScr) makeAstroList();
    lv_obj_clean(astroListBox);
    astroSkyN = 0;
    const uint32_t now = millis();
    double la, lo;
    const int64_t t = tsync::now();
    if (!store->get(Key::Lat, now, la) || !store->get(Key::Lon, now, lo) || t < 1600000000) {
        lv_label_set_text(astroListNote, "Hace falta la posici\xC3\xB3n del barco y la hora para saber qu\xC3\xA9 astros hay sobre el horizonte.");
        lv_obj_set_hidden(astroListScr, false);
        return;
    }
    const double jd = t / 86400.0 + 2440587.5;
    astroSkyN = plotter::visibleBodies(jd, la, lo, 10, 80, astroSky, 200);
    int best[3] = {-1, -1, -1};
    const bool trio = plotter::bestStarTriple(astroSky, astroSkyN, best);
    double gha, dec, sunAlt, sunAz;
    plotter::sunGhaDec(jd, gha, dec);
    plotter::bodyAltAz(gha, dec, la, lo, sunAlt, sunAz);
    char note[260];
    const char* phase = sunAlt > -3 ? " Es de d\xC3\xAD" "a: de las estrellas solo se ven las m\xC3\xA1s brillantes, y con dificultad."
                      : (sunAlt < -12 ? " Noche cerrada: sin horizonte visible las alturas no son fiables."
                                      : " Crep\xC3\xBAsculo: buen momento para tomar estrellas.");
    snprintf(note, sizeof(note), "Entre 10\xC2\xB0 y 80\xC2\xB0 de altura, por azimut.%s%s", phase,
             trio ? "\nEn amarillo, las tres estrellas que mejor reparten el horizonte." : "");
    lv_label_set_text(astroListNote, note);
    static const char* pn[] = {"Venus", "Marte", "J\xC3\xBApiter", "Saturno"};
    for (int i = 0; i < astroSkyN; i++) {
        const plotter::SkyItem& s = astroSky[i];
        const bool pick = trio && (i == best[0] || i == best[1] || i == best[2]);
        char txt[64];
        const char* nm = s.kind == 0 ? "Sol" : (s.kind == 1 ? "Luna" : (s.kind == 2 ? pn[s.index & 3] : plotter::navStar(s.index).name));
        snprintf(txt, sizeof(txt), "%s  %.0f\xC2\xB0 / %03.0f\xC2\xB0", nm, (double)s.alt, (double)s.az);
        lv_obj_t* b = textButton(astroListBox, txt, 324, 44, onAstroPick, i);
        if (s.kind < 3 || s.index < plotter::navStarCount())                // los de las tablas diarias, en blanco; los demas, apagados
            lv_obj_set_style_text_color(lv_obj_get_child(b, 0), lv_color_hex(C_TEXT), 0);
        else lv_obj_set_style_text_color(lv_obj_get_child(b, 0), lv_color_hex(C_SOFT), 0);
        if (pick) {
            lv_obj_set_style_border_color(b, lv_color_hex(C_WARN), 0);
            lv_obj_set_style_border_width(b, 3, 0);
            lv_obj_set_style_text_color(lv_obj_get_child(b, 0), lv_color_hex(C_WARN), 0);
        }
    }
    if (!astroSkyN) lv_label_set_text(astroListNote, "Ning\xC3\xBAn astro entre 10\xC2\xB0 y 80\xC2\xB0 de altura.");
    lv_obj_scroll_to_y(astroListBox, 0, LV_ANIM_OFF);
    lv_obj_set_hidden(astroListScr, false);
}

static void onAstroBtn(lv_event_t* e) {
    const int a = (int)(intptr_t)lv_event_get_user_data(e);
    if (a == 0) astroBody = (astroBody + 1) % 9;
    else if (a == 1) { astroBody = kAstroStar; astroStar = (astroStar + plotter::starCount() - 1) % plotter::starCount(); }
    else if (a == 2) { astroBody = kAstroStar; astroStar = (astroStar + 1) % plotter::starCount(); }
    else if (a == 3) astroNow();
    else if (a == 4) astroCompute();
    else if (a == 5) { lopN = 0; astroFixOn = false; lv_label_set_text(astroOut, "Rectas borradas."); }
    else if (a == 7) { openAstroList(); return; }
    else if (a == 8) { openCt(); return; }
    else if (a == 9) astroLatitude();
    else if (a == 10) { const int p = plotter::navStarFind("Polaris"); if (p >= 0) { astroBody = kAstroStar; astroStar = p; } }
    else { lv_obj_set_hidden(astroScr, true); return; }
    astroRefresh();
}

static void onAstroField(lv_event_t* e) { lv_keyboard_set_textarea(astroKb, (lv_obj_t*)lv_event_get_target(e)); }

static void openAstro() {
    if (!lv_textarea_get_text(astroTa[4])[0]) astroNow();
    lv_keyboard_set_textarea(astroKb, astroTa[0]);
    astroRefresh();
    lv_obj_set_hidden(astroScr, false);
}

static void makeAstro() {
    astroScr = lv_obj_create(lv_layer_top());
    lv_obj_remove_style_all(astroScr);
    lv_obj_set_size(astroScr, board::W, board::H);
    lv_obj_set_style_bg_color(astroScr, lv_color_hex(C_BG), 0);
    lv_obj_set_style_bg_opa(astroScr, LV_OPA_COVER, 0);
    lv_obj_set_scrollable(astroScr, false);
    lv_obj_set_clickable(astroScr, true);
    lv_obj_t* t = label(astroScr, "Astro", &dejavu_bold_20, C_TEXT);
    lv_obj_set_pos(t, 16, 18);
    lv_obj_t* b = textButton(astroScr, "Latitud", 110, 48, onAstroBtn, 9);
    lv_obj_set_pos(b, 184, 8);
    b = textButton(astroScr, "Desv\xC3\xADo (Ct)", 176, 48, onAstroBtn, 8);
    lv_obj_set_pos(b, 300, 8);
    b = textButton(astroScr, "Calcular y trazar", 220, 48, onAstroBtn, 4);
    lv_obj_set_pos(b, 484, 8);
    b = textButton(astroScr, "Borrar rectas", 160, 48, onAstroBtn, 5);
    lv_obj_set_pos(b, 712, 8);
    b = textButton(astroScr, "Cerrar", 124, 48, onAstroBtn, 6);
    lv_obj_set_pos(b, 884, 8);
    b = textButton(astroScr, "", 280, 48, onAstroBtn, 0);
    lv_obj_set_pos(b, 16, 66);
    astroBodyLbl = lv_obj_get_child(b, 0);
    lv_obj_set_style_text_font(astroBodyLbl, &dejavu_bold_16, 0);
    b = textButton(astroScr, "<", 52, 48, onAstroBtn, 1);
    lv_obj_set_pos(b, 304, 66);
    b = textButton(astroScr, ">", 52, 48, onAstroBtn, 2);
    lv_obj_set_pos(b, 364, 66);
    b = textButton(astroScr, "Visibles", 108, 48, onAstroBtn, 7);
    lv_obj_set_pos(b, 424, 66);
    b = textButton(astroScr, "Polar", 76, 48, onAstroBtn, 10);
    lv_obj_set_pos(b, 540, 66);
    astroStarLbl = label(astroScr, "", &dejavu_16, C_SOFT);
    lv_obj_set_pos(astroStarLbl, 626, 80);
    lv_obj_set_width(astroStarLbl, 390);
    static const struct { int x, w, max; const char* hint; const char* chars; } F[5] = {
        {16, 150, 2, "Grados", "0123456789"}, {174, 170, 4, "Minutos", "0123456789.,"}, {352, 200, 5, "Error \xC3\xADndice, '", "-0123456789.,"},
        {560, 190, 4, "Ojo, m", "0123456789.,"}, {758, 150, 6, "UTC hhmmss", "0123456789"}};
    lv_obj_t* keep = vesScr;                              // vesField crea sus campos en vesScr: se le presta esta pantalla
    lv_obj_t* keepTa[5];
    for (int i = 0; i < 5; i++) keepTa[i] = vesTa[i];
    vesScr = astroScr;
    for (int i = 0; i < 5; i++) {
        lv_obj_t* ta = vesField(i, F[i].x, 124, F[i].w, F[i].hint, F[i].max, F[i].chars);
        lv_obj_remove_event_cb(ta, onVesField);
        lv_obj_add_event_cb(ta, onAstroField, LV_EVENT_FOCUSED, nullptr);
        lv_obj_add_event_cb(ta, onAstroField, LV_EVENT_CLICKED, nullptr);
        astroTa[i] = ta;
        vesTa[i] = keepTa[i];
    }
    vesScr = keep;
    char sv[12];                                           // error de indice y elevacion del ojo: los ultimos usados
    snprintf(sv, sizeof(sv), "%.1f", settings::get().astroIdx10 / 10.0);
    lv_textarea_set_text(astroTa[2], sv);
    snprintf(sv, sizeof(sv), "%.1f", settings::get().astroEye10 / 10.0);
    lv_textarea_set_text(astroTa[3], sv);
    b = textButton(astroScr, "Ahora", 92, 48, onAstroBtn, 3);
    lv_obj_set_pos(b, 916, 124);
    astroOut = label(astroScr, "Mide la altura con el sextante, pulsa Ahora en el instante de la medida y escribe grados y minutos.\nLa estima es la posici\xC3\xB3n del barco en la carta.", &dejavu_16, C_SOFT);
    lv_obj_set_pos(astroOut, 16, 186);
    lv_obj_set_width(astroOut, 992);
    astroKb = lv_keyboard_create(astroScr);
    lv_keyboard_set_mode(astroKb, LV_KEYBOARD_MODE_NUMBER);
    lv_obj_set_size(astroKb, board::W, 300);
    lv_obj_align(astroKb, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_color(astroKb, lv_color_hex(C_BG), 0);
    lv_obj_set_style_bg_opa(astroKb, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(astroKb, 6, 0);
    lv_obj_set_style_pad_gap(astroKb, 6, 0);
    lv_obj_set_style_border_width(astroKb, 0, 0);
    lv_obj_set_style_bg_color(astroKb, lv_color_hex(C_PANEL), LV_PART_ITEMS);
    lv_obj_set_style_bg_opa(astroKb, LV_OPA_COVER, LV_PART_ITEMS);
    lv_obj_set_style_bg_color(astroKb, lv_color_hex(C_ACC), LV_PART_ITEMS | LV_STATE_PRESSED);
    lv_obj_set_style_text_color(astroKb, lv_color_hex(C_TEXT), LV_PART_ITEMS);
    lv_obj_set_style_radius(astroKb, 8, LV_PART_ITEMS);
    lv_obj_set_style_border_width(astroKb, 0, LV_PART_ITEMS);
    lv_obj_set_style_shadow_width(astroKb, 0, LV_PART_ITEMS);
    lv_obj_set_hidden(astroScr, true);
}

// Para las pruebas en el PC: abre la pantalla y traza la recta de una estrella con la altura que tendria ahora (+ err minutos)
void testAstro(const char* star, double errArcmin, bool latitude) {
    openAstro();
    const int i = plotter::navStarFind(star);
    static const char* bodies[] = {"luna", "venus", "marte", "jupiter", "saturno"};
    astroBody = i >= 0 ? kAstroStar : 0;
    for (int k = 0; k < 5; k++) if (!strcmp(star, bodies[k])) astroBody = k == 0 ? 2 : 3 + k;
    if (i >= 0) astroStar = i;
    astroNow();
    double la, lo, gha, dec, alt, az, sd, hp;
    if (store->get(Key::Lat, millis(), la) && store->get(Key::Lon, millis(), lo)) {
        astroGhaDec(tsync::now() / 86400.0 + 2440587.5, gha, dec, sd, hp);
        plotter::bodyAltAz(gha, dec, la, lo, alt, az);
        const double hs = alt + errArcmin / 60.0 + plotter::refractionArcmin(alt) / 60.0 + plotter::dipArcmin(2) / 60.0 -
                          (astroBody < 4 ? sd / 60.0 : 0) - hp * cos(deg2rad(alt)) / 60.0;
        char b[12];
        snprintf(b, sizeof(b), "%d", (int)hs);
        lv_textarea_set_text(astroTa[0], b);
        snprintf(b, sizeof(b), "%.1f", (hs - (int)hs) * 60);
        lv_textarea_set_text(astroTa[1], b);
    }
    if (latitude) astroLatitude();
    else astroCompute();
    astroRefresh();
}


// ---- Astro > Desvio (Ct): correccion total con la demora de aguja de un astro, y tablilla de desvios ----
static void ctComma(char* b) { for (char* p = b; *p; p++) if (*p == '.' && p > b && isdigit((unsigned char)p[-1]) && isdigit((unsigned char)p[1])) *p = ','; }
static double ctNum(int i, bool& ok) {
    char d[16];
    snprintf(d, sizeof(d), "%s", lv_textarea_get_text(ctTa[i]));
    ok = d[0] != 0;
    for (char* c = d; *c; c++) if (*c == ',') *c = '.';
    return atof(d);
}
static const char* ctSign(double v) { return v >= 0 ? "E" : "W"; }

static void ctRefresh() {
    char b[80];
    snprintf(b, sizeof(b), "Astro: %s", astroShortName());
    lv_label_set_text(ctBodyLbl, b);
}

static void ctNow() {
    const time_t tt = (time_t)tsync::now();
    struct tm g;
    gmtime_r(&tt, &g);
    char b[12];
    snprintf(b, sizeof(b), "%02d%02d%02d", g.tm_hour, g.tm_min, g.tm_sec);
    lv_textarea_set_text(ctTa[2], b);
}

static void ctHeading() {                                 // rumbo de aguja: el del compas del barco, si llega
    double hm;
    if (!store->get(Key::HeadingMag, millis(), hm)) return;
    char b[12];
    snprintf(b, sizeof(b), "%.0f", norm360(hm));
    lv_textarea_set_text(ctTa[1], b);
}

static void ctCompute() {
    ctHave = false;
    const uint32_t now = millis();
    double la, lo;
    if (!store->get(Key::Lat, now, la) || !store->get(Key::Lon, now, lo)) {
        lv_label_set_text(ctOut, "Hace falta la posici\xC3\xB3n del barco (GPS o la \xC3\xBAltima conocida).");
        return;
    }
    const links::Status& ls = links::status();
    if (!ls.hasDeclination) { lv_label_set_text(ctOut, "Falta la declinaci\xC3\xB3n magn\xC3\xA9tica (se calcula con la posici\xC3\xB3n)."); return; }
    const int64_t t0 = tsync::now();
    const char* hs = lv_textarea_get_text(ctTa[2]);
    if (t0 < 1600000000 || strlen(hs) != 6) { lv_label_set_text(ctOut, "Falta la hora UTC (hhmmss) o el reloj no tiene fecha."); return; }
    const int hh = (hs[0] - '0') * 10 + (hs[1] - '0'), mm = (hs[2] - '0') * 10 + (hs[3] - '0'), ss = (hs[4] - '0') * 10 + (hs[5] - '0');
    if (hh > 23 || mm > 59 || ss > 59) { lv_label_set_text(ctOut, "Hora UTC no v\xC3\xA1lida."); return; }
    int64_t t = t0 - t0 % 86400 + hh * 3600 + mm * 60 + ss;
    if (t > t0 + 3600) t -= 86400;
    bool okDa, okRa;
    const double da = ctNum(0, okDa), ra = ctNum(1, okRa);
    if (!okDa || da < 0 || da >= 360) { lv_label_set_text(ctOut, "Escribe la demora de aguja del astro (0 a 359,9\xC2\xB0)."); return; }
    if (!okRa || ra < 0 || ra >= 360) { lv_label_set_text(ctOut, "Escribe el rumbo de aguja del barco al tomar la demora (0 a 359\xC2\xB0)."); return; }
    double gha, dec, sd, hp, alt, zv;
    astroGhaDec(t / 86400.0 + 2440587.5, gha, dec, sd, hp);
    plotter::bodyAltAz(gha, dec, la, lo, alt, zv);
    if (alt < -1) {
        char w[120];
        snprintf(w, sizeof(w), "A esa hora %s est\xC3\xA1 bajo el horizonte (%.0f\xC2\xB0). Revisa el astro y la hora UTC.", astroShortName(), alt);
        lv_label_set_text(ctOut, w);
        return;
    }
    const double ct = plotter::totalCorrection(zv, da);
    const double dm = ls.declination;
    const double dev = plotter::deviationFromCt(ct, dm);
    char b[420];
    int n = snprintf(b, sizeof(b),
                     "%s %02d:%02d UTC \xC2\xB7 azimut verdadero %05.1f\xC2\xB0 (altura %.0f\xC2\xB0) \xC2\xB7 demora de aguja %05.1f\xC2\xB0\n"
                     "Correcci\xC3\xB3n total %+.1f\xC2\xB0 (%s) \xC2\xB7 declinaci\xC3\xB3n %.1f\xC2\xB0 %s \xC2\xB7 desv\xC3\xADo %+.1f\xC2\xB0 (%s) al rumbo de aguja %03.0f\xC2\xB0",
                     astroShortName(), hh, mm, zv, alt, da, ct, ctSign(ct), fabs(dm), ctSign(dm), dev, ctSign(dev), ra);
    const plotter::DeviationCard& card = links::deviationCard();
    if (card.terms && n > 0 && n < (int)sizeof(b))
        n += snprintf(b + n, sizeof(b) - (size_t)n, "\nLa tablilla daba %+.1f\xC2\xB0 a ese rumbo.", card.at(ra));
    if (fabs(dev) > 20 && n > 0 && n < (int)sizeof(b))
        n += snprintf(b + n, sizeof(b) - (size_t)n, "\nDesv\xC3\xADo muy grande: revisa la demora, el astro y la hora.");
    else if (alt > 60 && n > 0 && n < (int)sizeof(b))
        n += snprintf(b + n, sizeof(b) - (size_t)n, "\nCon el astro tan alto la demora es poco precisa: mejor bajo, por debajo de 30\xC2\xB0.");
    ctComma(b);
    lv_label_set_text(ctOut, b);
    ctHave = fabs(dev) <= 20;
    ctLastRa = ra;
    ctLastDev = dev;
}

static void ctCardOpen();
static void onCtCardPick(lv_event_t* e) {
    const int a = (int)(intptr_t)lv_event_get_user_data(e);
    plotter::DeviationCard& card = links::deviationCard();
    if (a == -1) { lv_obj_set_hidden(ctCardScr, true); return; }
    if (a == -2) { card.clear(); links::saveDeviationCard(); }
    else if (a == -3) { settings::get().devOn = !settings::get().devOn; settings::save(); }
    else if (a >= 0 && a < card.n) { card.remove(a); links::saveDeviationCard(); }
    ctCardOpen();
}

static void makeCtCard() {
    ctCardScr = lv_obj_create(ctScr);
    lv_obj_remove_style_all(ctCardScr);
    lv_obj_set_size(ctCardScr, board::W, board::H);
    lv_obj_set_style_bg_color(ctCardScr, lv_color_hex(C_BG), 0);
    lv_obj_set_style_bg_opa(ctCardScr, LV_OPA_COVER, 0);
    lv_obj_set_scrollable(ctCardScr, false);
    lv_obj_set_clickable(ctCardScr, true);
    lv_obj_t* t = label(ctCardScr, "Tablilla de desv\xC3\xADos", &dejavu_bold_20, C_TEXT);
    lv_obj_set_pos(t, 16, 18);
    ctApplyBtn = textButton(ctCardScr, "", 300, 48, onCtCardPick, -3);
    lv_obj_set_pos(ctApplyBtn, 440, 8);
    lv_obj_t* b = textButton(ctCardScr, "Vaciar", 124, 48, onCtCardPick, -2);
    lv_obj_set_pos(b, 752, 8);
    b = textButton(ctCardScr, "Cerrar", 124, 48, onCtCardPick, -1);
    lv_obj_set_pos(b, 884, 8);
    ctCardNote = label(ctCardScr, "", &dejavu_16, C_SOFT);
    lv_obj_set_pos(ctCardNote, 16, 66);
    lv_obj_set_width(ctCardNote, 992);
    ctCardBox = lv_obj_create(ctCardScr);
    lv_obj_remove_style_all(ctCardBox);
    lv_obj_set_pos(ctCardBox, 16, 250);
    lv_obj_set_size(ctCardBox, 992, board::H - 260);
    lv_obj_set_flex_flow(ctCardBox, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_style_pad_gap(ctCardBox, 8, 0);
    lv_obj_set_scroll_dir(ctCardBox, LV_DIR_VER);
}

static void ctCardOpen() {
    if (!ctCardScr) makeCtCard();
    const plotter::DeviationCard& card = links::deviationCard();
    const bool on = settings::get().devOn;
    lv_label_set_text(lv_obj_get_child(ctApplyBtn, 0), on ? "Aplicar al rumbo: S\xC3\x8D" : "Aplicar al rumbo: NO");
    lv_obj_set_style_text_color(lv_obj_get_child(ctApplyBtn, 0), lv_color_hex(on ? C_WARN : C_TEXT), 0);
    char b[700];
    int n;
    if (!card.terms) n = snprintf(b, sizeof(b), "Sin observaciones. Calcula el desv\xC3\xADo con un astro y pulsa \xC2\xAB" "A la tablilla\xC2\xBB; mejor a rumbos repartidos (cada 45\xC2\xB0).");
    else {
        static const char* how[] = {"", "solo el desv\xC3\xADo medio (A)", "", "A, B y C (hacen falta 5 rumbos repartidos para D y E)", "", "A, B, C, D y E"};
        n = snprintf(b, sizeof(b), "%d observaciones \xC2\xB7 curva con %s \xC2\xB7 A %+.1f  B %+.1f  C %+.1f  D %+.1f  E %+.1f \xC2\xB7 residuo %.1f\xC2\xB0\n",
                     card.n, how[card.terms], card.coef[0], card.coef[1], card.coef[2], card.coef[3], card.coef[4], card.rmsDeg);
        for (int h = 0; h < 360 && n > 0 && n < (int)sizeof(b); h += 30)
            n += snprintf(b + n, sizeof(b) - (size_t)n, "%s%03d\xC2\xB0 %+.1f", h ? (h == 180 ? "\n" : "   ") : "", h, card.at(h));
    }
    if (n > 0 && n < (int)sizeof(b))
        snprintf(b + n, sizeof(b) - (size_t)n, "\n\nCon \xC2\xAB" "Aplicar\xC2\xBB, el rumbo verdadero sale de rumbo de aguja + desv\xC3\xADo + declinaci\xC3\xB3n (solo si el comp\xC3\xA1s no lo corrige ya).\nObservaciones (rumbo de aguja y desv\xC3\xADo); toca una para quitarla:");
    ctComma(b);
    lv_label_set_text(ctCardNote, b);
    lv_obj_clean(ctCardBox);
    for (int i = 0; i < card.n; i++) {
        char txt[40];
        snprintf(txt, sizeof(txt), "%03.0f\xC2\xB0  %+.1f\xC2\xB0 %s", (double)card.p[i].compassHdg, (double)card.p[i].dev, ctSign(card.p[i].dev));
        ctComma(txt);
        textButton(ctCardBox, txt, 236, 44, onCtCardPick, i);
    }
    lv_obj_set_hidden(ctCardScr, false);
}

static void ctAdd() {
    if (!ctHave) { lv_label_set_text(ctOut, "Primero calcula un desv\xC3\xADo v\xC3\xA1lido."); return; }
    links::deviationCard().add(ctLastRa, ctLastDev);
    links::saveDeviationCard();
    ctHave = false;
    char b[120];
    snprintf(b, sizeof(b), "A\xC3\xB1" "adido a la tablilla: rumbo de aguja %03.0f\xC2\xB0, desv\xC3\xADo %+.1f\xC2\xB0 (%d observaciones).", ctLastRa, ctLastDev, links::deviationCard().n);
    ctComma(b);
    lv_label_set_text(ctOut, b);
}

static void onCtBtn(lv_event_t* e) {
    const int a = (int)(intptr_t)lv_event_get_user_data(e);
    if (a == 0) { astroBody = (astroBody + 1) % 9; if (astroBody == 1 || astroBody == 3) astroBody++; }   // sin limbos
    else if (a == 1) { astroBody = kAstroStar; astroStar = (astroStar + plotter::starCount() - 1) % plotter::starCount(); }
    else if (a == 2) { astroBody = kAstroStar; astroStar = (astroStar + 1) % plotter::starCount(); }
    else if (a == 3) ctNow();
    else if (a == 4) ctCompute();
    else if (a == 5) ctAdd();
    else if (a == 6) { ctCardOpen(); return; }
    else if (a == 7) { const int i = plotter::navStarFind("Polaris"); if (i >= 0) { astroBody = kAstroStar; astroStar = i; } }
    else if (a == 8) ctHeading();
    else { lv_obj_set_hidden(ctScr, true); astroRefresh(); return; }
    ctRefresh();
}

static void onCtField(lv_event_t* e) { lv_keyboard_set_textarea(ctKb, (lv_obj_t*)lv_event_get_target(e)); }

static void makeCt() {
    ctScr = lv_obj_create(lv_layer_top());
    lv_obj_remove_style_all(ctScr);
    lv_obj_set_size(ctScr, board::W, board::H);
    lv_obj_set_style_bg_color(ctScr, lv_color_hex(C_BG), 0);
    lv_obj_set_style_bg_opa(ctScr, LV_OPA_COVER, 0);
    lv_obj_set_scrollable(ctScr, false);
    lv_obj_set_clickable(ctScr, true);
    lv_obj_t* t = label(ctScr, "Correcci\xC3\xB3n total y desv\xC3\xADo", &dejavu_bold_20, C_TEXT);
    lv_obj_set_pos(t, 16, 18);
    lv_obj_t* b = textButton(ctScr, "Calcular", 140, 48, onCtBtn, 4);
    lv_obj_set_pos(b, 392, 8);
    b = textButton(ctScr, "A la tablilla", 160, 48, onCtBtn, 5);
    lv_obj_set_pos(b, 540, 8);
    b = textButton(ctScr, "Tablilla", 124, 48, onCtBtn, 6);
    lv_obj_set_pos(b, 708, 8);
    b = textButton(ctScr, "Cerrar", 136, 48, onCtBtn, 9);
    lv_obj_set_pos(b, 872, 8);
    b = textButton(ctScr, "", 280, 48, onCtBtn, 0);
    lv_obj_set_pos(b, 16, 66);
    ctBodyLbl = lv_obj_get_child(b, 0);
    lv_obj_set_style_text_font(ctBodyLbl, &dejavu_bold_16, 0);
    b = textButton(ctScr, "<", 52, 48, onCtBtn, 1);
    lv_obj_set_pos(b, 304, 66);
    b = textButton(ctScr, ">", 52, 48, onCtBtn, 2);
    lv_obj_set_pos(b, 364, 66);
    b = textButton(ctScr, "Polar", 92, 48, onCtBtn, 7);
    lv_obj_set_pos(b, 424, 66);
    static const struct { int x, w, max; const char* hint; const char* chars; } F[3] = {
        {16, 260, 5, "Demora de aguja, \xC2\xB0", "0123456789.,"}, {284, 260, 3, "Rumbo de aguja, \xC2\xB0", "0123456789"},
        {552, 180, 6, "UTC hhmmss", "0123456789"}};
    lv_obj_t* keep = vesScr;                              // vesField crea sus campos en vesScr: se le presta esta pantalla
    lv_obj_t* keepTa[3];
    for (int i = 0; i < 3; i++) keepTa[i] = vesTa[i];
    vesScr = ctScr;
    for (int i = 0; i < 3; i++) {
        lv_obj_t* ta = vesField(i, F[i].x, 124, F[i].w, F[i].hint, F[i].max, F[i].chars);
        lv_obj_remove_event_cb(ta, onVesField);
        lv_obj_add_event_cb(ta, onCtField, LV_EVENT_FOCUSED, nullptr);
        lv_obj_add_event_cb(ta, onCtField, LV_EVENT_CLICKED, nullptr);
        ctTa[i] = ta;
        vesTa[i] = keepTa[i];
    }
    vesScr = keep;
    b = textButton(ctScr, "Proa", 84, 48, onCtBtn, 8);
    lv_obj_set_pos(b, 740, 124);
    b = textButton(ctScr, "Ahora", 92, 48, onCtBtn, 3);
    lv_obj_set_pos(b, 916, 124);
    ctOut = label(ctScr, "Toma la demora del astro con la aguja, pulsa Ahora en ese instante y escribe la demora y el rumbo de aguja del barco.\nMejor con el astro bajo (Sol al salir o al ponerse, la Polar).", &dejavu_16, C_SOFT);
    lv_obj_set_pos(ctOut, 16, 186);
    lv_obj_set_width(ctOut, 992);
    ctKb = lv_keyboard_create(ctScr);
    lv_keyboard_set_mode(ctKb, LV_KEYBOARD_MODE_NUMBER);
    lv_obj_set_size(ctKb, board::W, 300);
    lv_obj_align(ctKb, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_color(ctKb, lv_color_hex(C_BG), 0);
    lv_obj_set_style_bg_opa(ctKb, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(ctKb, 6, 0);
    lv_obj_set_style_pad_gap(ctKb, 6, 0);
    lv_obj_set_style_border_width(ctKb, 0, 0);
    lv_obj_set_style_bg_color(ctKb, lv_color_hex(C_PANEL), LV_PART_ITEMS);
    lv_obj_set_style_bg_opa(ctKb, LV_OPA_COVER, LV_PART_ITEMS);
    lv_obj_set_style_bg_color(ctKb, lv_color_hex(C_ACC), LV_PART_ITEMS | LV_STATE_PRESSED);
    lv_obj_set_style_text_color(ctKb, lv_color_hex(C_TEXT), LV_PART_ITEMS);
    lv_obj_set_style_radius(ctKb, 8, LV_PART_ITEMS);
    lv_obj_set_style_border_width(ctKb, 0, LV_PART_ITEMS);
    lv_obj_set_style_shadow_width(ctKb, 0, LV_PART_ITEMS);
    lv_obj_set_hidden(ctScr, true);
}

static void openCt() {
    if (!ctScr) makeCt();
    if (astroBody == 1 || astroBody == 3) astroBody--;    // el limbo no importa para el azimut
    if (!lv_textarea_get_text(ctTa[2])[0]) ctNow();
    if (!lv_textarea_get_text(ctTa[1])[0]) ctHeading();
    lv_keyboard_set_textarea(ctKb, ctTa[0]);
    ctRefresh();
    lv_obj_set_hidden(ctScr, false);
}

// Para las pruebas en el PC: con el astro elegido, un compas con desvio dev y rumbos de aguja cada 'step' grados
void testCt(const char* star, double dev, int step) {
    openAstro();
    const int i = plotter::navStarFind(star);
    if (i >= 0) { astroBody = kAstroStar; astroStar = i; } else astroBody = 0;
    openCt();
    double la, lo, gha, dec, sd, hp, alt, zv;
    if (!store->get(Key::Lat, millis(), la) || !store->get(Key::Lon, millis(), lo)) return;
    astroGhaDec(tsync::now() / 86400.0 + 2440587.5, gha, dec, sd, hp);
    plotter::bodyAltAz(gha, dec, la, lo, alt, zv);
    const double dm = links::status().declination;
    for (int h = 0; h < 360; h += step > 0 ? step : 360) {
        const double r = deg2rad(h), d = dev + 2.5 * sin(r) - 1.5 * cos(r) + 0.8 * sin(2 * r);   // curva de un compas real
        char b[16];
        ctNow();
        snprintf(b, sizeof(b), "%.1f", norm360(zv - d - dm));
        lv_textarea_set_text(ctTa[0], b);
        snprintf(b, sizeof(b), "%d", h);
        lv_textarea_set_text(ctTa[1], b);
        ctCompute();
        if (step > 0) ctAdd();
    }
}
void testCtCard() { if (ctScr) ctCardOpen(); }

// Para las pruebas en el PC: mensajes NAVTEX de ejemplo por NRX (uno de salvamento abre la ventana) y la lista
void testNavtex(bool list) {
    static const char* const lines[] = {
        "$CRNRX,GA05,1,3,518,15,11,2026,130500,0.8,TARIFA NAVTEX NAV WARNING 123/26",
        "$CRNRX,GA05,2,3,518,15,11,2026,130500,0.8,WRECK IN 36-07.5N 005-21.2W.",
        "$CRNRX,GA05,3,3,518,15,11,2026,130500,0.8,VESSELS KEEP CLEAR 1 MILE.",
        "$CRNRX,XE41,1,2,518,15,11,2026,155200,0.0,FORECAST CABO DE GATA TO CABO DE LA NAO",
        "$CRNRX,XE41,2,2,518,15,11,2026,155200,0.0,EAST 4 TO 5, LOCALLY 6. MODERATE SEA.",
        "$CRNRX,XD12,1,2,518,15,11,2026,140200,1.2,MAYDAY RELAY. YACHT TAKING WATER IN 3810N 00012W",
        "$CRNRX,XD12,2,2,518,15,11,2026,140200,1.2,VESSELS IN VICINITY KEEP SHARP LOOKOUT AND REPORT TO MRCC VALENCIA",
    };
    links::navtex().load("");                            // el dibujo no depende de lo que hubiera en la tarjeta de pruebas
    nvShown.clear();
    for (const char* l : lines) links::injectLine(l);
    if (list) { closeModal(); nvShown.push_back("XD12@518"); openNavtexList(); }
}

// Para las pruebas en el PC: la pantalla de filtros con lo que contesta un receptor (Navtex) a la consulta, y un cambio sin enviar
void testNavtexFilters() {
    closeModal();
    links::injectLine("$CRNRM,1,1,03FFFFFF,0000083F*3E");
    links::injectLine("$CRNRM,1,2,00800040,0000081B*34");
    links::injectLine("$CRNRM,1,3,03FFFFFF,0000083F*3C");
    onNavtexFilters(nullptr);
    nfPaint();
}

// Para las pruebas en el PC: vuelve a leer la lista de luces, centra la carta en ella y elige la luz "pick" (-1 = ninguna)
void testLights(int zoom, int pick) {
    loadLights();
    if (lightList.empty()) return;
    closeModal();
    chFollow = false;
    chCenter = {lightList[0].lat, lightList[0].lon};
    chZoom = zoom;
    showTab(1);
    update();
    if (pick >= 0 && pick < (int)lightList.size()) { selectPoint({lightList[(size_t)pick].lat + 0.00002, lightList[(size_t)pick].lon}); update(); }
}

// Para las pruebas en el PC: llamadas LSD de ejemplo (una alerta de socorro abre la ventana) y la lista
void testDsc(bool list) {
    static const char* const lines[] = {
        "$CDDSC,20,2240001110,00,00,26,900072,,,,R,",
        "$CDDSC,16,0022410240,08,18,26,,,,,S,",
        "$CDDSC,12,2241234500,12,05,00,0383000016,1402,,,S,",
        "$CDDSC,16,0022410240,12,12,00,0383000016,1402,2241234500,05,S,",
    };
    for (const char* l : lines) links::injectLine(l);
    if (list) { closeModal(); dscShown.push_back(links::dsc().items().back().key()); for (const plotter::DscCall& c : links::dsc().items()) dscShown.push_back(c.key()); openDscList(); }
}

void closeAstro() { if (astroScr) lv_obj_set_hidden(astroScr, true); if (ctScr) lv_obj_set_hidden(ctScr, true); }
void testAstroList() { if (!astroScr) return; lv_obj_set_hidden(astroScr, false); openAstroList(); }

static void makeAbout() {
    aboutScr = lv_obj_create(lv_layer_top());
    lv_obj_remove_style_all(aboutScr);
    lv_obj_set_size(aboutScr, board::W, board::H);
    lv_obj_set_style_bg_color(aboutScr, lv_color_hex(C_BG), 0);
    lv_obj_set_style_bg_opa(aboutScr, LV_OPA_COVER, 0);
    lv_obj_set_scrollable(aboutScr, false);
    lv_obj_set_clickable(aboutScr, true);
    lv_obj_t* t = label(aboutScr, "Acerca de", &dejavu_bold_20, C_TEXT);
    lv_obj_set_pos(t, 16, 18);
    lv_obj_t* b = textButton(aboutScr, "Cerrar", 124, 48, onAboutClose, 0);
    lv_obj_set_pos(b, board::W - 140, 8);
    lv_obj_t* p = box(aboutScr, C_PANEL, 12);
    lv_obj_set_size(p, board::W - 32, board::H - 80);
    lv_obj_set_pos(p, 16, 68);
    char v[96];
    snprintf(v, sizeof(v), "APRS LoRa+ Plotter %s", PLOTTER_VERSION);
    t = label(p, v, &dejavu_bold_28, C_TEXT);
    lv_obj_set_pos(t, 24, 22);
    snprintf(v, sizeof(v), "Placa %s \xC2\xB7 ESP32-S3", BOARD_ENV);
    t = label(p, v, &dejavu_16, C_SOFT);
    lv_obj_set_pos(t, 24, 66);
    t = label(p, about::kCopyright, &dejavu_bold_20, C_TEXT);
    lv_obj_set_pos(t, 24, 104);
    t = label(p, about::kLicense, &dejavu_16, C_SOFT);
    lv_obj_set_width(t, 650);
    lv_obj_set_pos(t, 24, 140);
    snprintf(v, sizeof(v), "Proyecto e instalador: %s", about::kProject);
    t = label(p, v, &dejavu_16, C_TEXT);
    lv_obj_set_pos(t, 24, 204);
    t = label(p, about::kCredits, &dejavu_14, C_DIM);
    lv_obj_set_width(t, 650);
    lv_obj_set_pos(t, 24, 250);
    t = label(p, about::kChartCredits, &dejavu_14, C_DIM);
    lv_obj_set_width(t, 650);
    lv_obj_set_pos(t, 24, 330);
    lv_obj_t* qw = about::donateQr(p, 200, C_BG);
    lv_obj_update_layout(qw);
    int qx = board::W - 32 - 30 - lv_obj_get_width(qw);
    lv_obj_set_pos(qw, qx, 30);
    int qc = qx + lv_obj_get_width(qw) / 2, ty = 30 + lv_obj_get_height(qw) + 14;   // textos centrados bajo el QR
    t = label(p, about::kDonateText, &dejavu_bold_16, C_ACC);
    lv_obj_set_width(t, 300);
    lv_obj_set_style_text_align(t, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(t, qc - 150, ty);
    t = label(p, about::kScan, &dejavu_14, C_DIM);
    lv_obj_set_width(t, 300);
    lv_obj_set_style_text_align(t, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(t, qc - 150, ty + 26);
    lv_obj_set_hidden(aboutScr, true);
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
    netField(2, 232, 126, 420, "Servidor (vac\xC3\xADo: el tel\xC3\xA9" "fono que da la WiFi)", 39, false);
    netField(3, 668, 126, 140, "Puerto", 5, true);
    netField(4, 824, 126, 184, "Puerto UDP", 5, true);
    netField(5, 16, 184, 330, "Clave de OpenWeatherMap (tiempo de internet)", 39, false);
    netField(7, 358, 184, 330, "Avisos: zonas (vac\xC3\xADo = todas)", 63, false);
    netField(8, 700, 184, 120, "Pa\xC3\xADs ISO", 2, false);
    netField(6, 832, 184, 176, "PIN del tracker", 6, true);
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

// ---------- Embarcacion: datos propios, sincronizados con el transpondedor APRS LoRa+ MAPRS ----------
static void onVesField(lv_event_t* e) { lv_keyboard_set_textarea(vesKb, (lv_obj_t*)lv_event_get_target(e)); }

static void openVessel() {
    const settings::Values& v = settings::get();
    char b[16];
    lv_textarea_set_text(vesTa[0], v.vesselName);
    char id[12];
    size_t k = 0;
    for (; v.licIds[k] && v.licIds[k] != ',' && k + 1 < sizeof(id); k++) id[k] = v.licIds[k];
    id[k] = 0;
    lv_textarea_set_text(vesTa[6], id);
    snprintf(b, sizeof(b), "%09lu", (unsigned long)v.vesselMmsi);
    lv_textarea_set_text(vesTa[1], v.vesselMmsi ? b : "");
    lv_textarea_set_text(vesTa[2], v.vesselCall);
    lv_textarea_set_text(vesTa[7], v.vesselMCall);
    lv_textarea_set_text(vesTa[8], v.mtKey);
    snprintf(b, sizeof(b), "%d", v.vesselLen);
    lv_textarea_set_text(vesTa[3], v.vesselLen ? b : "");
    snprintf(b, sizeof(b), "%d", v.vesselBeam);
    lv_textarea_set_text(vesTa[4], v.vesselBeam ? b : "");
    snprintf(b, sizeof(b), "%d,%d", v.vesselDraftDm / 10, v.vesselDraftDm % 10);
    lv_textarea_set_text(vesTa[5], v.vesselDraftDm ? b : "");
    vesType = v.vesselType;
    lv_keyboard_set_textarea(vesKb, vesTa[0]);
    lv_obj_set_hidden(vesScr, false);
}

void testVessel() { openVessel(); }

static void onVesBtn(lv_event_t* e) {
    int a = (int)(intptr_t)lv_event_get_user_data(e);
    if (a == 0) {                                         // tipo; con un transpondedor de clase B solo los de clase B
        const bool onlyB = vessel_link::sync().classBOnly();
        do vesType = (vesType + 1) % plotter::vessel::typeCount();
        while (onlyB && !plotter::vessel::classB(vesType));
    } else if (a == 1 || a == 3 || a == 4 || a == 5) {    // 1 guardar y mandar; 3 validar el CALLSIGN; 4 MarineTraffic; 5 MARS
        settings::Values& v = settings::get();
        snprintf(v.vesselName, sizeof(v.vesselName), "%s", lv_textarea_get_text(vesTa[0]));
        for (char* c = v.vesselName; *c; c++) *c = (char)toupper((unsigned char)*c);
        v.vesselMmsi = (uint32_t)strtoul(lv_textarea_get_text(vesTa[1]), nullptr, 10);
        if (!plotter::vessel::validMmsi(v.vesselMmsi)) v.vesselMmsi = 0;
        snprintf(v.vesselCall, sizeof(v.vesselCall), "%s", lv_textarea_get_text(vesTa[2]));
        for (char* c = v.vesselCall; *c; c++) { *c = (char)toupper((unsigned char)*c); if (*c == '-') { *c = 0; break; } }
        size_t mk = 0;                                    // distintivo maritimo: A-Z 0-9, maximo 7
        for (const char* c = lv_textarea_get_text(vesTa[7]); *c && mk + 1 < sizeof(v.vesselMCall); c++) {
            const char u = (char)toupper((unsigned char)*c);
            if ((u >= 'A' && u <= 'Z') || (u >= '0' && u <= '9')) v.vesselMCall[mk++] = u;
        }
        v.vesselMCall[mk] = 0;
        auto clampi = [](int x, int lo, int hi) { return x < lo ? lo : (x > hi ? hi : x); };
        v.vesselLen = clampi(atoi(lv_textarea_get_text(vesTa[3])), 0, 999);
        v.vesselBeam = clampi(atoi(lv_textarea_get_text(vesTa[4])), 0, 126);
        char d[12];
        snprintf(d, sizeof(d), "%s", lv_textarea_get_text(vesTa[5]));
        for (char* c = d; *c; c++) if (*c == ',') *c = '.';
        v.vesselDraftDm = clampi((int)lround(atof(d) * 10.0), 0, 255);
        v.vesselType = vesType;
        settings::save();
        snprintf(v.mtKey, sizeof(v.mtKey), "%s", lv_textarea_get_text(vesTa[8]));
        if (strcmp(v.vesselMcPair, "") != 0) {            // otro MMSI u otro distintivo: deja de estar comprobado
            char k[24];
            plotter::mtcheck::pairKey(v.vesselMmsi, v.vesselMCall, k, sizeof(k));
            if (strcmp(k, v.vesselMcPair) != 0) { v.vesselMcOk = 0; v.vesselMcPair[0] = 0; }
        }
        settings::save();
        if (a == 3) licence::validate((uint32_t)strtoul(lv_textarea_get_text(vesTa[6]), nullptr, 10));
        else if (a == 4) maritime::checkMarineTraffic();
        else if (a == 5) maritime::confirmMars(v.vesselMcOk != plotter::mtcheck::MARS);
        else vessel_link::changed();
        openVessel();                                     // con los valores ya corregidos
    } else lv_obj_set_hidden(vesScr, true);
}

static lv_obj_t* vesField(int idx, int x, int y, int w, const char* hint, int maxLen, const char* chars) {
    lv_obj_t* ta = lv_textarea_create(vesScr);
    lv_obj_remove_style_all(ta);
    lv_obj_set_pos(ta, x, y);
    lv_obj_set_size(ta, w, 48);
    lv_textarea_set_one_line(ta, true);
    lv_textarea_set_max_length(ta, (uint32_t)maxLen);
    lv_textarea_set_placeholder_text(ta, hint);
    if (chars) lv_textarea_set_accepted_chars(ta, chars);
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
    lv_obj_add_event_cb(ta, onVesField, LV_EVENT_FOCUSED, nullptr);
    lv_obj_add_event_cb(ta, onVesField, LV_EVENT_CLICKED, nullptr);
    vesTa[idx] = ta;
    return ta;
}

static void makeVessel() {
    vesScr = lv_obj_create(lv_layer_top());
    lv_obj_remove_style_all(vesScr);
    lv_obj_set_size(vesScr, board::W, board::H);
    lv_obj_set_style_bg_color(vesScr, lv_color_hex(C_BG), 0);
    lv_obj_set_style_bg_opa(vesScr, LV_OPA_COVER, 0);
    lv_obj_set_scrollable(vesScr, false);
    lv_obj_set_clickable(vesScr, true);
    lv_obj_t* t = label(vesScr, "Embarcaci\xC3\xB3n", &dejavu_bold_20, C_TEXT);
    lv_obj_set_pos(t, 16, 18);
    vesStatus = label(vesScr, "", &dejavu_14, C_SOFT);
    lv_obj_set_pos(vesStatus, 170, 12);
    lv_obj_set_width(vesStatus, 460);
    lv_obj_t* b = textButton(vesScr, "Guardar y enviar", 220, 48, onVesBtn, 1);
    lv_obj_set_pos(b, 648, 8);
    b = textButton(vesScr, "Cerrar", 124, 48, onVesBtn, 2);
    lv_obj_set_pos(b, 884, 8);
    vesField(0, 16, 68, 300, "Nombre (20 letras)", 20, nullptr);
    vesField(1, 328, 68, 200, "MMSI (9 cifras)", 9, "0123456789");
    vesField(7, 540, 68, 228, "Distintivo mar\xC3\xADtimo", 7, nullptr);
    vesField(2, 780, 68, 228, "CALLSIGN", 6, nullptr);
    b = textButton(vesScr, "", 420, 48, onVesBtn, 0);
    lv_obj_set_pos(b, 16, 126);
    vesTypeLbl = lv_obj_get_child(b, 0);
    vesField(3, 452, 126, 180, "Eslora, m", 3, "0123456789");
    vesField(4, 640, 126, 180, "Manga, m", 3, "0123456789");
    vesField(5, 828, 126, 180, "Calado, m", 4, "0123456789.,");
    vesField(6, 16, 184, 230, "DMR ID / CCS7", 7, "0123456789");
    b = textButton(vesScr, "Validar", 180, 48, onVesBtn, 3);
    lv_obj_set_pos(b, 262, 184);
    vesLic = label(vesScr, "", &dejavu_14, C_SOFT);
    lv_obj_set_pos(vesLic, 458, 186);
    lv_obj_set_width(vesLic, 550);
    vesField(8, 16, 242, 300, "Clave API MarineTraffic", 40, "0123456789abcdefABCDEF");
    b = textButton(vesScr, "MarineTraffic", 180, 48, onVesBtn, 4);
    lv_obj_set_pos(b, 328, 242);
    vesMtLbl = lv_obj_get_child(b, 0);
    b = textButton(vesScr, "MARS UIT", 140, 48, onVesBtn, 5);
    lv_obj_set_pos(b, 520, 242);
    vesMarsLbl = lv_obj_get_child(b, 0);
    vesMar = label(vesScr, "", &dejavu_14, C_SOFT);
    lv_obj_set_pos(vesMar, 672, 244);
    lv_obj_set_width(vesMar, 336);
    vesKb = lv_keyboard_create(vesScr);
    lv_obj_set_size(vesKb, board::W, 300);
    lv_obj_align(vesKb, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_color(vesKb, lv_color_hex(C_BG), 0);
    lv_obj_set_style_bg_opa(vesKb, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(vesKb, 6, 0);
    lv_obj_set_style_pad_gap(vesKb, 6, 0);
    lv_obj_set_style_border_width(vesKb, 0, 0);
    lv_obj_set_style_bg_color(vesKb, lv_color_hex(C_PANEL), LV_PART_ITEMS);
    lv_obj_set_style_bg_opa(vesKb, LV_OPA_COVER, LV_PART_ITEMS);
    lv_obj_set_style_bg_color(vesKb, lv_color_hex(C_LINE), LV_PART_ITEMS | LV_STATE_CHECKED);
    lv_obj_set_style_bg_color(vesKb, lv_color_hex(C_ACC), LV_PART_ITEMS | LV_STATE_PRESSED);
    lv_obj_set_style_text_color(vesKb, lv_color_hex(C_TEXT), LV_PART_ITEMS);
    lv_obj_set_style_text_color(vesKb, lv_color_hex(C_TEXT), LV_PART_ITEMS | LV_STATE_CHECKED);
    lv_obj_set_style_radius(vesKb, 8, LV_PART_ITEMS);
    lv_obj_set_style_border_width(vesKb, 0, LV_PART_ITEMS);
    lv_obj_set_style_shadow_width(vesKb, 0, LV_PART_ITEMS);
    lv_obj_set_hidden(vesScr, true);
}

static const char* vesselStateText() {
    switch (vessel_link::sync().state()) {
        case plotter::vessel::WAITING:    return "Transponder: leyendo sus datos...";
        case plotter::vessel::SYNCED:     return "Transponder: mismos datos que el plotter";
        case plotter::vessel::SENT:       return "Transponder: enviando los datos...";
        case plotter::vessel::ADJUSTED:   return "Transponder: los ha ajustado a lo que admite AIS";
        case plotter::vessel::NO_PROFILE: return "El tracker conectado no tiene perfil n\xC3\xA1utico AIS";
        case plotter::vessel::NOT_LICENSED: return "Transponder: sin CALLSIGN validado no se le mandan los datos";
        default:                          return "Transponder sin conectar (Ajustes > Bluetooth): se enviar\xC3\xA1n al conectar";
    }
}

static void updateVessel() {
    if (!vesScr || lv_obj_is_hidden(vesScr)) return;
    char b[160];
    const bool onlyB = vessel_link::sync().classBOnly();
    snprintf(b, sizeof(b), "%s%s", vesselStateText(), onlyB ? "\nTransponder de clase B: velero de recreo (B CS) o lancha de recreo (B SO)" : "");
    lv_label_set_text(vesStatus, b);
    const char cls = plotter::vessel::typeClass(vesType);
    snprintf(b, sizeof(b), "%s \xC2\xB7 %s", plotter::vessel::typeName(vesType), cls == 'A' ? "clase A" : (cls == 'S' ? "clase B SO" : "clase B CS"));
    lv_label_set_text(vesTypeLbl, b);
    char l[200];
    licence::text(l, sizeof(l));
    lv_label_set_text(vesLic, l);
    lv_obj_set_style_text_color(vesLic, lv_color_hex(licence::valid() ? 0x3CE68Au : C_WARN), 0);
    maritime::text(l, sizeof(l));
    lv_label_set_text(vesMar, l);
    lv_obj_set_style_text_color(vesMar, lv_color_hex(maritime::verified() ? 0x3CE68Au : C_WARN), 0);
    lv_label_set_text(vesMarsLbl, settings::get().vesselMcOk == plotter::mtcheck::MARS && maritime::verified() ? "MARS: OK" : "MARS UIT");
    lv_label_set_text(vesMtLbl, settings::get().vesselMcOk == plotter::mtcheck::MARINETRAFFIC && maritime::verified() ? "MarineTraffic: OK" : "MarineTraffic");
}

static void updateNet() {
    if (!netScr || lv_obj_is_hidden(netScr)) return;
    char b[220];
    netSummary(b, sizeof(b), true);
    lv_label_set_text(netStatus, b);
    lv_label_set_text(netKindLbl, kTcpKinds[netKind]);
}

// ---------- Pestana Viento ----------
static const char* sourceName(Source s);
static const int WD = 476, WD_R = 196;                    // esfera: proa arriba
static const int kWindHist = 60;                          // 10 min, una muestra cada 10 s
static lv_obj_t* wLineA = nullptr;
static lv_obj_t* wLineT = nullptr;
static lv_point_precise_t wPtsA[2], wPtsT[2];
static lv_obj_t* wMarkA = nullptr;
static lv_obj_t* wMarkT = nullptr;
static lv_obj_t* wCenter = nullptr;
static lv_obj_t* wCenterSub = nullptr;
struct WTile { lv_obj_t* value = nullptr; lv_obj_t* sub = nullptr; lv_obj_t* unit = nullptr; };
static WTile wTile[4];
static lv_obj_t* wChart = nullptr;
static lv_chart_series_t* wSer = nullptr;
static lv_obj_t* wChartLbl = nullptr;
static float windHist[kWindHist];
static int windHistN = 0;
static uint32_t windHistMs = 0;

// Viento de ahora. El real sale del aparente con la velocidad y el rumbo; si no se puede, del que llegue ya calculado.
struct WindNow { bool app = false, tru = false, truAngle = false, truDir = false, computed = false; double awa = 0, aws = 0, twa = 0, tws = 0, twd = 0; };
static WindNow windNow() {
    WindNow w;
    uint32_t now = millis();
    double hdg = 0, spd = 0;
    w.app = store->get(Key::Awa, now, w.awa) && store->get(Key::Aws, now, w.aws);
    bool haveSpd = store->get(Key::Stw, now, spd) || store->get(Key::Sog, now, spd);
    bool haveHdg = store->get(Key::Heading, now, hdg);
    if (w.app && haveSpd && haveHdg) {
        trueWind(w.awa, w.aws, spd, hdg, w.twa, w.tws, w.twd);
        w.tru = w.truAngle = w.truDir = w.computed = true;
    } else if (store->get(Key::Tws, now, w.tws)) {
        w.tru = true;
        if (store->get(Key::Twd, now, w.twd)) {
            w.truDir = true;
            if (haveHdg) { w.twa = norm180(w.twd - hdg); w.truAngle = true; }
        }
    }
    return w;
}

static void sampleWind() {
    uint32_t now = millis();
    if (windHistN && now - windHistMs < 10000) return;
    WindNow w = windNow();
    if (!w.tru && !w.app) return;
    windHistMs = now;
    if (windHistN == kWindHist) { memmove(windHist, windHist + 1, sizeof(float) * (kWindHist - 1)); windHistN--; }
    windHist[windHistN++] = (float)(w.tru ? w.tws : w.aws);
}

static const char* cardinalEs(double deg) {
    static const char* const k[16] = {"N", "NNE", "NE", "ENE", "E", "ESE", "SE", "SSE", "S", "SSO", "SO", "OSO", "O", "ONO", "NO", "NNO"};
    return k[(int)lround(norm360(deg) / 22.5) % 16];
}

static lv_obj_t* windArc(lv_obj_t* parent, int from, int to, uint32_t color) {      // sector de cenida, grados desde la proa
    lv_obj_t* a = lv_arc_create(parent);
    lv_obj_remove_style_all(a);
    lv_obj_set_size(a, 2 * WD_R, 2 * WD_R);
    lv_obj_center(a);
    lv_arc_set_bg_angles(a, (lv_value_precise_t)((from + 270) % 360), (lv_value_precise_t)((to + 270) % 360));
    lv_obj_set_style_arc_color(a, lv_color_hex(color), 0);
    lv_obj_set_style_arc_width(a, 12, 0);
    lv_obj_set_style_arc_opa(a, LV_OPA_COVER, 0);
    lv_obj_set_clickable(a, false);
    return a;
}

static void makeWind(lv_obj_t* page) {
    lv_obj_t* d = box(page, C_PANEL, 10);
    lv_obj_set_size(d, WD, WD);
    lv_obj_set_pos(d, 0, 0);
    lv_obj_t* ring = lv_obj_create(d);
    lv_obj_remove_style_all(ring);
    lv_obj_set_size(ring, 2 * WD_R, 2 * WD_R);
    lv_obj_center(ring);
    lv_obj_set_style_radius(ring, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_color(ring, lv_color_hex(0x2F5F88), 0);
    lv_obj_set_style_border_width(ring, 2, 0);
    lv_obj_set_clickable(ring, false);
    windArc(d, 300, 340, C_RED);                          // cenida por babor
    windArc(d, 20, 60, 0x2ECC71);                         // y por estribor
    for (int a = 0; a < 360; a += 30) {                   // marcas cada 30 grados: 0 a 180 por cada banda
        double r = deg2rad((double)a);
        int port = a > 180 ? 360 - a : a;
        char t[8];
        snprintf(t, sizeof(t), "%d", port);
        lv_obj_t* l = label(d, a == 0 ? "PROA" : t, &dejavu_12, C_DIM);
        lv_obj_align(l, LV_ALIGN_CENTER, (int)lround(sin(r) * (WD_R + 20)), (int)lround(-cos(r) * (WD_R + 20)));
    }
    lv_obj_t* hull = box(d, C_BAR, 30);                   // el barco, visto desde arriba
    lv_obj_set_size(hull, 60, 150);
    lv_obj_center(hull);
    lv_obj_set_style_border_color(hull, lv_color_hex(C_LINE), 0);
    lv_obj_set_style_border_width(hull, 2, 0);
    wLineT = lv_line_create(d);
    lv_obj_set_style_line_color(wLineT, lv_color_hex(C_ACC), 0);
    lv_obj_set_style_line_width(wLineT, 5, 0);
    lv_obj_set_style_line_rounded(wLineT, true, 0);
    wLineA = lv_line_create(d);
    lv_obj_set_style_line_color(wLineA, lv_color_hex(C_WARN), 0);
    lv_obj_set_style_line_width(wLineA, 7, 0);
    lv_obj_set_style_line_rounded(wLineA, true, 0);
    wMarkT = label(d, "R", &dejavu_bold_16, C_ACC);
    wMarkA = label(d, "A", &dejavu_bold_16, C_WARN);
    wCenter = label(d, "--", &dejavu_num_44, C_TEXT);
    lv_obj_align(wCenter, LV_ALIGN_CENTER, 0, -8);
    wCenterSub = label(d, "", &dejavu_12, C_DIM);
    lv_obj_align(wCenterSub, LV_ALIGN_CENTER, 0, 30);
    lv_obj_t* leg = label(d, "A aparente \xC2\xB7 R real", &dejavu_12, C_DIM);
    lv_obj_align(leg, LV_ALIGN_BOTTOM_LEFT, 10, -8);

    static const char* const names[4] = {"VIENTO APARENTE", "VIENTO REAL", "DIRECCI\xC3\x93N REAL", "\xC3\x81NGULO REAL"};
    for (int i = 0; i < 4; i++) {
        lv_obj_t* t = box(page, C_PANEL, 10);
        lv_obj_set_size(t, 254, 150);
        lv_obj_set_pos(t, 486 + (i % 2) * 264, (i / 2) * 160);
        lv_obj_set_style_pad_all(t, 12, 0);
        lv_obj_t* n = label(t, names[i], &dejavu_bold_16, C_DIM);
        lv_obj_align(n, LV_ALIGN_TOP_LEFT, 0, 0);
        wTile[i].unit = label(t, "", &dejavu_14, C_DIM);
        lv_obj_align(wTile[i].unit, LV_ALIGN_TOP_RIGHT, 0, 2);
        wTile[i].value = label(t, "--", &dejavu_num_44, i == 0 ? C_WARN : (i == 1 ? C_ACC : C_TEXT));
        lv_obj_align(wTile[i].value, LV_ALIGN_LEFT_MID, 0, 6);
        wTile[i].sub = label(t, "", &dejavu_12, C_DIM);
        lv_obj_align(wTile[i].sub, LV_ALIGN_BOTTOM_LEFT, 0, 0);
    }
    lv_obj_t* h = box(page, C_PANEL, 10);
    lv_obj_set_size(h, 518, 156);
    lv_obj_set_pos(h, 486, 320);
    wChartLbl = label(h, "", &dejavu_12, C_DIM);
    lv_obj_align(wChartLbl, LV_ALIGN_TOP_LEFT, 12, 8);
    wChart = lv_chart_create(h);
    lv_obj_remove_style_all(wChart);
    lv_obj_set_size(wChart, 494, 112);
    lv_obj_align(wChart, LV_ALIGN_BOTTOM_MID, 0, -8);
    lv_chart_set_type(wChart, LV_CHART_TYPE_LINE);
    lv_chart_set_point_count(wChart, kWindHist);
    lv_chart_set_div_line_count(wChart, 3, 0);
    lv_obj_set_style_line_color(wChart, lv_color_hex(C_LINE), LV_PART_MAIN);
    lv_obj_set_style_line_width(wChart, 1, LV_PART_MAIN);
    lv_obj_set_style_line_width(wChart, 3, LV_PART_ITEMS);
    lv_obj_set_style_size(wChart, 0, 0, LV_PART_INDICATOR);
    lv_obj_set_clickable(wChart, false);
    wSer = lv_chart_add_series(wChart, lv_color_hex(C_ACC), LV_CHART_AXIS_PRIMARY_Y);
}

static void windNeedle(lv_obj_t* line, lv_point_precise_t* pts, lv_obj_t* mark, bool has, double angle, int inner) {
    lv_obj_set_hidden(line, !has);
    lv_obj_set_hidden(mark, !has);
    if (!has) return;
    double r = deg2rad(angle);
    double sx = sin(r), cy = -cos(r);
    int c = WD / 2;
    pts[0] = {(lv_value_precise_t)(c + sx * (WD_R - 4)), (lv_value_precise_t)(c + cy * (WD_R - 4))};   // viene de fuera
    pts[1] = {(lv_value_precise_t)(c + sx * inner), (lv_value_precise_t)(c + cy * inner)};
    lv_line_set_points(line, pts, 2);
    lv_obj_align(mark, LV_ALIGN_CENTER, (int)lround(sx * (WD_R - 34) + cy * 16), (int)lround(cy * (WD_R - 34) - sx * 16));
}

static void updateWind() {
    WindNow w = windNow();
    uint32_t now = millis();
    char b[48], s[128];
    const char* wu = plotter::label(U.windSpeed);
    windNeedle(wLineA, wPtsA, wMarkA, w.app, w.awa, 96);
    windNeedle(wLineT, wPtsT, wMarkT, w.truAngle, w.twa, 96);
    // Centro y casilla del aparente
    if (w.app) {
        snprintf(b, sizeof(b), "%.1f", speedFromKn(w.aws, U.windSpeed)); comma(b);
        lv_label_set_text(wCenter, b);
        snprintf(s, sizeof(s), "%s aparente", wu);
        lv_label_set_text(wCenterSub, s);
        lv_label_set_text(wTile[0].value, b);
        snprintf(s, sizeof(s), "%d\xC2\xB0 por %s \xC2\xB7 %s", (int)lround(fabs(w.awa)), w.awa < 0 ? "babor" : "estribor", sourceName(store->source(Key::Aws, now)));
        lv_label_set_text(wTile[0].sub, s);
    } else {
        lv_label_set_text(wCenter, "--");
        lv_label_set_text(wCenterSub, "sin viento aparente");
        lv_label_set_text(wTile[0].value, "--");
        lv_label_set_text(wTile[0].sub, "Sin datos");
    }
    lv_label_set_text(wTile[0].unit, wu);
    lv_label_set_text(wTile[1].unit, wu);
    lv_label_set_text(wTile[2].unit, "\xC2\xB0T");
    lv_label_set_text(wTile[3].unit, "\xC2\xB0");
    if (w.tru) {
        int f = beaufort(w.tws);
        snprintf(b, sizeof(b), "%.1f", speedFromKn(w.tws, U.windSpeed)); comma(b);
        lv_label_set_text(wTile[1].value, b);
        bool inet = !w.computed && store->source(Key::Tws, now) == Source::Internet;
        snprintf(s, sizeof(s), "Fuerza %d \xC2\xB7 %s%s", f, beaufortNameEs(f), w.computed ? " \xC2\xB7 calculado" : (inet ? " \xC2\xB7 internet, estimado" : ""));
        lv_label_set_text(wTile[1].sub, s);
        lv_obj_set_style_text_color(wTile[1].value, lv_color_hex(inet ? C_WARN : C_ACC), 0);
    } else {
        lv_label_set_text(wTile[1].value, "--");
        lv_label_set_text(wTile[1].sub, "Falta viento, velocidad o rumbo");
    }
    if (w.truDir) {
        snprintf(b, sizeof(b), "%03d\xC2\xB0", (int)lround(norm360(w.twd)) % 360);
        lv_label_set_text(wTile[2].value, b);
        snprintf(s, sizeof(s), "Viene del %s", cardinalEs(w.twd));
        lv_label_set_text(wTile[2].sub, s);
    } else {
        lv_label_set_text(wTile[2].value, "--");
        lv_label_set_text(wTile[2].sub, "Sin datos");
    }
    if (w.truAngle) {
        double a = fabs(w.twa);
        snprintf(b, sizeof(b), "%d\xC2\xB0", (int)lround(a));
        lv_label_set_text(wTile[3].value, b);
        const char* point = a < 30 ? "Proa al viento" : (a < 60 ? "Ce\xC3\xB1ida" : (a < 100 ? "Trav\xC3\xA9s" : (a < 150 ? "Largo" : "Empopada")));
        snprintf(s, sizeof(s), "%s \xC2\xB7 por %s", point, w.twa < 0 ? "babor" : "estribor");
        lv_label_set_text(wTile[3].sub, s);
    } else {
        lv_label_set_text(wTile[3].value, "--");
        lv_label_set_text(wTile[3].sub, "Sin datos");
    }
    // Historial de 10 minutos
    float mx = 0, mn = 1e9f;
    for (int i = 0; i < kWindHist; i++) {
        int k = i - (kWindHist - windHistN);              // lo mas nuevo, a la derecha
        if (k < 0) { lv_chart_set_series_value_by_id(wChart, wSer, (uint32_t)i, LV_CHART_POINT_NONE); continue; }
        float v = (float)speedFromKn(windHist[k], U.windSpeed);
        if (v > mx) mx = v;
        if (v < mn) mn = v;
        lv_chart_set_series_value_by_id(wChart, wSer, (uint32_t)i, (int32_t)lround(v * 10));
    }
    int top = ((int)(mx / 5) + 1) * 5;                    // escala en pasos de 5
    lv_chart_set_axis_range(wChart, LV_CHART_AXIS_PRIMARY_Y, 0, top * 10);
    lv_chart_refresh(wChart);
    if (windHistN) {
        char a1[16], a2[16];
        snprintf(a1, sizeof(a1), "%.1f", mn); comma(a1);
        snprintf(a2, sizeof(a2), "%.1f", mx); comma(a2);
        snprintf(s, sizeof(s), "\xC3\x9Altimos 10 min \xC2\xB7 m\xC3\xADn %s \xC2\xB7 m\xC3\xA1x %s %s \xC2\xB7 escala 0 a %d", a1, a2, wu, top);
    } else snprintf(s, sizeof(s), "\xC3\x9Altimos 10 min \xC2\xB7 sin datos");
    lv_label_set_text(wChartLbl, s);
}

// ---------- Pestana Piloto ----------
// El plotter solo da al piloto los datos de la ruta (APB y RMB); embragar y mandar se hace desde el mando del piloto.
static lv_obj_t* pHts = nullptr;
static lv_obj_t* pHtsSub = nullptr;
static lv_obj_t* pHdg = nullptr;
static lv_obj_t* pHdgSub = nullptr;
static lv_obj_t* pXteBoat = nullptr;
static lv_obj_t* pXteTxt = nullptr;
static lv_obj_t* pXteScale = nullptr;
static lv_obj_t* pRudBar = nullptr;
static lv_obj_t* pRudTxt = nullptr;
static lv_obj_t* pDest = nullptr;
static lv_obj_t* pInfo = nullptr;
static lv_obj_t* pOutLbl = nullptr;
static lv_obj_t* pOutTxt = nullptr;
static const int P_BAR_W = 560;
static uint32_t pilotLastMs = 0;
static bool pilotOk = false;

static lv_obj_t* pModelLbl = nullptr;
static bool pilotWind = false;                             // se esta mandando el viento (VWR) al ST2000+

static void onPilotOut(lv_event_t* e) {
    settings::Values& v = settings::get();
    if ((intptr_t)lv_event_get_user_data(e) == 1) v.pilotModel = (v.pilotModel + 1) % 2;
    else v.pilotOut = (v.pilotOut + 1) % 5;
    settings::save();
    links::pilotN2kEnable(v.pilotOut == 4);               // reclama/suelta la direccion en el bus
}

// Una vez por segundo por la via elegida: con ruta activa, APB y RMB; con el ST2000+, ademas el viento aparente (VWR, para su
// modo WindTrim, aunque no haya ruta) y la velocidad en el agua (VHW). El ST2000+ solo lee 4 letras del nombre del waypoint:
// se manda "W001", "W002"... (con "WPT 01", "WPT 02" no veria el cambio de waypoint).
static void pilotSend() {
    const settings::Values& v = settings::get();
    uint32_t now = millis();
    if (now - pilotLastMs < 1000) return;
    pilotLastMs = now;
    pilotOk = false;
    pilotWind = false;
    if (!v.pilotOut) return;
    if (v.pilotOut == 4) links::pilotN2kEnable(true);        // reclama la direccion (idempotente) por si el CAN se encendio despues
    const bool st2000 = v.pilotModel == 1;
    char b[128];
    bool ok = true, any = false;
    if (navOn) {
        const RouteStatus& st = nav.status();
        char destId[8], origId[8];
        const char* dest = navCount > 1 ? navNames[st.target] : navName;
        const char* orig = (navCount > 1 && st.target > 0) ? navNames[st.target - 1] : "ORIGEN";
        if (st2000) {
            pilotWaypointId(navCount > 1 ? st.target + 1 : 1, destId, sizeof(destId));
            pilotWaypointId(navCount > 1 ? st.target : 0, origId, sizeof(origId));
            dest = destId;
            orig = origId;
        }
        bool arrived = navCount > 1 ? st.finished : st.inArrival;
        double sog = 0, cog = 0, closing = 0;
        if (store->get(Key::Sog, now, sog) && store->get(Key::Cog, now, cog)) closing = sog * cos(deg2rad(norm180(st.bearing - cog)));
        if (v.pilotOut == 4) {                                     // piloto por NMEA 2000: 129283 (XTE) + 129284 (navegacion)
            ok = links::sendPilotN2k(st.xteNm, st.bearing, st.legBearing, st.distanceNm, navPts[st.target].lat, navPts[st.target].lon,
                                     navCount > 1 && st.target > 0 ? (uint32_t)st.target : 0xFFFFFFFF,
                                     navCount > 1 ? (uint32_t)(st.target + 1) : 1, closing, arrived, false);
        } else {
            int n = nmeaRmb(b, sizeof(b), st.xteNm, orig, dest, navPts[st.target].lat, navPts[st.target].lon, st.distanceNm, st.bearing, closing, arrived);
            ok = links::sendPilot(b, n, v.pilotOut);
            n = nmeaApb(b, sizeof(b), st.xteNm, arrived, st.abeam, st.legBearing, dest, st.bearing, st.bearing, false);
            ok = links::sendPilot(b, n, v.pilotOut) && ok;
        }
        any = true;
    }
    if (st2000) {
        double awa, aws, stw;
        if (store->get(Key::Awa, now, awa) && store->get(Key::Aws, now, aws)) {
            int n = nmeaVwr(b, sizeof(b), awa, aws);
            pilotWind = links::sendPilot(b, n, v.pilotOut);
            ok = pilotWind && ok;
            any = true;
        }
        if (store->get(Key::Stw, now, stw)) {
            char body[64];
            std::snprintf(body, sizeof(body), "IIVHW,,T,,M,%.1f,N,%.1f,K", stw, stw * 1.852);
            const int n = std::snprintf(b, sizeof(b), "$%s*%02X\r\n", body, nmeaChecksum(body));
            links::sendPilot(b, n, v.pilotOut);
        }
    }
    pilotOk = any && ok;
}

static lv_obj_t* pilotBig(lv_obj_t* parent, int x, const char* name, lv_obj_t** value, lv_obj_t** sub, uint32_t color) {
    lv_obj_t* t = box(parent, C_PANEL, 10);
    lv_obj_set_size(t, 298, 190);
    lv_obj_set_pos(t, x, 0);
    lv_obj_set_style_pad_all(t, 12, 0);
    lv_obj_t* n = label(t, name, &dejavu_bold_16, C_DIM);
    lv_obj_align(n, LV_ALIGN_TOP_LEFT, 0, 0);
    *value = label(t, "--", &dejavu_num_60, color);
    lv_obj_align(*value, LV_ALIGN_LEFT_MID, 0, 6);
    *sub = label(t, "", &dejavu_12, C_DIM);
    lv_obj_align(*sub, LV_ALIGN_BOTTOM_LEFT, 0, 0);
    return t;
}

static void makePilot(lv_obj_t* page) {
    pilotBig(page, 0, "RUMBO A GOBERNAR", &pHts, &pHtsSub, 0xFF9F43);
    pilotBig(page, 308, "RUMBO DEL BARCO", &pHdg, &pHdgSub, C_TEXT);
    // Desvio de la ruta: la linea del centro es la ruta y el punto, el barco
    lv_obj_t* x = box(page, C_PANEL, 10);
    lv_obj_set_size(x, 606, 150);
    lv_obj_set_pos(x, 0, 200);
    lv_obj_t* t = label(x, "DESV\xC3\x8DO DE LA RUTA", &dejavu_bold_16, C_DIM);
    lv_obj_align(t, LV_ALIGN_TOP_LEFT, 12, 12);
    pXteScale = label(x, "", &dejavu_12, C_DIM);
    lv_obj_align(pXteScale, LV_ALIGN_TOP_RIGHT, -12, 14);
    lv_obj_t* bar = box(x, C_BAR, 6);
    lv_obj_set_size(bar, P_BAR_W, 30);
    lv_obj_align(bar, LV_ALIGN_CENTER, 0, 4);
    lv_obj_t* mid = box(x, 0xFF9F43);
    lv_obj_set_size(mid, 4, 46);
    lv_obj_align(mid, LV_ALIGN_CENTER, 0, 4);
    pXteBoat = box(x, C_TEXT, LV_RADIUS_CIRCLE);
    lv_obj_set_size(pXteBoat, 24, 24);
    lv_obj_set_style_border_color(pXteBoat, lv_color_hex(C_INK), 0);
    lv_obj_set_style_border_width(pXteBoat, 3, 0);
    lv_obj_align(pXteBoat, LV_ALIGN_CENTER, 0, 4);
    pXteTxt = label(x, "", &dejavu_16, C_TEXT);
    lv_obj_align(pXteTxt, LV_ALIGN_BOTTOM_LEFT, 12, -10);
    // Timon
    lv_obj_t* r = box(page, C_PANEL, 10);
    lv_obj_set_size(r, 606, 116);
    lv_obj_set_pos(r, 0, 360);
    t = label(r, "TIM\xC3\x93N", &dejavu_bold_16, C_DIM);
    lv_obj_align(t, LV_ALIGN_TOP_LEFT, 12, 12);
    lv_obj_t* rb = box(r, C_BAR, 6);
    lv_obj_set_size(rb, P_BAR_W, 22);
    lv_obj_align(rb, LV_ALIGN_CENTER, 0, 8);
    lv_obj_t* rm = box(r, C_DIM);
    lv_obj_set_size(rm, 2, 34);
    lv_obj_align(rm, LV_ALIGN_CENTER, 0, 8);
    pRudBar = box(r, C_ACC, 4);
    lv_obj_set_size(pRudBar, 4, 22);
    lv_obj_align(pRudBar, LV_ALIGN_CENTER, 0, 8);
    pRudTxt = label(r, "", &dejavu_14, C_SOFT);
    lv_obj_align(pRudTxt, LV_ALIGN_BOTTOM_LEFT, 12, -8);
    // Derecha: destino y salida al piloto
    lv_obj_t* d = box(page, C_PANEL, 10);
    lv_obj_set_size(d, 388, 476);
    lv_obj_set_pos(d, 616, 0);
    pDest = label(d, "", &dejavu_bold_20, C_TEXT);
    lv_obj_align(pDest, LV_ALIGN_TOP_LEFT, 14, 12);
    lv_obj_set_width(pDest, 360);
    pInfo = label(d, "", &dejavu_16, C_SOFT);
    lv_obj_align(pInfo, LV_ALIGN_TOP_LEFT, 14, 50);
    lv_obj_set_width(pInfo, 360);
    lv_obj_set_style_text_line_space(pInfo, 8, 0);
    t = label(d, "Datos de ruta para el piloto", &dejavu_bold_16, C_DIM);
    lv_obj_align(t, LV_ALIGN_TOP_LEFT, 14, 232);
    lv_obj_t* b = textButton(d, "", 176, 52, onPilotOut, 1);           // modelo del piloto
    lv_obj_align(b, LV_ALIGN_TOP_LEFT, 14, 260);
    pModelLbl = lv_obj_get_child(b, 0);
    b = textButton(d, "", 176, 52, onPilotOut, 0);                     // por donde sale
    lv_obj_align(b, LV_ALIGN_TOP_LEFT, 198, 260);
    pOutLbl = lv_obj_get_child(b, 0);
    pOutTxt = label(d, "", &dejavu_14, C_SOFT);
    lv_obj_align(pOutTxt, LV_ALIGN_TOP_LEFT, 14, 324);
    lv_obj_set_width(pOutTxt, 360);
    lv_obj_set_style_text_line_space(pOutTxt, 5, 0);
}

static void updatePilot() {
    uint32_t now = millis();
    const settings::Values& v = settings::get();
    char b[400], c[24];
    double hdg = 0, cog = 0, sog = 0, rud = 0;
    bool hasHdg = store->get(Key::Heading, now, hdg), hasCog = store->get(Key::Cog, now, cog);
    store->get(Key::Sog, now, sog);
    if (hasHdg || hasCog) {
        snprintf(b, sizeof(b), "%03d\xC2\xB0", (int)lround(norm360(hasHdg ? hdg : cog)) % 360);
        lv_label_set_text(pHdg, b);
        lv_label_set_text(pHdgSub, hasHdg ? "Comp\xC3\xA1s, verdadero" : "Sin comp\xC3\xA1s: rumbo sobre el fondo");
    } else {
        lv_label_set_text(pHdg, "--");
        lv_label_set_text(pHdgSub, "Sin datos");
    }
    if (navOn) {
        const RouteStatus& st = nav.status();
        snprintf(b, sizeof(b), "%03d\xC2\xB0", (int)lround(norm360(st.bearing)) % 360);
        lv_label_set_text(pHts, b);
        if (hasHdg || hasCog) {
            double turn = norm180(st.bearing - (hasHdg ? hdg : cog));
            if (fabs(turn) < 2) snprintf(b, sizeof(b), "En rumbo");
            else snprintf(b, sizeof(b), "Caer %d\xC2\xB0 a %s", (int)lround(fabs(turn)), turn < 0 ? "babor" : "estribor");
        } else snprintf(b, sizeof(b), "Demora verdadera al destino");
        lv_label_set_text(pHtsSub, b);
        // Barra: de -escala a +escala; la escala crece si el desvio no cabe
        double scale = fabs(st.xteNm) > 0.5 ? 2.0 : (fabs(st.xteNm) > 0.1 ? 0.5 : 0.1);
        double f = st.xteNm / scale;
        if (f > 1) f = 1;
        if (f < -1) f = -1;
        lv_obj_align(pXteBoat, LV_ALIGN_CENTER, (int)lround(f * (P_BAR_W / 2 - 12)), 4);
        lv_obj_set_hidden(pXteBoat, false);
        fmtDist(c, sizeof(c), scale, scale < 1 ? 1 : 0);
        snprintf(b, sizeof(b), "\xC2\xB1 %s", c);
        lv_label_set_text(pXteScale, b);
        if (fabs(st.xteNm) < 0.005) snprintf(b, sizeof(b), "En la derrota");
        else { fmtDist(c, sizeof(c), fabs(st.xteNm), 2); snprintf(b, sizeof(b), "%s a %s de la ruta \xC2\xB7 gobernar a %s", c, st.xteNm > 0 ? "estribor" : "babor", st.xteNm > 0 ? "babor" : "estribor"); }
        lv_label_set_text(pXteTxt, b);
        bool arrived = !navMob && (navCount > 1 ? st.finished : st.inArrival);
        if (navCount > 1) snprintf(b, sizeof(b), "%s%s (%d de %d)", arrived ? "LLEGADA \xC2\xB7 " : "A ", navNames[st.target], st.target + 1, navCount);
        else snprintf(b, sizeof(b), "%s%s", arrived ? "LLEGADA \xC2\xB7 " : "A ", navName);
        lv_label_set_text(pDest, b);
        char d[24], t[24] = "--";
        fmtDist(d, sizeof(d), st.distanceNm, 2);
        if (sog > 0.5) formatDuration(t, sizeof(t), st.distanceNm / sog);
        snprintf(b, sizeof(b), "Distancia:  %s\nDemora:  %03d\xC2\xB0\nTiempo:  %s\nRumbo del tramo:  %03d\xC2\xB0", d, (int)lround(norm360(st.bearing)) % 360, t,
                 (int)lround(norm360(st.legBearing)) % 360);
        lv_label_set_text(pInfo, b);
    } else {
        lv_label_set_text(pHts, "--");
        lv_label_set_text(pHtsSub, "Sin destino");
        lv_obj_set_hidden(pXteBoat, true);
        lv_label_set_text(pXteScale, "");
        lv_label_set_text(pXteTxt, "");
        lv_label_set_text(pDest, "Sin destino");
        lv_label_set_text(pInfo, "Elige un punto en la Carta y pulsa \xC2\xAB" "Ir a\xC2\xBB, o navega una ruta desde la pesta\xC3\xB1" "a Rutas.");
    }
    if (store->get(Key::RudderAngle, now, rud)) {
        double f = rud / 35.0;                             // de -35 a +35 grados
        if (f > 1) f = 1;
        if (f < -1) f = -1;
        int w = (int)lround(fabs(f) * (P_BAR_W / 2));
        if (w < 4) w = 4;
        lv_obj_set_width(pRudBar, w);
        lv_obj_align(pRudBar, LV_ALIGN_CENTER, (int)lround(f * (P_BAR_W / 4)), 8);
        lv_obj_set_style_bg_color(pRudBar, lv_color_hex(rud < 0 ? C_RED : 0x2ECC71), 0);
        lv_obj_set_hidden(pRudBar, false);
        snprintf(b, sizeof(b), "%d\xC2\xB0 a %s", (int)lround(fabs(rud)), fabs(rud) < 0.5 ? "la v\xC3\xAD" "a" : (rud < 0 ? "babor" : "estribor"));
        lv_label_set_text(pRudTxt, b);
    } else {
        lv_obj_set_hidden(pRudBar, true);
        lv_label_set_text(pRudTxt, "Sin sensor de tim\xC3\xB3n (RSA o NMEA 2000)");
    }
    static const char* const kOut[5] = {"Apagado", "RS485", "Red (TCP)", "Bluetooth", "NMEA 2000"};
    lv_label_set_text(pOutLbl, kOut[v.pilotOut]);
    lv_label_set_text(pModelLbl, v.pilotModel == 1 ? "ST2000+" : "Gen\xC3\xA9rico");
    const bool st2000 = v.pilotModel == 1;
    const char* state;
    if (!v.pilotOut) state = "No se env\xC3\xAD" "a nada.";
    else if (pilotOk) state = st2000 ? (navOn ? (pilotWind ? "Enviando RMB, APB y viento (VWR) cada segundo." : "Enviando RMB y APB cada segundo.")
                                              : "Enviando el viento (VWR) para el modo WindTrim.")
                                     : "Enviando APB y RMB cada segundo.";
    else if (!navOn && !(st2000 && pilotWind)) state = st2000 ? "Sin destino ni viento aparente: no se env\xC3\xAD" "a nada." : "Sin destino: no se env\xC3\xAD" "a nada.";
    else state = v.pilotOut == 3 ? "Sin pasarela por Bluetooth: no se env\xC3\xAD" "a." : "Sin conexi\xC3\xB3n con el servidor: no se env\xC3\xAD" "a.";
    const char* extra = "";
    if (st2000 && v.pilotOut == 1 && v.serialBaud != 4800) extra = "\nEl ST2000+ escucha a 4800: pon el RS485 a 4800 (entonces no entra el AIS).";
    else if (v.pilotOut == 1) extra = "\nEl RS485 es el mismo puerto por el que entra el AIS.";
    else if (st2000 && v.pilotOut == 3) extra = "\nLa pasarela lo saca a 4800 hacia el piloto (blanco NMEA+, verde NMEA\xE2\x88\x92).";
    snprintf(b, sizeof(b), "%s\nEl plotter solo da los datos. El piloto se embraga y se manda desde su propio mando (Track y WindTrim en el ST2000+).%s",
             state, extra);
    lv_label_set_text(pOutTxt, b);
}

// ---------- Pestana Meteo ----------
// Cada dato dice de donde sale: del sensor de a bordo o, si este falta, de internet (estimado).
static WTile mTile[6];
static lv_obj_t* mNote = nullptr;
static lv_obj_t* mNavtexBtn = nullptr;

static void makeMeteo(lv_obj_t* page) {
    static const char* const names[6] = {"PRESI\xC3\x93N", "TEMPERATURA", "HUMEDAD", "VIENTO REAL", "RACHA", "VISIBILIDAD"};
    for (int i = 0; i < 6; i++) {
        lv_obj_t* t = box(page, C_PANEL, 10);
        lv_obj_set_size(t, 328, 176);
        lv_obj_set_pos(t, (i % 3) * 338, (i / 3) * 186);
        lv_obj_set_style_pad_all(t, 12, 0);
        lv_obj_t* n = label(t, names[i], &dejavu_bold_16, C_DIM);
        lv_obj_align(n, LV_ALIGN_TOP_LEFT, 0, 0);
        mTile[i].unit = label(t, "", &dejavu_14, C_DIM);
        lv_obj_align(mTile[i].unit, LV_ALIGN_TOP_RIGHT, 0, 2);
        mTile[i].value = label(t, "--", &dejavu_num_60, C_TEXT);
        lv_obj_align(mTile[i].value, LV_ALIGN_LEFT_MID, 0, 6);
        mTile[i].sub = label(t, "", &dejavu_12, C_DIM);
        lv_obj_align(mTile[i].sub, LV_ALIGN_BOTTOM_LEFT, 0, 0);
    }
    lv_obj_t* nbox = box(page, C_PANEL, 10);
    lv_obj_set_size(nbox, 1004, 104);
    lv_obj_set_pos(nbox, 0, 372);
    mNote = label(nbox, "", &dejavu_14, C_SOFT);
    lv_obj_align(mNote, LV_ALIGN_TOP_LEFT, 14, 12);
    lv_obj_set_width(mNote, 836);
    mNavtexBtn = textButton(nbox, "NAVTEX", 130, 44, onNavtexOpen, 0);
    lv_obj_align(mNavtexBtn, LV_ALIGN_TOP_RIGHT, -10, 4);
    lv_obj_t* dscBtn = textButton(nbox, "LSD", 130, 44, onDscOpen, 0);
    lv_obj_align(dscBtn, LV_ALIGN_BOTTOM_RIGHT, -10, -4);
    lv_obj_set_style_text_line_space(mNote, 6, 0);
}

// Sol y luna del dia en la posicion del barco (horas UTC, como el reloj de Ajustes > Estado)
static void hhmm(char* o, size_t n, double utcMin) {
    int m = (int)lround(utcMin);
    m = ((m % 1440) + 1440) % 1440;
    snprintf(o, n, "%02d:%02d", m / 60, m % 60);
}

static void skyLine(char* out, size_t n) {
    uint32_t now = millis();
    double la, lo;
    const time_t tt = (time_t)tsync::now();
    if (!tt || !store->get(Key::Lat, now, la) || !store->get(Key::Lon, now, lo)) { snprintf(out, n, "Sol y luna: hace falta posici\xC3\xB3n y hora."); return; }
    int y, mo, d; double h;
    plotter::unixToUtc((long long)tt, y, mo, d, h);
    double sr, ss, cr, cs, mr, ms;
    bool hr, hs;
    char a[8], bb[8], c[8], e[8], f[8], g[8];
    char sun[112], moon[80];
    if (plotter::sunTimes(y, mo, d, la, lo, -0.833, sr, ss)) {
        hhmm(a, sizeof a, sr); hhmm(bb, sizeof bb, ss);
        if (plotter::sunTimes(y, mo, d, la, lo, -6, cr, cs)) { hhmm(c, sizeof c, cr); hhmm(e, sizeof e, cs); }
        else { snprintf(c, sizeof c, "--"); snprintf(e, sizeof e, "--"); }
        char n1[8], n2[8];                                   // crepusculo nautico: el de las estrellas con horizonte
        double nr, ns;
        if (plotter::sunTimes(y, mo, d, la, lo, -12, nr, ns)) { hhmm(n1, sizeof n1, nr); hhmm(n2, sizeof n2, ns); }
        else { snprintf(n1, sizeof n1, "--"); snprintf(n2, sizeof n2, "--"); }
        snprintf(sun, sizeof sun, "Sol %s-%s (crep\xC3\xBAsculo civil %s-%s, n\xC3\xA1utico %s-%s)", a, bb, c, e, n1, n2);
    } else snprintf(sun, sizeof sun, "Sol: no sale ni se pone hoy");
    double age = plotter::moonAgeDays(y, mo, d, h);
    plotter::moonTimes(y, mo, d, la, lo, mr, hr, ms, hs);
    if (hr) hhmm(f, sizeof f, mr); else snprintf(f, sizeof f, "--");
    if (hs) hhmm(g, sizeof g, ms); else snprintf(g, sizeof g, "--");
    char ph[24];
    snprintf(ph, sizeof ph, "%s", plotter::moonPhaseNameEs(plotter::moonPhase8(age)));
    ph[0] = (char)tolower((unsigned char)ph[0]);
    snprintf(moon, sizeof moon, "Luna: %s (%d %%), sale %s, se pone %s", ph, (int)lround(plotter::moonIllumination(age) * 100), f, g);
    snprintf(out, n, "%s \xC2\xB7 %s (UTC).", sun, moon);
}

// Fuente, antiguedad y si es estimado; el valor de internet se pinta en ambar
static bool meteoSub(int i, Key k, const char* extra) {
    uint32_t now = millis();
    Source src = store->source(k, now);
    bool est = src == Source::Internet || store->quality(k) == Quality::Estimated;
    uint32_t age = store->ageMs(k, now) / 1000;
    char t[24], s[96];
    if (age < 120) snprintf(t, sizeof(t), "hace %u s", (unsigned)age);
    else snprintf(t, sizeof(t), "hace %u min", (unsigned)(age / 60));
    snprintf(s, sizeof(s), "%s%s%s \xC2\xB7 %s%s", extra, extra[0] ? " \xC2\xB7 " : "", sourceName(src), t, est ? " \xC2\xB7 estimado" : "");
    lv_label_set_text(mTile[i].sub, s);
    lv_obj_set_style_text_color(mTile[i].value, lv_color_hex(est ? C_WARN : C_TEXT), 0);
    return est;
}

static void meteoNone(int i) {
    lv_label_set_text(mTile[i].value, "--");
    lv_label_set_text(mTile[i].sub, "Sin datos");
    lv_obj_set_style_text_color(mTile[i].value, lv_color_hex(C_TEXT), 0);
}

static void updateMeteo() {
    uint32_t now = millis();
    char b[32], x[48];
    double v;
    int est = 0;
    lv_label_set_text(mTile[0].unit, plotter::label(U.press));
    if (store->get(Key::Pressure, now, v)) {
        double p = pressFromHpa(v, U.press);
        snprintf(b, sizeof(b), U.press == PressUnit::InHg ? "%.2f" : "%.0f", p); comma(b);
        lv_label_set_text(mTile[0].value, b);
        est += meteoSub(0, Key::Pressure, "");
    } else meteoNone(0);
    // Temperatura con la sensacion termica y humedad con el punto de rocio (wxcalc del nucleo)
    double airT = NAN, rh = NAN, tws = NAN;
    store->get(Key::AirTemp, now, airT);
    store->get(Key::Humidity, now, rh);
    store->get(Key::Tws, now, tws);
    lv_label_set_text(mTile[1].unit, plotter::label(U.temp));
    if (store->get(Key::AirTemp, now, v)) {
        snprintf(b, sizeof(b), "%.1f", tempFromC(v, U.temp)); comma(b);
        lv_label_set_text(mTile[1].value, b);
        double fl = feelsLikeC(v, std::isnan(rh) ? -1 : rh, std::isnan(tws) ? -1 : tws * 1.852);
        if (std::fabs(fl - v) >= 0.5) { snprintf(x, sizeof(x), "Aire \xC2\xB7 sensaci\xC3\xB3n %.0f%s", tempFromC(fl, U.temp), plotter::label(U.temp)); comma(x); }
        else snprintf(x, sizeof(x), "Aire");
        est += meteoSub(1, Key::AirTemp, x);
    } else meteoNone(1);
    lv_label_set_text(mTile[2].unit, "%");
    if (store->get(Key::Humidity, now, v)) {
        snprintf(b, sizeof(b), "%.0f", v);
        lv_label_set_text(mTile[2].value, b);
        double dp = std::isnan(airT) ? NAN : dewPointC(airT, v);
        if (!std::isnan(dp)) { snprintf(x, sizeof(x), "Roc\xC3\xADo %.1f%s", tempFromC(dp, U.temp), plotter::label(U.temp)); comma(x); }
        else x[0] = 0;
        est += meteoSub(2, Key::Humidity, x);
    } else meteoNone(2);
    // Viento real: el calculado con los sensores de a bordo o, si faltan, el de internet
    lv_label_set_text(mTile[3].unit, plotter::label(U.windSpeed));
    WindNow w = windNow();
    if (w.tru) {
        snprintf(b, sizeof(b), "%.1f", speedFromKn(w.tws, U.windSpeed)); comma(b);
        lv_label_set_text(mTile[3].value, b);
        if (w.truDir) snprintf(x, sizeof(x), "Del %s (%03d\xC2\xB0) \xC2\xB7 fuerza %d", cardinalEs(w.twd), (int)lround(norm360(w.twd)) % 360, beaufort(w.tws));
        else snprintf(x, sizeof(x), "Fuerza %d", beaufort(w.tws));
        if (w.computed) {
            char s[96];
            snprintf(s, sizeof(s), "%s \xC2\xB7 calculado a bordo", x);
            lv_label_set_text(mTile[3].sub, s);
            lv_obj_set_style_text_color(mTile[3].value, lv_color_hex(C_TEXT), 0);
        } else est += meteoSub(3, Key::Tws, x);
    } else meteoNone(3);
    lv_label_set_text(mTile[4].unit, plotter::label(U.windSpeed));
    if (store->get(Key::Gust, now, v)) {
        snprintf(b, sizeof(b), "%.1f", speedFromKn(v, U.windSpeed)); comma(b);
        lv_label_set_text(mTile[4].value, b);
        est += meteoSub(4, Key::Gust, "");
    } else meteoNone(4);
    lv_label_set_text(mTile[5].unit, distLabel());
    if (store->get(Key::Visibility, now, v)) {
        snprintf(b, sizeof(b), "%.1f", distFromNm(v, U.dist)); comma(b);
        lv_label_set_text(mTile[5].value, b);
        est += meteoSub(5, Key::Visibility, "");
    } else meteoNone(5);

    const settings::Values& sv = settings::get();
    const links::Status& ls = links::status();
    const net::Status& ns = net::status();
    char n[620], last[96];
    if (!sv.owmKey[0]) snprintf(last, sizeof(last), "Sin clave de OpenWeatherMap: se pone en Ajustes > Red.");
    else if (!ns.wifi) snprintf(last, sizeof(last), "Sin WiFi: no se puede pedir el tiempo de internet.");
    else if (!ls.wxMs) snprintf(last, sizeof(last), ns.wxFail ? "OpenWeatherMap no contesta (%u intentos)." : "Esperando la primera observaci\xC3\xB3n de OpenWeatherMap.", (unsigned)ns.wxFail);
    else snprintf(last, sizeof(last), "OpenWeatherMap%s%s \xC2\xB7 hace %u min.", ls.wxPlace[0] ? " \xC2\xB7 " : "", ls.wxPlace, (unsigned)((now - ls.wxMs) / 60000));
    char sky[160];
    skyLine(sky, sizeof(sky));
    // Avisos oficiales en vigor (el mas grave primero) y tiempo espacial
    char oa[220], spc[120] = "";
    if (!sv.alertsOn) snprintf(oa, sizeof(oa), "Avisos oficiales: apagados.");
    else if (oaList.size() == 0) snprintf(oa, sizeof(oa), "Avisos oficiales: ninguno en vigor%s%s.", alertsnet::status().msg[0] ? " \xC2\xB7 " : "", alertsnet::status().msg);
    else {
        int k = snprintf(oa, sizeof(oa), "Avisos (%u):", (unsigned)oaList.size());
        for (size_t i = 0; i < oaList.size() && i < 3 && k < (int)sizeof(oa) - 8; i++)
            k += snprintf(oa + k, sizeof(oa) - k, "%s %s %s, %s", i ? ";" : "", plotter::alertLevelName(oaList[i].level), oaList[i].event.c_str(), oaList[i].area.c_str());
    }
    if (oaSpace.valid()) snprintf(spc, sizeof(spc), "\nEspacio (SWPC): G%d R%d S%d \xC2\xB7 Kp %.1f%s", oaSpace.g < 0 ? 0 : oaSpace.g, oaSpace.r < 0 ? 0 : oaSpace.r,
                                  oaSpace.s < 0 ? 0 : oaSpace.s, oaSpace.kpNow() < 0 ? 0.0 : oaSpace.kpNow(), oaSpace.sfi > 0 ? "" : "");
    if (oaSpace.valid() && oaSpace.sfi > 0) { size_t l = strlen(spc); snprintf(spc + l, sizeof(spc) - l, " \xC2\xB7 SFI %.0f", oaSpace.sfi); }
    snprintf(n, sizeof(n), "%s\nDatos de internet: %s (solo si falta el sensor; en \xC3\xA1mbar, estimado)\n%s%s", sky, last, oa, spc);
    (void)est;
    lv_label_set_text(mNote, n);
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
    else if (what == 9) { lv_obj_set_hidden(aboutScr, false); return; }
    else if (what == 10) { openVessel(); return; }
    else if (what == 8) {                                 // Bluetooth: encender busca pasarela; apagar la olvida
        v.bleOn = !v.bleOn;
        v.bleAddr[0] = 0;
        links::applyBle();
    }
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
    lv_obj_set_size(info, 374, 336);
    b = textButton(page, "Red: WiFi y TCP", 183, 60, onSetting, 7);
    lv_obj_set_pos(b, 630, 344);
    b = textButton(page, "Embarcaci\xC3\xB3n", 183, 60, onSetting, 10);
    lv_obj_set_pos(b, 821, 344);
    b = textButton(page, "", 374, 60, onSetting, 8);
    lv_obj_set_pos(b, 630, 412);
    setBleLbl = lv_obj_get_child(b, 0);
    lv_obj_set_pos(info, 630, 0);
    lv_obj_t* t = label(info, "Estado", &dejavu_bold_20, C_TEXT);
    lv_obj_align(t, LV_ALIGN_TOP_LEFT, 14, 12);
    setInfo = label(info, "", &dejavu_16, C_SOFT);
    lv_obj_align(setInfo, LV_ALIGN_TOP_LEFT, 14, 48);
    lv_obj_set_width(setInfo, 346);
    lv_obj_set_style_text_line_space(setInfo, 6, 0);
    b = textButton(info, "Acerca de", 150, 40, onSetting, 9);         // arriba a la derecha: el estado ocupa todo el alto
    lv_obj_align(b, LV_ALIGN_TOP_RIGHT, -12, 8);
}

static void updateSettings() {
    const settings::Values& v = settings::get();
    lv_label_set_text(setSimLbl, v.simulator ? "Encendido" : "Apagado");
    lv_label_set_text(setBaudLbl, v.serialBaud == 38400 ? "38400 (AIS)" : "4800 (NMEA)");
    lv_label_set_text(setUnitsLbl, v.units == 1 ? "M\xC3\xA9trico" : (v.units == 2 ? "Imperial" : "N\xC3\xA1utico"));
    lv_label_set_text(setMapLbl, kBaseMaps[baseMap].name);
    lv_label_set_text(setCanLbl, v.can ? "Encendido" : "Apagado");
    {
        const ble::Status& bs = ble::status();
        char t[64];
        if (!v.bleOn) snprintf(t, sizeof(t), "Bluetooth: apagado");
        else if (bs.state == ble::CONNECTED) snprintf(t, sizeof(t), "Bluetooth: %.20s", bs.name);
        else if (bs.pinFailed) snprintf(t, sizeof(t), "Bluetooth: el tracker no acepta el PIN");
        else if (bs.state == ble::CONNECTING) snprintf(t, sizeof(t), "Bluetooth: conectando...");
        else snprintf(t, sizeof(t), v.bleAddr[0] ? "Bluetooth: buscando la pasarela" : "Bluetooth: buscando pasarela nueva");
        lv_label_set_text(setBleLbl, t);
    }
    char b[520], netTxt[80], gpsTxt[64];
    {                                                     // de donde sale la posicion ahora mismo
        uint32_t t = millis();
        double la, sats;
        if (!store->get(Key::Lat, t, la)) snprintf(gpsTxt, sizeof(gpsTxt), "sin posici\xC3\xB3n");
        else if (store->get(Key::SatsUsed, t, sats)) snprintf(gpsTxt, sizeof(gpsTxt), "%s, %d sat\xC3\xA9lites", sourceName(store->source(Key::Lat, t)), (int)lround(sats));
        else snprintf(gpsTxt, sizeof(gpsTxt), "%s", sourceName(store->source(Key::Lat, t)));
    }
    netSummary(netTxt, sizeof(netTxt), false);
    char timeTxt[48];                                     // reloj interno: hora UTC y quien la puso (GPS, NTP del ROA...)
    {
        const time_t tt = (time_t)tsync::now();
        const tsync::Status ts = tsync::status();
        if (!tt) snprintf(timeTxt, sizeof(timeTxt), "sin hora");
        else {
            struct tm u;
            gmtime_r(&tt, &u);
            snprintf(timeTxt, sizeof(timeTxt), "%02d:%02d:%02d UTC (%s)", u.tm_hour, u.tm_min, u.tm_sec,
                     ts.source == plotter::TimeSource::None ? "sin sincronizar" : plotter::timeSourceName(ts.source));
        }
    }
    snprintf(b, sizeof(b), "%d %%", v.brightness);
    lv_label_set_text(setBrightLbl, b);
    const links::Status& ls = links::status();
    char decTxt[40];                                      // declinacion magnetica del WMM en la posicion del barco
    if (!ls.hasDeclination) snprintf(decTxt, sizeof(decTxt), "sin posici\xC3\xB3n");
    else snprintf(decTxt, sizeof(decTxt), "%.1f\xC2\xB0 %s (%s)", fabs(ls.declination), ls.declination >= 0 ? "E" : "W", plotter::defaultWmm().name());
    for (char* c = decTxt; *c; c++) if (*c == '.' && c[1] >= '0' && c[1] <= '9') *c = ',';
    const NmeaStats& ns = links::serialStats();
    uint32_t now = millis();
    snprintf(b, sizeof(b),
             "Versi\xC3\xB3n %s\nPuerto serie: %u l\xC3\xADneas, %u con error\n\xC3\x9Altima l\xC3\xADnea: %s\nNMEA 2000: %u tramas, %u mensajes\n\xC3\x9Altima trama: %s\nRed: %s\nGPS: %s\nAIS: %u mensajes, %d blancos\nWaypoints: %d de %d \xC2\xB7 Bluetooth: %u l\xC3\xADneas\nHora: %s\nDeclinaci\xC3\xB3n: %s",
#ifdef PLOTTER_VERSION
             PLOTTER_VERSION,
#else
             "(PC)",
#endif
             (unsigned)ls.serialLines, (unsigned)(ns.badChecksum + ns.malformed),
             ls.serialLines ? (now - ls.serialLastMs < 5000 ? "ahora" : "hace rato") : "ninguna", (unsigned)ls.canFrames, (unsigned)ls.canMessages,
             !ls.can ? "apagado" : (ls.canFrames ? (now - ls.canLastMs < 5000 ? "ahora" : "hace rato") : "ninguna"), netTxt, gpsTxt, (unsigned)ls.aisMessages, links::targets().count(), wpN, kWps,
             (unsigned)ls.bleLines, timeTxt, decTxt);
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

// ---- Radio: MAYDAY, MAYDAY RELAY, PAN PAN y SECURITE para leer por el canal 16 ----
static lv_obj_t* rtScr = nullptr;
static lv_obj_t* rtKindBtn[4] = {};
static lv_obj_t* rtLangLbl = nullptr;
static lv_obj_t* rtNatLbl = nullptr;
static lv_obj_t* rtPobLbl = nullptr;
static lv_obj_t* rtText = nullptr;
static lv_obj_t* rtHint = nullptr;
static int rtKind = 0;
static int rtNature[4] = {7, 7, 0, 0};                    // MAYDAY y RELAY: «peligro grave e inminente» de partida
static uint32_t rtRefreshMs = 0;
static std::string rtRelayKey;                          // alerta LSD de la que se tomo la naturaleza (se puede cambiar despues)

static void rtRefresh() {
    if (!rtScr) return;
    settings::Values& st = settings::get();
    const uint32_t now = millis();
    plotter::RtInput in;
    in.kind = (plotter::RtKind)rtKind;
    in.lang = st.radioLang ? plotter::RtLang::En : plotter::RtLang::Es;
    in.nature = rtNature[rtKind];
    in.name = st.vesselName;
    in.callsign = st.vesselMCall;
    if (st.vesselMmsi) { char b[12]; snprintf(b, sizeof(b), "%09lu", (unsigned long)st.vesselMmsi); in.mmsi = b; }
    double la, lo;
    if (store->get(Key::Lat, now, la) && store->get(Key::Lon, now, lo)) { in.hasPos = true; in.lat = la; in.lon = lo; }
    const int64_t t = tsync::now();
    if (t > 1600000000) in.utcMin = (int)((t % 86400) / 60);
    in.persons = st.vesselPob;
    if (st.vesselLen > 0 || st.vesselName[0]) {
        char d[80];
        if (st.radioLang) snprintf(d, sizeof(d), st.vesselLen > 0 ? "%s, %d metres" : "%s", plotter::vessel::typeNameEn(st.vesselType), st.vesselLen);
        else snprintf(d, sizeof(d), st.vesselLen > 0 ? "%s de %d metros" : "%s", plotter::vessel::typeName(st.vesselType), st.vesselLen);
        in.description = d;
    }
    bool relayFrom = false;
    if (rtKind == 1) {                                    // la ultima alerta de socorro LSD recibida
        const auto& items = links::dsc().items();
        for (auto it = items.rbegin(); it != items.rend(); ++it) {
            if (!it->distress()) continue;
            in.relayMmsi = it->ship();
            if (it->hasPos) { in.relayHasPos = true; in.relayLat = it->pos.lat; in.relayLon = it->pos.lon; }
            if (it->time.size() == 4) in.relayUtcMin = atoi(it->time.substr(0, 2).c_str()) * 60 + atoi(it->time.substr(2).c_str());
            const int n = plotter::rtNatureFromDsc(it->nature());
            if (n >= 0 && it->key() != rtRelayKey) { rtRelayKey = it->key(); in.nature = rtNature[1] = n; }
            relayFrom = true;
            break;
        }
    }
    lv_label_set_text(rtText, plotter::radiotelMessage(in).c_str());
    char b[160];
    snprintf(b, sizeof(b), "%s", plotter::rtNatureName(in.kind, in.nature, in.lang));
    lv_label_set_text(rtNatLbl, b);
    if (st.vesselPob < 0) snprintf(b, sizeof(b), "Personas: no se dicen");
    else snprintf(b, sizeof(b), "Personas a bordo: %d", st.vesselPob);
    lv_label_set_text(rtPobLbl, b);
    if (rtLangLbl) lv_label_set_text(rtLangLbl, st.radioLang ? "Idioma: EN" : "Idioma: ES");
    static const char* const hint[4] = {
        "1. Con LSD: tapa del bot\xC3\xB3n DISTRESS, elige la naturaleza y p\xC3\xBAlsalo 5 s.  2. Canal 16 VHF (o 2182 kHz), potencia alta.  "
        "3. Pulsa el PTT, lee despacio, suelta y escucha. Sin respuesta en unos minutos, rep\xC3\xADtelo.",
        "Retransmite solo si el buque en peligro no puede hacerlo o nadie le ha acusado recibo. Canal 16, potencia alta.",
        "Canal 16 para la llamada; luego pasa a un canal de trabajo si te lo indican. Con LSD, llamada de urgencia a todos los buques.",
        "Anuncio por el canal 16 y el aviso en el canal de trabajo que digas en el anuncio. Con LSD, llamada de seguridad a todos los buques."};
    std::string h = hint[rtKind];
    if (rtKind == 1) h += relayFrom ? "  Datos de la \xC3\xBAltima alerta LSD recibida." : "  No hay ninguna alerta LSD recibida: rellena de viva voz.";
    if (!st.vesselName[0] || !st.vesselMmsi) h += "  Faltan datos de la embarcaci\xC3\xB3n (Ajustes > Embarcaci\xC3\xB3n).";
    lv_label_set_text(rtHint, h.c_str());
    for (int k = 0; k < 4; k++) {
        static const uint32_t col[4] = {C_RED, C_RED, C_WARN, C_ACC};
        lv_obj_set_style_bg_color(rtKindBtn[k], lv_color_hex(k == rtKind ? col[k] : C_PANEL), 0);
        lv_obj_t* l = lv_obj_get_child(rtKindBtn[k], 0);
        if (l) lv_obj_set_style_text_color(l, lv_color_hex(k == rtKind ? C_INK : C_TEXT), 0);
    }
}

static void onRadioBtn(lv_event_t* e) {
    const int a = (int)(intptr_t)lv_event_get_user_data(e);
    settings::Values& st = settings::get();
    if (a >= 1 && a <= 4) rtKind = a - 1;
    else if (a == 5) { st.radioLang = !st.radioLang; settings::save(); }
    else if (a == 6 || a == 7) {
        const int n = plotter::rtNatureCount((plotter::RtKind)rtKind);
        rtNature[rtKind] = (rtNature[rtKind] + (a == 7 ? 1 : n - 1)) % n;
    } else if (a == 8 || a == 9) {
        st.vesselPob = a == 9 ? (st.vesselPob < 0 ? 1 : std::min(st.vesselPob + 1, 999)) : (st.vesselPob <= 1 ? -1 : st.vesselPob - 1);
        settings::save();
    } else { lv_obj_set_hidden(rtScr, true); return; }
    rtRefresh();
}

static void makeRadio() {
    rtScr = lv_obj_create(lv_layer_top());
    lv_obj_remove_style_all(rtScr);
    lv_obj_set_size(rtScr, board::W, board::H);
    lv_obj_set_style_bg_color(rtScr, lv_color_hex(C_BG), 0);
    lv_obj_set_style_bg_opa(rtScr, LV_OPA_COVER, 0);
    lv_obj_set_scrollable(rtScr, false);
    lv_obj_set_clickable(rtScr, true);
    lv_obj_t* t = label(rtScr, "Radio", &dejavu_bold_20, C_TEXT);
    lv_obj_set_pos(t, 16, 18);
    static const struct { const char* txt; int x, w, a; } K[] = {
        {"MAYDAY", 110, 150, 1}, {"MAYDAY RELAY", 268, 190, 2}, {"PAN PAN", 466, 150, 3}, {"S\xC3\x89" "CURIT\xC3\x89", 624, 150, 4}};
    for (int k = 0; k < 4; k++) {
        rtKindBtn[k] = textButton(rtScr, K[k].txt, K[k].w, 48, onRadioBtn, K[k].a);
        lv_obj_set_pos(rtKindBtn[k], K[k].x, 8);
    }
    lv_obj_t* b = textButton(rtScr, "Cerrar", 124, 48, onRadioBtn, 0);
    lv_obj_set_pos(b, 884, 8);
    b = textButton(rtScr, "<", 52, 48, onRadioBtn, 6); lv_obj_set_pos(b, 16, 64);
    b = textButton(rtScr, ">", 52, 48, onRadioBtn, 7); lv_obj_set_pos(b, 76, 64);
    rtNatLbl = label(rtScr, "", &dejavu_bold_16, C_TEXT);
    lv_obj_set_pos(rtNatLbl, 140, 78);
    lv_obj_set_width(rtNatLbl, 380);
    lv_label_set_long_mode(rtNatLbl, LV_LABEL_LONG_DOT);
    b = textButton(rtScr, "-", 52, 48, onRadioBtn, 8); lv_obj_set_pos(b, 530, 64);
    b = textButton(rtScr, "+", 52, 48, onRadioBtn, 9); lv_obj_set_pos(b, 590, 64);
    rtPobLbl = label(rtScr, "", &dejavu_16, C_TEXT);
    lv_obj_set_pos(rtPobLbl, 652, 78);
    b = textButton(rtScr, "Idioma: ES", 160, 48, onRadioBtn, 5); lv_obj_set_pos(b, 848, 64);
    rtLangLbl = lv_obj_get_child(b, 0);
    lv_obj_t* panel = box(rtScr, C_PANEL, 10);
    lv_obj_set_pos(panel, 16, 122);
    lv_obj_set_size(panel, 992, 396);
    lv_obj_set_style_pad_all(panel, 14, 0);
    lv_obj_set_scroll_dir(panel, LV_DIR_VER);
    rtText = label(panel, "", &dejavu_bold_20, C_TEXT);
    lv_obj_set_width(rtText, 960);
    lv_obj_set_style_text_line_space(rtText, 6, 0);
    rtHint = label(rtScr, "", &dejavu_14, C_SOFT);
    lv_obj_set_pos(rtHint, 16, 526);
    lv_obj_set_width(rtHint, 992);
    lv_obj_set_hidden(rtScr, true);
}

static void openRadio(int kind, int nature) {
    if (!rtScr) makeRadio();
    if (kind >= 0 && kind < 4) rtKind = kind;
    if (nature >= 0 && nature < plotter::rtNatureCount((plotter::RtKind)rtKind)) rtNature[rtKind] = nature;
    rtRefresh();
    lv_obj_set_hidden(rtScr, false);
}

static void onRadioOpen(lv_event_t*) { openRadio(-1, -1); }

// Para las pruebas en el PC
void testRadio(int kind, int nature, int lang) {
    settings::Values& st = settings::get();
    if (!st.vesselName[0]) {                              // datos inventados para el dibujo
        snprintf(st.vesselName, sizeof(st.vesselName), "Mar de Prueba");
        st.vesselMmsi = 224000123;
        snprintf(st.vesselMCall, sizeof(st.vesselMCall), "EB0001");
        st.vesselLen = 11;
        st.vesselPob = 3;
    }
    st.radioLang = lang;
    openRadio(kind, nature);
}
void closeRadio() { if (rtScr) lv_obj_set_hidden(rtScr, true); }

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
    lv_obj_t* rad = lv_button_create(top);                 // Radio: MAYDAY, PAN PAN, SECURITE desde cualquier pestana
    lv_obj_remove_style_all(rad);
    lv_obj_set_size(rad, 84, TOP_H - 1);
    lv_obj_align(rad, LV_ALIGN_RIGHT_MID, -154, 0);
    lv_obj_set_style_bg_color(rad, lv_color_hex(C_WARN), 0);
    lv_obj_set_style_bg_opa(rad, LV_OPA_COVER, 0);
    lv_obj_t* radL = label(rad, "Radio", &dejavu_bold_16, C_INK);
    lv_obj_center(radL);
    lv_obj_add_event_cb(rad, onRadioOpen, LV_EVENT_CLICKED, nullptr);
    lblPos = label(top, "Sin posici\xC3\xB3n", &dejavu_16, C_SOFT);
    lv_obj_align(lblPos, LV_ALIGN_RIGHT_MID, -252, 0);

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
        else if (i == 4) makeWind(pages[i]);
        else if (i == 5) makePilot(pages[i]);
        else if (i == 6) makeMeteo(pages[i]);
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
    gridNav = grid;
    // Motor (NMEA 2000 127488 / 127489 / 127505 / 127508 o J1939): se ve tocando otra vez la pestana Instrumentos
    grid = lv_obj_create(pages[0]);
    lv_obj_remove_style_all(grid);
    lv_obj_set_size(grid, lv_pct(100), lv_pct(100));
    lv_obj_set_layout(grid, LV_LAYOUT_GRID);
    lv_obj_set_grid_dsc_array(grid, cols, rows);
    lv_obj_set_style_pad_gap(grid, PAD, 0);
    lv_obj_set_scrollable(grid, false);
    makeTile(grid, T_RPM, 0, 0, "R\xC3\x89GIMEN", "rpm", C_ACC);
    makeTile(grid, T_COOL, 1, 0, "REFRIGERANTE", "\xC2\xB0" "C");
    makeTile(grid, T_OILP, 2, 0, "PRESI\xC3\x93N ACEITE", "bar");
    makeTile(grid, T_FUEL, 3, 0, "COMBUSTIBLE", "%");
    makeTile(grid, T_BATT, 0, 1, "BATER\xC3\x8D" "A", "V");
    makeTile(grid, T_RATE, 1, 1, "CONSUMO", "L/h");
    makeTile(grid, T_OILT, 2, 1, "TEMP. ACEITE", "\xC2\xB0" "C");
    makeTile(grid, T_HOURS, 3, 1, "HORAS MOTOR", "h");
    gridEngine = grid;
    lv_obj_set_hidden(gridEngine, true);

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
    makeVessel();
    makeAstro();
    makeCt();
    makeAbout();
    makeModal();
    loadWaypoints();
    loadLights();
    loadNavtex();
    applyUnits();
    setBaseMap(settings::get().baseMap);
    {                                                     // fondeo que estaba vigilado antes de apagar o reiniciar
        const settings::Values& v = settings::get();
        anchorRode = v.anchorRode;
        if (v.anchorOn) anchorW.drop({v.anchorLat, v.anchorLon}, v.anchorRadius, millis());
    }
    showTab(0);
    showSplash(scr, "Plotter " PLOTTER_VERSION, &dejavu_bold_20);   // la bienvenida de APRS LoRa+, y luego esto
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

// Casilla de la pagina Motor: valor x escala; las temperaturas, en la unidad elegida en Ajustes
static void showEngine(int idx, Key k, int decimals, double scale, bool temp) {
    uint32_t now = millis();
    double v;
    if (temp) lv_label_set_text(tileUnit[idx], plotter::label(U.temp));
    if (!store->get(k, now, v)) {
        lv_label_set_text(tiles[idx].value, "--");
        lv_label_set_text(tiles[idx].sub, "Sin datos");
        return;
    }
    v = temp ? tempFromC(v, U.temp) : v * scale;
    char b[24];
    snprintf(b, sizeof(b), "%.*f", decimals, v);
    for (char* p = b; *p; p++) if (*p == '.') *p = ',';
    lv_label_set_text(tiles[idx].value, b);
    char t[48];
    snprintf(t, sizeof(t), "%s \xC2\xB7 hace %u s", sourceName(store->source(k, now)), (unsigned)(store->ageMs(k, now) / 1000));
    lv_label_set_text(tiles[idx].sub, t);
}

// ---- Carta > Corriente: corriente medida, corriente por la estima y rumbo a gobernar ----
static lv_obj_t* curScr = nullptr;
static lv_obj_t* curTa[6] = {};      // rumbo efectivo, velocidad, abatimiento, corriente rumbo, corriente nudos, Rv navegado
static lv_obj_t* curKb = nullptr;
static lv_obj_t* curLive = nullptr;
static lv_obj_t* curOut = nullptr;
static plotter::CurrentAverager curAvg(120);
static uint32_t curSampleMs = 0, curLiveMs = 0;
static bool curMarked = false;
static LatLon curMarkPos{0, 0};
static int64_t curMarkT = 0;

static void decimalComma(char* b) {
    for (char* c = b; *c; c++) if (*c == '.' && c > b && isdigit((unsigned char)c[-1]) && isdigit((unsigned char)c[1])) *c = ',';
}

static double curNum(int i, bool& ok) {
    char d[16];
    snprintf(d, sizeof(d), "%s", lv_textarea_get_text(curTa[i]));
    for (char* c = d; *c; c++) if (*c == ',') *c = '.';
    ok = d[0] != 0 && !(d[0] == '-' && d[1] == 0);
    return atof(d);
}

static void curSet(int i, double v, int dec) {
    char b[16];
    snprintf(b, sizeof(b), "%.*f", dec, v);
    decimalComma(b);
    lv_textarea_set_text(curTa[i], b);
}

// Corriente medida: fondo (GPS) - agua (rumbo verdadero y corredera), media de 2 minutos; una muestra por segundo
static void sampleCurrent() {
    const uint32_t now = millis();
    if (now - curSampleMs < 1000) return;
    curSampleMs = now;
    double h, stw, cog, sog;
    if (!store->get(Key::Heading, now, h) || !store->get(Key::Stw, now, stw) || !store->get(Key::Cog, now, cog) || !store->get(Key::Sog, now, sog)) return;
    if (stw < 0.5 && sog < 0.5) return;                   // parado: el rumbo y la demora no dicen nada
    curAvg.add(plotter::currentFromVectors(h, stw, 0, cog, sog), now);
}

static void curLiveText() {
    if (!curLive) return;
    const uint32_t now = millis();
    plotter::Current c;
    char b[220];
    double v;
    if (curAvg.get(c, now)) snprintf(b, sizeof(b), "Corriente medida (GPS frente a rumbo y corredera, media de 2 min): al %03.0f\xC2\xB0, %.1f kn", c.setDeg, c.driftKn);
    else if (!store->get(Key::Stw, now, v)) snprintf(b, sizeof(b), "Sin corredera la corriente no se puede medir: calc\xC3\xBAlala por la estima (Marcar salida y, m\xC3\xA1s tarde, Por la estima).");
    else snprintf(b, sizeof(b), "Midiendo la corriente (hace falta rumbo verdadero, corredera y GPS durante 20 s).");
    if (curMarked) {
        const int64_t t = curMarkT;
        const int sod = (int)(((t % 86400) + 86400) % 86400);
        const int n = (int)strlen(b);
        snprintf(b + n, sizeof(b) - (size_t)n, "\nSalida marcada a las %02d:%02d UTC (hace %.0f min).", sod / 3600, sod / 60 % 60, (tsync::now() - t) / 60.0);
    }
    decimalComma(b);
    lv_label_set_text(curLive, b);
}

// rumbo de aguja para un rumbo verdadero: Ra = Rv - dm - desvio(Ra)
static bool curCompass(double rv, double& ra, double& dm, double& dev) {
    const links::Status& ls = links::status();
    if (!ls.hasDeclination) return false;
    dm = ls.declination;
    const plotter::DeviationCard& card = links::deviationCard();
    dev = 0;
    ra = norm360(rv - dm);
    for (int i = 0; i < 4 && card.terms; i++) { dev = card.at(ra); ra = norm360(rv - dm - dev); }
    return true;
}

static void curSteer() {
    bool ok[5];
    const double ref = curNum(0, ok[0]), stw = curNum(1, ok[1]), lw = curNum(2, ok[2]);
    plotter::Current c;
    c.setDeg = curNum(3, ok[3]);
    c.driftKn = curNum(4, ok[4]);
    if (!ok[0] || ref < 0 || ref >= 360) { lv_label_set_text(curOut, "Escribe el rumbo efectivo que quieres hacer (0 a 359\xC2\xB0) o pulsa Destino."); return; }
    if (!ok[1] || stw <= 0) { lv_label_set_text(curOut, "Escribe la velocidad en el agua (nudos) o pulsa Corredera."); return; }
    if (!ok[3] || !ok[4]) { c.driftKn = 0; c.setDeg = 0; }
    double hdg, sog;
    if (!plotter::courseToSteer(ref, stw, ok[2] ? lw : 0, c, hdg, sog)) {
        lv_label_set_text(curOut, "Con esa corriente no se puede hacer ese rumbo: cruza m\xC3\xA1s r\xC3\xA1pido que el barco o lo frena del todo.");
        return;
    }
    char b[420];
    int n = snprintf(b, sizeof(b), "Rumbo verdadero a gobernar: %05.1f\xC2\xB0 (corrige %+.0f\xC2\xB0) \xC2\xB7 velocidad efectiva %.1f kn", hdg, remainder(hdg - ref, 360.0), sog);
    double ra, dm, dev;
    if (curCompass(hdg, ra, dm, dev)) {
        n += snprintf(b + n, sizeof(b) - (size_t)n, "\nRumbo de aguja: %05.1f\xC2\xB0 (declinaci\xC3\xB3n %.1f\xC2\xB0 %s", ra, fabs(dm), dm >= 0 ? "E" : "W");
        if (n > 0 && n < (int)sizeof(b))
            n += links::deviationCard().terms ? snprintf(b + n, sizeof(b) - (size_t)n, ", desv\xC3\xADo %+.1f\xC2\xB0)", dev)
                                              : snprintf(b + n, sizeof(b) - (size_t)n, ", sin tablilla de desv\xC3\xADos)");
    }
    const RouteStatus& st = nav.status();
    if (navOn && st.active && n > 0 && n < (int)sizeof(b)) {
        const int mins = (int)lround(st.distanceNm / sog * 60);
        n += snprintf(b + n, sizeof(b) - (size_t)n, "\nAl destino: %.1f mn, %d h %02d min a esa velocidad efectiva.", st.distanceNm, mins / 60, mins % 60);
    }
    decimalComma(b);
    lv_label_set_text(curOut, b);
}

static void curFromDr() {
    const uint32_t now = millis();
    double la, lo;
    if (!curMarked) { lv_label_set_text(curOut, "Primero pulsa Marcar salida en un punto conocido; luego gobierna a rumbo y velocidad constantes."); return; }
    if (!store->get(Key::Lat, now, la) || !store->get(Key::Lon, now, lo)) { lv_label_set_text(curOut, "Hace falta la situaci\xC3\xB3n del barco ahora (GPS)."); return; }
    bool ok[3];
    const double rv = curNum(5, ok[0]), stw = curNum(1, ok[1]), lw = curNum(2, ok[2]);
    if (!ok[0] || rv < 0 || rv >= 360) { lv_label_set_text(curOut, "Escribe el rumbo verdadero gobernado desde la salida (Rv navegado) o pulsa Proa."); return; }
    if (!ok[1] || stw < 0) { lv_label_set_text(curOut, "Escribe la velocidad en el agua (nudos)."); return; }
    const double hours = (tsync::now() - curMarkT) / 3600.0;
    if (hours < 0.1) { lv_label_set_text(curOut, "Han pasado menos de 6 minutos desde la salida: espera m\xC3\xA1s para que la corriente se note."); return; }
    LatLon dr;
    plotter::Current c;
    plotter::currentFromDr(curMarkPos, rv, stw, ok[2] ? lw : 0, hours, {la, lo}, dr, c);
    curSet(3, c.setDeg, 0);
    curSet(4, c.driftKn, 1);
    const int mins = (int)lround(hours * 60);
    char p[48], b[360];
    formatLatLon(p, sizeof(p), dr, 1, true);
    for (char* q = p; *q; q++) if ((unsigned char)q[0] == 0xE2 && (unsigned char)q[1] == 0x80 && (unsigned char)q[2] == 0xB2) { q[0] = '\''; memmove(q + 1, q + 3, strlen(q + 3) + 1); }
    snprintf(b, sizeof(b), "Estima tras %d h %02d min: %s.\nLa situaci\xC3\xB3n est\xC3\xA1 %.1f mn al %03.0f\xC2\xB0 de la estima: corriente al %03.0f\xC2\xB0, %.1f kn (copiada arriba para el rumbo a gobernar).",
             mins / 60, mins % 60, p, c.driftKn * hours, c.setDeg, c.setDeg, c.driftKn);
    decimalComma(b);
    lv_label_set_text(curOut, b);
}

static void onCurrentBtn(lv_event_t* e) {
    const int a = (int)(intptr_t)lv_event_get_user_data(e);
    const uint32_t now = millis();
    double v;
    if (a == 1) curSteer();
    else if (a == 2) curFromDr();
    else if (a == 3) {
        double la, lo;
        if (store->get(Key::Lat, now, la) && store->get(Key::Lon, now, lo) && tsync::now() > 1600000000) {
            curMarked = true; curMarkPos = {la, lo}; curMarkT = tsync::now();
            if (store->get(Key::Heading, now, v)) curSet(5, v, 0);
            lv_label_set_text(curOut, "Salida marcada en la situaci\xC3\xB3n del barco. Gobierna a rumbo y velocidad constantes y pulsa Por la estima pasado un rato (mejor una hora o m\xC3\xA1s).");
        } else lv_label_set_text(curOut, "Hace falta la situaci\xC3\xB3n del barco y la hora para marcar la salida.");
    } else if (a == 4) {                                  // destino
        const RouteStatus& st = nav.status();
        if (navOn && st.active) curSet(0, st.bearing, 0);
        else lv_label_set_text(curOut, "No hay destino: elige uno en la carta (Ir a) o en Rutas.");
    } else if (a == 5) {                                  // corredera
        if (store->get(Key::Stw, now, v)) curSet(1, v, 1);
        else lv_label_set_text(curOut, "No llega la corredera: escribe la velocidad de m\xC3\xA1quina.");
    } else if (a == 6) {                                  // corriente medida
        plotter::Current c;
        if (curAvg.get(c, now)) { curSet(3, c.setDeg, 0); curSet(4, c.driftKn, 1); }
        else lv_label_set_text(curOut, "Todav\xC3\xAD" "a no hay corriente medida.");
    } else if (a == 7) {                                  // proa
        if (store->get(Key::Heading, now, v)) curSet(5, v, 0);
        else lv_label_set_text(curOut, "No llega el rumbo verdadero del barco.");
    } else { lv_obj_set_hidden(curScr, true); return; }
    curLiveText();
}

static void onCurField(lv_event_t* e) { lv_keyboard_set_textarea(curKb, (lv_obj_t*)lv_event_get_target(e)); }

static void makeCurrent() {
    curScr = lv_obj_create(lv_layer_top());
    lv_obj_remove_style_all(curScr);
    lv_obj_set_size(curScr, board::W, board::H);
    lv_obj_set_style_bg_color(curScr, lv_color_hex(C_BG), 0);
    lv_obj_set_style_bg_opa(curScr, LV_OPA_COVER, 0);
    lv_obj_set_scrollable(curScr, false);
    lv_obj_set_clickable(curScr, true);
    lv_obj_t* t = label(curScr, "Corriente", &dejavu_bold_20, C_TEXT);
    lv_obj_set_pos(t, 16, 18);
    static const struct { const char* txt; int x, y, w, a; } B[] = {
        {"Rumbo a gobernar", 170, 8, 220, 1}, {"Marcar salida", 398, 8, 170, 3}, {"Por la estima", 576, 8, 170, 2}, {"Cerrar", 884, 8, 124, 0},
        {"Destino", 650, 82, 112, 4}, {"Corredera", 770, 82, 130, 5}, {"Medida", 442, 154, 112, 6}, {"Proa", 770, 154, 112, 7}};
    for (const auto& d : B) {
        lv_obj_t* b = textButton(curScr, d.txt, d.w, 48, onCurrentBtn, d.a);
        lv_obj_set_pos(b, d.x, d.y);
    }
    static const struct { int x, y, w, max; const char* hint; const char* chars; } F[6] = {
        {16, 82, 210, 5, "Rumbo efectivo, \xC2\xB0", "0123456789.,"}, {234, 82, 200, 4, "Veloc. agua, kn", "0123456789.,"},
        {442, 82, 200, 5, "Abatim., \xC2\xB0 (+Er)", "-0123456789.,"}, {16, 154, 210, 5, "Corriente al, \xC2\xB0", "0123456789.,"},
        {234, 154, 200, 4, "Corriente, kn", "0123456789.,"}, {562, 154, 200, 5, "Rv navegado, \xC2\xB0", "0123456789.,"}};
    static const char* const cap[6] = {"Rumbo efectivo deseado (\xC2\xB0)", "Velocidad en el agua (kn)", "Abatimiento (\xC2\xB0, + a estribor)",
                                       "Corriente: rumbo (\xC2\xB0)", "Corriente: intensidad (kn)", "Rv navegado desde la salida (\xC2\xB0)"};
    for (int i = 0; i < 6; i++) {
        lv_obj_t* l = label(curScr, cap[i], &dejavu_14, C_SOFT);
        lv_obj_set_pos(l, F[i].x + 2, F[i].y - 20);
    }
    lv_obj_t* keep = vesScr;                              // vesField crea sus campos en vesScr: se le presta esta pantalla
    lv_obj_t* keepTa[6];
    for (int i = 0; i < 6; i++) keepTa[i] = vesTa[i];
    vesScr = curScr;
    for (int i = 0; i < 6; i++) {
        lv_obj_t* ta = vesField(i, F[i].x, F[i].y, F[i].w, F[i].hint, F[i].max, F[i].chars);
        lv_obj_remove_event_cb(ta, onVesField);
        lv_obj_add_event_cb(ta, onCurField, LV_EVENT_FOCUSED, nullptr);
        lv_obj_add_event_cb(ta, onCurField, LV_EVENT_CLICKED, nullptr);
        curTa[i] = ta;
        vesTa[i] = keepTa[i];
    }
    vesScr = keep;
    lv_textarea_set_text(curTa[2], "0");
    curLive = label(curScr, "", &dejavu_16, C_ACC);
    lv_obj_set_pos(curLive, 16, 212);
    lv_obj_set_width(curLive, 992);
    curOut = label(curScr, "Rumbo a gobernar: rumbo efectivo que quieres hacer, velocidad en el agua, abatimiento y corriente.", &dejavu_16, C_SOFT);
    lv_obj_set_pos(curOut, 16, 256);
    lv_obj_set_width(curOut, 992);
    curKb = lv_keyboard_create(curScr);
    lv_keyboard_set_mode(curKb, LV_KEYBOARD_MODE_NUMBER);
    lv_obj_set_size(curKb, board::W, 220);
    lv_obj_align(curKb, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_color(curKb, lv_color_hex(C_BG), 0);
    lv_obj_set_style_bg_opa(curKb, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(curKb, 6, 0);
    lv_obj_set_style_pad_gap(curKb, 6, 0);
    lv_obj_set_style_border_width(curKb, 0, 0);
    lv_obj_set_style_bg_color(curKb, lv_color_hex(C_PANEL), LV_PART_ITEMS);
    lv_obj_set_style_bg_opa(curKb, LV_OPA_COVER, LV_PART_ITEMS);
    lv_obj_set_style_bg_color(curKb, lv_color_hex(C_ACC), LV_PART_ITEMS | LV_STATE_PRESSED);
    lv_obj_set_style_text_color(curKb, lv_color_hex(C_TEXT), LV_PART_ITEMS);
    lv_obj_set_style_radius(curKb, 8, LV_PART_ITEMS);
    lv_obj_set_style_border_width(curKb, 0, LV_PART_ITEMS);
    lv_obj_set_style_shadow_width(curKb, 0, LV_PART_ITEMS);
    lv_obj_set_hidden(curScr, true);
}

static void openCurrent() {
    if (!curScr) makeCurrent();
    const uint32_t now = millis();
    double v;
    const RouteStatus& st = nav.status();
    if (!lv_textarea_get_text(curTa[0])[0] && navOn && st.active) curSet(0, st.bearing, 0);
    if (!lv_textarea_get_text(curTa[1])[0] && store->get(Key::Stw, now, v)) curSet(1, v, 1);
    plotter::Current c;
    if (!lv_textarea_get_text(curTa[3])[0] && curAvg.get(c, now)) { curSet(3, c.setDeg, 0); curSet(4, c.driftKn, 1); }
    lv_keyboard_set_textarea(curKb, curTa[0]);
    curLiveText();
    lv_obj_set_hidden(curScr, false);
}

// Para las pruebas en el PC: rumbo a gobernar con unos datos fijos (mode 0) o la corriente por la estima con la salida hace 'mins' (mode 1)
void testCurrent(int mode, int mins) {
    openCurrent();
    lv_textarea_set_text(curTa[0], "90");
    lv_textarea_set_text(curTa[1], "6");
    lv_textarea_set_text(curTa[2], "3");
    lv_textarea_set_text(curTa[3], "180");
    lv_textarea_set_text(curTa[4], "1,5");
    if (mode == 1) {
        double la, lo;
        if (store->get(Key::Lat, millis(), la) && store->get(Key::Lon, millis(), lo)) {
            curMarked = true;
            curMarkT = tsync::now() - mins * 60;
            // ha ido al 090 a 6 kn con una corriente al 200 de 1,2 kn: la salida se reconstruye hacia atras
            curMarkPos = plotter::rhumbDestination(plotter::rhumbDestination({la, lo}, 20, 1.2 * mins / 60), 270, 6.0 * mins / 60);
            lv_textarea_set_text(curTa[2], "0");
            lv_textarea_set_text(curTa[5], "90");
        }
        curFromDr();
    } else curSteer();
    curLiveText();
}

// ---- Carta > Demoras: situacion por dos demoras, simultaneas o no (demora trasladada) ----
struct BrgPoint { LatLon p; char name[40]; };
static std::vector<BrgPoint> brgPoints;
static int brgSel[2] = {0, 0};
static int64_t brgT[2] = {0, 0};
static lv_obj_t* brgScr = nullptr;
static lv_obj_t* brgTa[5] = {};      // demora 1, demora 2, rumbo, velocidad, correccion total
static lv_obj_t* brgKb = nullptr;
static lv_obj_t* brgPtLbl[2] = {};
static lv_obj_t* brgTimeLbl[2] = {};
static lv_obj_t* brgOut = nullptr;

static double brgNum(int i, bool& ok) {
    char d[16];
    snprintf(d, sizeof(d), "%s", lv_textarea_get_text(brgTa[i]));
    for (char* c = d; *c; c++) if (*c == ',') *c = '.';
    ok = d[0] != 0 && !(d[0] == '-' && d[1] == 0);
    return atof(d);
}

static void brgSet(int i, double v, int dec) {
    char b[16];
    snprintf(b, sizeof(b), "%.*f", dec, v);
    decimalComma(b);
    lv_textarea_set_text(brgTa[i], b);
}

// Puntos conocidos: luces de la tarjeta a menos de 40 millas y waypoints, del mas cercano al mas lejano
static void brgCollect() {
    brgPoints.clear();
    const uint32_t now = millis();
    double la, lo;
    const bool own = store->get(Key::Lat, now, la) && store->get(Key::Lon, now, lo);
    std::vector<std::pair<double, BrgPoint>> v;
    for (const LightRecord& l : lightList) {
        const double d = own ? gcDistanceNm({la, lo}, {l.lat, l.lon}) : 0;
        if (own && d > 40) continue;
        BrgPoint b;
        b.p = {l.lat, l.lon};
        snprintf(b.name, sizeof(b.name), "%s", l.name);
        v.push_back({d, b});
    }
    for (int i = 0; i < wpN; i++) {
        BrgPoint b;
        b.p = wps[i].p;
        snprintf(b.name, sizeof(b.name), "%s", wps[i].name);
        v.push_back({own ? gcDistanceNm({la, lo}, wps[i].p) : 0, b});
    }
    std::sort(v.begin(), v.end(), [](const std::pair<double, BrgPoint>& a, const std::pair<double, BrgPoint>& b) { return a.first < b.first; });
    for (size_t i = 0; i < v.size() && i < 60; i++) brgPoints.push_back(v[i].second);
    for (int k = 0; k < 2; k++) if (brgSel[k] >= (int)brgPoints.size()) brgSel[k] = 0;
}

static void brgRefresh() {
    for (int k = 0; k < 2; k++) {
        char b[80];
        if (brgPoints.empty()) snprintf(b, sizeof(b), "Punto %d: sin luces ni waypoints cerca", k + 1);
        else snprintf(b, sizeof(b), "Punto %d: %s", k + 1, brgPoints[brgSel[k]].name);
        lv_label_set_text(brgPtLbl[k], b);
        if (brgT[k]) {
            const int sod = (int)(((brgT[k] % 86400) + 86400) % 86400);
            snprintf(b, sizeof(b), "%02d:%02d:%02d UTC", sod / 3600, sod / 60 % 60, sod % 60);
        } else snprintf(b, sizeof(b), "%s", k == 0 ? "sin hora" : "ahora");
        lv_label_set_text(brgTimeLbl[k], b);
    }
}

static void brgCompute() {
    if (brgPoints.empty()) { lv_label_set_text(brgOut, "Hacen falta puntos conocidos: la lista de luces de la tarjeta o waypoints."); return; }
    bool ok[5];
    const double d1 = brgNum(0, ok[0]), d2 = brgNum(1, ok[1]), rc = brgNum(2, ok[2]), sp = brgNum(3, ok[3]), ct = brgNum(4, ok[4]);
    if (!ok[0] || !ok[1] || d1 < 0 || d1 >= 360 || d2 < 0 || d2 >= 360) { lv_label_set_text(brgOut, "Escribe las dos demoras (0 a 359\xC2\xB0)."); return; }
    const int64_t t2 = brgT[1] ? brgT[1] : tsync::now();
    double runNm = 0;
    if (brgT[0] && t2 > brgT[0]) {
        if (!ok[2] || !ok[3]) { lv_label_set_text(brgOut, "Demoras a horas distintas: escribe el rumbo efectivo y la velocidad (o pulsa GPS)."); return; }
        runNm = sp * (t2 - brgT[0]) / 3600.0;
    }
    const double c = ok[4] ? ct : 0;
    const BrgPoint& a = brgPoints[brgSel[0]];
    const BrgPoint& b = brgPoints[brgSel[1]];
    plotter::BearingFix f;
    if (!plotter::fixFromBearings(a.p, norm360(d1 + c), b.p, norm360(d2 + c), ok[2] ? rc : 0, runNm, f)) {
        char w[200];
        snprintf(w, sizeof(w), f.cutDeg < 10 ? "Las dos l\xC3\xADneas son casi paralelas (corte de %.0f\xC2\xB0): elige puntos m\xC3\xA1s separados o espera a que cambie la demora."
                                             : "Las l\xC3\xADneas se cortan detr\xC3\xA1s de los puntos: revisa las demoras y la correcci\xC3\xB3n total.", f.cutDeg);
        decimalComma(w);
        lv_label_set_text(brgOut, w);
        brgOn = false;
        return;
    }
    brgOn = true;
    brgFix = f.fix;
    brgFirst = {a.p.lat + (f.fix.lat - f.first.lat), a.p.lon + (f.fix.lon - f.first.lon)};   // el punto 1 trasladado
    brgMark2 = b.p;
    char p[48], o[420];
    formatLatLon(p, sizeof(p), f.fix, 1, true);
    for (char* q = p; *q; q++) if ((unsigned char)q[0] == 0xE2 && (unsigned char)q[1] == 0x80 && (unsigned char)q[2] == 0xB2) { q[0] = '\''; memmove(q + 1, q + 3, strlen(q + 3) + 1); }
    int n = snprintf(o, sizeof(o), "Situaci\xC3\xB3n: %s \xC2\xB7 a %.1f mn de %s \xC2\xB7 corte %.0f\xC2\xB0%s", p, f.dist2Nm, b.name, f.cutDeg,
                     f.cutDeg < 30 ? " (corte malo: poco fiable)" : "");
    if (runNm > 0 && n > 0 && n < (int)sizeof(o))
        n += snprintf(o + n, sizeof(o) - (size_t)n, "\nCamino entre las dos demoras: %.1f mn al %03.0f\xC2\xB0 (primera l\xC3\xADnea trasladada).", runNm, rc);
    const uint32_t now = millis();
    double la, lo;
    if (store->get(Key::Lat, now, la) && store->get(Key::Lon, now, lo) && n > 0 && n < (int)sizeof(o))
        snprintf(o + n, sizeof(o) - (size_t)n, "\nEl GPS est\xC3\xA1 a %.2f mn al %03.0f\xC2\xB0 de esa situaci\xC3\xB3n. Dibujada en la carta.", gcDistanceNm(f.fix, {la, lo}), rhumbBearing(f.fix, {la, lo}));
    decimalComma(o);
    lv_label_set_text(brgOut, o);
}

static void onBrgBtn(lv_event_t* e) {
    const int a = (int)(intptr_t)lv_event_get_user_data(e);
    const uint32_t now = millis();
    const int np = (int)brgPoints.size();
    double v, w;
    if (a == 1) brgCompute();
    else if (a == 2) { brgOn = false; brgT[0] = brgT[1] = 0; lv_label_set_text(brgOut, "Borrado."); }
    else if (a >= 10 && a < 14 && np) { const int k = (a - 10) / 2; brgSel[k] = (brgSel[k] + ((a % 2) ? 1 : np - 1)) % np; }
    else if (a == 14 || a == 15) brgT[a - 14] = tsync::now();
    else if (a == 16) {                                   // rumbo y velocidad del GPS
        if (store->get(Key::Cog, now, v) && store->get(Key::Sog, now, w)) { brgSet(2, v, 0); brgSet(3, w, 1); }
        else lv_label_set_text(brgOut, "No llegan rumbo y velocidad del GPS.");
    } else if (a == 17) {                                 // correccion total = declinacion + desvio al rumbo de aguja
        const links::Status& ls = links::status();
        double hm;
        if (!ls.hasDeclination) lv_label_set_text(brgOut, "Sin declinaci\xC3\xB3n: hace falta la posici\xC3\xB3n.");
        else {
            const plotter::DeviationCard& card = links::deviationCard();
            const double dev = card.terms && store->get(Key::HeadingMag, now, hm) ? card.at(hm) : 0;
            brgSet(4, ls.declination + dev, 1);
        }
    } else if (a == 0) { lv_obj_set_hidden(brgScr, true); return; }
    brgRefresh();
}

static void onBrgField(lv_event_t* e) { lv_keyboard_set_textarea(brgKb, (lv_obj_t*)lv_event_get_target(e)); }

static void makeBearings() {
    brgScr = lv_obj_create(lv_layer_top());
    lv_obj_remove_style_all(brgScr);
    lv_obj_set_size(brgScr, board::W, board::H);
    lv_obj_set_style_bg_color(brgScr, lv_color_hex(C_BG), 0);
    lv_obj_set_style_bg_opa(brgScr, LV_OPA_COVER, 0);
    lv_obj_set_scrollable(brgScr, false);
    lv_obj_set_clickable(brgScr, true);
    lv_obj_t* t = label(brgScr, "Demoras", &dejavu_bold_20, C_TEXT);
    lv_obj_set_pos(t, 16, 18);
    static const struct { const char* txt; int x, y, w, a; } B[] = {
        {"Calcular y trazar", 470, 8, 220, 1}, {"Borrar", 698, 8, 124, 2}, {"Cerrar", 884, 8, 124, 0},
        {"<", 16, 64, 52, 10}, {">", 76, 64, 52, 11}, {"Ahora", 772, 64, 100, 14},
        {"<", 16, 122, 52, 12}, {">", 76, 122, 52, 13}, {"Ahora", 772, 122, 100, 15},
        {"GPS", 442, 200, 90, 16}, {"Ct", 772, 200, 90, 17}};
    for (const auto& d : B) {
        lv_obj_t* b = textButton(brgScr, d.txt, d.w, 48, onBrgBtn, d.a);
        lv_obj_set_pos(b, d.x, d.y);
    }
    for (int k = 0; k < 2; k++) {
        brgPtLbl[k] = label(brgScr, "", &dejavu_16, C_TEXT);
        lv_obj_set_pos(brgPtLbl[k], 140, 78 + 58 * k);
        lv_obj_set_width(brgPtLbl[k], 400);
        lv_label_set_long_mode(brgPtLbl[k], LV_LABEL_LONG_DOT);
        brgTimeLbl[k] = label(brgScr, "", &dejavu_16, C_SOFT);
        lv_obj_set_pos(brgTimeLbl[k], 884, 78 + 58 * k);
    }
    static const struct { int x, y, w, max; const char* hint; const char* chars; } F[5] = {
        {556, 64, 200, 5, "Demora 1, \xC2\xB0", "0123456789.,"}, {556, 122, 200, 5, "Demora 2, \xC2\xB0", "0123456789.,"},
        {16, 200, 200, 5, "Rumbo, \xC2\xB0", "0123456789.,"}, {226, 200, 200, 4, "Velocidad, kn", "0123456789.,"},
        {556, 200, 200, 5, "Ct, \xC2\xB0 (+E)", "-0123456789.,"}};
    static const char* const cap[5] = {"", "", "Rumbo efectivo (\xC2\xB0)", "Velocidad efectiva (kn)", "Correcci\xC3\xB3n total (\xC2\xB0, + E)"};
    lv_obj_t* keep = vesScr;                              // vesField crea sus campos en vesScr: se le presta esta pantalla
    lv_obj_t* keepTa[5];
    for (int i = 0; i < 5; i++) keepTa[i] = vesTa[i];
    vesScr = brgScr;
    for (int i = 0; i < 5; i++) {
        lv_obj_t* ta = vesField(i, F[i].x, F[i].y, F[i].w, F[i].hint, F[i].max, F[i].chars);
        lv_obj_remove_event_cb(ta, onVesField);
        lv_obj_add_event_cb(ta, onBrgField, LV_EVENT_FOCUSED, nullptr);
        lv_obj_add_event_cb(ta, onBrgField, LV_EVENT_CLICKED, nullptr);
        brgTa[i] = ta;
        vesTa[i] = keepTa[i];
        if (cap[i][0]) { lv_obj_t* l = label(brgScr, cap[i], &dejavu_14, C_SOFT); lv_obj_set_pos(l, F[i].x + 2, F[i].y - 20); }
    }
    vesScr = keep;
    lv_textarea_set_text(brgTa[4], "0");
    brgOut = label(brgScr, "Elige los dos puntos (pueden ser el mismo), escribe cada demora y pulsa Ahora al tomarla. Con demoras a horas distintas, el rumbo y la velocidad efectivos trasladan la primera.", &dejavu_16, C_SOFT);
    lv_obj_set_pos(brgOut, 16, 258);
    lv_obj_set_width(brgOut, 992);
    brgKb = lv_keyboard_create(brgScr);
    lv_keyboard_set_mode(brgKb, LV_KEYBOARD_MODE_NUMBER);
    lv_obj_set_size(brgKb, board::W, 220);
    lv_obj_align(brgKb, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_color(brgKb, lv_color_hex(C_BG), 0);
    lv_obj_set_style_bg_opa(brgKb, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(brgKb, 6, 0);
    lv_obj_set_style_pad_gap(brgKb, 6, 0);
    lv_obj_set_style_border_width(brgKb, 0, 0);
    lv_obj_set_style_bg_color(brgKb, lv_color_hex(C_PANEL), LV_PART_ITEMS);
    lv_obj_set_style_bg_opa(brgKb, LV_OPA_COVER, LV_PART_ITEMS);
    lv_obj_set_style_bg_color(brgKb, lv_color_hex(C_ACC), LV_PART_ITEMS | LV_STATE_PRESSED);
    lv_obj_set_style_text_color(brgKb, lv_color_hex(C_TEXT), LV_PART_ITEMS);
    lv_obj_set_style_radius(brgKb, 8, LV_PART_ITEMS);
    lv_obj_set_style_border_width(brgKb, 0, LV_PART_ITEMS);
    lv_obj_set_style_shadow_width(brgKb, 0, LV_PART_ITEMS);
    lv_obj_set_hidden(brgScr, true);
}

static void openBearings() {
    if (!brgScr) makeBearings();
    brgCollect();
    brgRefresh();
    lv_keyboard_set_textarea(brgKb, brgTa[0]);
    lv_obj_set_hidden(brgScr, false);
}

// Para las pruebas en el PC: mode 0 = dos demoras simultaneas a las dos luces mas cercanas; 1 = demora trasladada a la
// mas cercana (el barco ha hecho 3 millas al 090 en 30 min); las demoras salen de la posicion del simulador
void testBearings(int mode) {
    openBearings();
    double la, lo;
    if (brgPoints.size() < 2 || !store->get(Key::Lat, millis(), la) || !store->get(Key::Lon, millis(), lo)) return;
    const LatLon here{la, lo};
    brgSel[0] = 0;
    brgSel[1] = 0;
    if (mode == 0) {                                      // la segunda, la que mejor corta con la primera entre las cercanas
        double best = -1;
        for (int i = 1; i < (int)brgPoints.size() && i < 8; i++) {
            double c = fabs(fmod(rhumbBearing(here, brgPoints[i].p) - rhumbBearing(here, brgPoints[0].p), 180.0));
            if (c > 90) c = 180 - c;
            if (c > best) { best = c; brgSel[1] = i; }
        }
    }
    lv_textarea_set_text(brgTa[4], "0");
    if (mode == 0) {
        brgT[0] = brgT[1] = tsync::now();
        brgSet(0, rhumbBearing(here, brgPoints[0].p), 1);
        brgSet(1, rhumbBearing(here, brgPoints[brgSel[1]].p), 1);
    } else {
        const LatLon p1 = plotter::rhumbDestination(here, 270, 3);
        brgT[1] = tsync::now();
        brgT[0] = brgT[1] - 1800;
        brgSet(0, rhumbBearing(p1, brgPoints[0].p), 1);
        brgSet(1, rhumbBearing(here, brgPoints[0].p), 1);
        lv_textarea_set_text(brgTa[2], "90");
        lv_textarea_set_text(brgTa[3], "6");
    }
    brgRefresh();
    brgCompute();
}

void closeBearings() { if (brgScr) lv_obj_set_hidden(brgScr, true); }

void update() {
    if (!store) return;
    uint32_t now = millis();
    sampleCurrent();
    if (rtScr && !lv_obj_is_hidden(rtScr) && now - rtRefreshMs >= 5000) { rtRefreshMs = now; rtRefresh(); }   // situacion y hora al dia
    if (curScr && !lv_obj_is_hidden(curScr) && now - curLiveMs >= 1000) { curLiveMs = now; curLiveText(); }
    sampleTrack();
    sampleWind();
    updateNav();
    pilotSend();
    if (active == 1) updateChart();
    if (active == 5) updatePilot();
    if (active == 6) updateMeteo();
    if (active == 2) updateRoutes();
    if (active == 4) updateWind();
    if (active == 7) updateSettings();
    updateNet();
    updateVessel();
    if (active == 3) updateRadar();
    checkCollision();
    checkOfficial();
    checkNavtex();
    checkDsc();
    updateModal();
    showEngine(T_RPM, Key::Rpm, 0, 1.0, false);
    showEngine(T_COOL, Key::CoolantTemp, 0, 1.0, true);
    showEngine(T_OILP, Key::OilPress, 1, 0.01, false);       // kPa -> bar
    showEngine(T_FUEL, Key::FuelLevel, 0, 1.0, false);
    showEngine(T_BATT, Key::BatteryV, 1, 1.0, false);
    showEngine(T_RATE, Key::FuelRate, 1, 1.0, false);
    showEngine(T_OILT, Key::OilTemp, 0, 1.0, true);
    showEngine(T_HOURS, Key::EngineHours, 0, 1.0, false);
    show(T_SOG, Key::Sog, 1);
    show(T_COG, Key::Cog, 0, true);
    show(T_HDG, Key::Heading, 0, true);
    show(T_DEPTH, Key::Depth, 1);
    show(T_AWS, Key::Aws, 1);
    show(T_WATER, Key::WaterTemp, 1);

    // Angulo aparente: babor / estribor
    double awa;
    if (store->get(Key::Awa, now, awa)) {
        char b[24];
        snprintf(b, sizeof(b), "%d\xC2\xB0", (int)lround(fabs(awa)));
        lv_label_set_text(tiles[T_AWA].value, b);
        lv_label_set_text(tiles[T_AWA].sub, awa < 0 ? "Por babor" : "Por estribor");
    } else {
        lv_label_set_text(tiles[T_AWA].value, "--");
        lv_label_set_text(tiles[T_AWA].sub, "Sin datos");
    }

    // Viento real: calculado con el aparente, la velocidad y el rumbo; si falta el sensor, el que llegue de otra fuente
    {
        WindNow w = windNow();
        lv_label_set_text(tileUnit[T_TWS], plotter::label(U.windSpeed));
        if (w.tru) {
            char b[24], s[72];
            bool inet = !w.computed && store->source(Key::Tws, now) == Source::Internet;
            snprintf(b, sizeof(b), "%.1f", speedFromKn(w.tws, U.windSpeed));
            for (char* p = b; *p; p++) if (*p == '.') *p = ',';
            lv_label_set_text(tiles[T_TWS].value, b);
            if (w.truDir) snprintf(s, sizeof(s), "De %03d\xC2\xB0 \xC2\xB7 fuerza %d \xC2\xB7 %s", (int)lround(norm360(w.twd)) % 360, beaufort(w.tws),
                                   w.computed ? "calculado" : (inet ? "internet, estimado" : sourceName(store->source(Key::Tws, now))));
            else snprintf(s, sizeof(s), "Fuerza %d", beaufort(w.tws));
            lv_label_set_text(tiles[T_TWS].sub, s);
            lv_obj_set_style_text_color(tiles[T_TWS].value, lv_color_hex(inet ? C_WARN : C_TEXT), 0);
        } else {
            lv_label_set_text(tiles[T_TWS].value, "--");
            lv_label_set_text(tiles[T_TWS].sub, "Falta viento, velocidad o rumbo");
            lv_obj_set_style_text_color(tiles[T_TWS].value, lv_color_hex(C_TEXT), 0);
        }
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
void testRs485() { settings::get().pilotOut = 1; rs485Warned = true; openModal(M_RS485, 0); }
void testInternet() {                                      // como si acabara de llegar una observacion de internet
    uint32_t now = millis();
    const double v[][2] = {{(double)Key::Tws, 14.0}, {(double)Key::Twd, 280}, {(double)Key::Gust, 21.5}, {(double)Key::Pressure, 1016},
                           {(double)Key::AirTemp, 17.4}, {(double)Key::Humidity, 72}, {(double)Key::Visibility, 5.4}};
    for (const auto& e : v) store->set((Key)(int)e[0], e[1], Source::Internet, now, Quality::Estimated);
}
void testNet() { openNet(); }
void testSetting(int what) {                               // 4 unidades, 5 mapa base
    settings::Values& v = settings::get();
    if (what == 4) { v.units = (v.units + 1) % 3; applyUnits(); }
    else if (what == 5) { v.baseMap = (v.baseMap + 1) % kBaseMapN; setBaseMap(v.baseMap); }
}

}
