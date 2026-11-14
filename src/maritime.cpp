/*
 * Copyright (C) 2026 Manuel - EA7JRS
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "maritime.h"
#include "mtcheck.h"
#include "net.h"
#include "settings.h"
#include "vessel_link.h"
#include <Arduino.h>
#include <string>

using namespace plotter;

namespace maritime {

static bool pending = false;
static char msg[140] = "";
static char body[2048];

bool verified() {
    const settings::Values& v = settings::get();
    return mtcheck::verified((uint8_t)v.vesselMcOk, v.vesselMcPair, v.vesselMmsi, v.vesselMCall);
}
bool busy() { return pending; }

static void setOk(uint8_t src) {
    settings::Values& v = settings::get();
    v.vesselMcOk = src;
    if (src) mtcheck::pairKey(v.vesselMmsi, v.vesselMCall, v.vesselMcPair, sizeof(v.vesselMcPair));
    else v.vesselMcPair[0] = 0;
    settings::save();
    vessel_link::changed();                                     // el transponder se entera
}

bool checkMarineTraffic() {
    const settings::Values& v = settings::get();
    if (pending) return false;
    if (!v.vesselMCall[0] || v.vesselMmsi < 200000000UL) { snprintf(msg, sizeof(msg), "Escribe el MMSI y el distintivo mar\xC3\xADtimo y guarda"); return false; }
    if (!mtcheck::validKey(v.mtKey)) { snprintf(msg, sizeof(msg), "Escribe la clave de la API de MarineTraffic (40 caracteres)"); return false; }
    if (!net::requestGet(mtcheck::searchUrl(v.mtKey, v.vesselMmsi).c_str())) { snprintf(msg, sizeof(msg), "No se puede consultar ahora"); return false; }
    pending = true;
    snprintf(msg, sizeof(msg), "Consultando MarineTraffic...");
    return true;
}

void confirmMars(bool on) {
    const settings::Values& v = settings::get();
    if (on && (!v.vesselMCall[0] || v.vesselMmsi < 200000000UL)) { snprintf(msg, sizeof(msg), "Escribe el MMSI y el distintivo mar\xC3\xADtimo y guarda"); return; }
    msg[0] = 0;
    setOk(on ? mtcheck::MARS : mtcheck::NONE);
}

void loop() {
    int code = 0;
    if (!pending || !net::takeGet(code, body, sizeof(body))) return;
    pending = false;
    if (code < 0) { snprintf(msg, sizeof(msg), "Sin conexi\xC3\xB3n: conecta la WiFi (Ajustes > Red)"); return; }
    char mt[8];
    const mtcheck::Result r = mtcheck::parse(code, body, settings::get().vesselMCall, mt, sizeof(mt));
    if (r == mtcheck::MATCH) { msg[0] = 0; setOk(mtcheck::MARINETRAFFIC); return; }
    if (r == mtcheck::MISMATCH) snprintf(msg, sizeof(msg), "%s (%s)", mtcheck::resultEs(r), mt);
    else snprintf(msg, sizeof(msg), "%s", mtcheck::resultEs(r));
    if (r == mtcheck::MISMATCH && settings::get().vesselMcOk == mtcheck::MARINETRAFFIC) setOk(mtcheck::NONE);
}

void text(char* out, size_t size) {
    const settings::Values& v = settings::get();
    if (msg[0]) { snprintf(out, size, "%s", msg); return; }
    if (!v.vesselMCall[0]) { snprintf(out, size, "Sin distintivo mar\xC3\xADtimo: AIS lleva el CALLSIGN"); return; }
    if (verified()) { snprintf(out, size, "%s del MMSI %09lu comprobado en %s", v.vesselMCall, (unsigned long)v.vesselMmsi, mtcheck::sourceName((uint8_t)v.vesselMcOk)); return; }
    snprintf(out, size, "Distintivo sin comprobar: MarineTraffic (clave) o MARS de la UIT, itu.int/mmsapp/ShipStation/list");
}

}
