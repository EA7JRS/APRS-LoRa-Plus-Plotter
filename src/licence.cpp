/*
 * Copyright (C) 2026 Manuel - EA7JRS
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "licence.h"
#include "callsign.h"
#include "net.h"
#include "settings.h"
#include "tsync.h"
#include "vessel_link.h"
#include <Arduino.h>
#include <string>

using namespace plotter;

namespace licence {

static bool pending = false, recheck = false;
static uint32_t expected = 0, lastRecheckMs = 0;
static char msg[120] = "";
static char body[2048];

static callsign::Licence stored() {
    const settings::Values& v = settings::get();
    callsign::Licence l;
    snprintf(l.call, sizeof(l.call), "%s", v.licCall);
    snprintf(l.ids, sizeof(l.ids), "%s", v.licIds);
    snprintf(l.name, sizeof(l.name), "%s", v.licName);
    snprintf(l.country, sizeof(l.country), "%s", v.licCountry);
    l.atUnix = v.licAt;
    return l;
}

static void store(const callsign::Licence& l) {
    settings::Values& v = settings::get();
    snprintf(v.licCall, sizeof(v.licCall), "%s", l.call);
    snprintf(v.licIds, sizeof(v.licIds), "%s", l.ids);
    snprintf(v.licName, sizeof(v.licName), "%s", l.name);
    snprintf(v.licCountry, sizeof(v.licCountry), "%s", l.country);
    v.licAt = (uint32_t)(l.atUnix > 0 ? l.atUnix : 0);
    settings::save();
}

bool valid() { return callsign::valid(settings::get().vesselCall, stored()); }
bool busy() { return pending; }

static bool ask(uint32_t id, bool again) {
    char b[10];
    if (pending) return false;
    if (!callsign::base(settings::get().vesselCall, b, sizeof(b))) { snprintf(msg, sizeof(msg), "CALLSIGN: no es un indicativo de radioaficionado"); return false; }
    if (!net::requestGet(callsign::url(b).c_str())) { snprintf(msg, sizeof(msg), "No se puede consultar ahora"); return false; }
    pending = true;
    recheck = again;
    expected = id;
    if (!again) snprintf(msg, sizeof(msg), "Consultando RadioID.net...");
    return true;
}

bool validate(uint32_t dmrId) { return ask(dmrId, false); }

void loop() {
    int code = 0;
    if (pending && net::takeGet(code, body, sizeof(body))) {
        pending = false;
        char b[10];
        callsign::base(settings::get().vesselCall, b, sizeof(b));
        callsign::Result r;
        const bool ok = code == 200 && callsign::parse(body, b, r);
        if (recheck) {                                              // revision de los 90 dias: un fallo de red conserva lo que habia
            if (ok && !r.found) { store(callsign::Licence()); snprintf(msg, sizeof(msg), "%s ya no est\xC3\xA1 en RadioID", b); }
            else if (ok) { callsign::Licence l; if (!callsign::check(b, &r, 0, tsync::now(), l)) store(l); }
            return;
        }
        if (code < 0) { snprintf(msg, sizeof(msg), "Sin conexi\xC3\xB3n con RadioID: conecta la WiFi (Ajustes > Red)"); return; }
        if (code != 200) { snprintf(msg, sizeof(msg), "RadioID ha contestado HTTP %d", code); return; }
        callsign::Licence l;
        const char* err = callsign::check(b, ok ? &r : nullptr, expected, tsync::now(), l);
        if (err) { snprintf(msg, sizeof(msg), "%s", err); return; }
        store(l);
        msg[0] = 0;
        vessel_link::changed();                                     // ya se pueden mandar los datos al transponder
        return;
    }
    // Validado hace mas de 90 dias: otra vez, en segundo plano, como mucho una vez por hora
    const uint32_t now = millis();
    if (!pending && valid() && callsign::needsRecheck(stored(), tsync::now()) && net::status().wifi
        && (lastRecheckMs == 0 || now - lastRecheckMs > 3600000u)) {
        lastRecheckMs = now;
        ask(0, true);
    }
}

void text(char* out, size_t size) {
    const settings::Values& v = settings::get();
    if (msg[0]) { snprintf(out, size, "%s", msg); return; }
    if (valid()) {
        snprintf(out, size, "%s validado en RadioID (DMR / CCS7 %s)%s%s%s%s", v.licCall, v.licIds, v.licName[0] || v.licCountry[0] ? " \xC2\xB7 " : "",
                 v.licName, v.licName[0] && v.licCountry[0] ? ", " : "", v.licCountry);
        return;
    }
    snprintf(out, size, "Sin validar: no se mandan los datos al transponder. Escribe tu DMR ID / CCS7 y pulsa Validar");
}

}
