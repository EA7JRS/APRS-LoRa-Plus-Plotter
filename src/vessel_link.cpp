/*
 * Copyright (C) 2026 Manuel - EA7JRS
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "vessel_link.h"
#include "ble.h"
#include "settings.h"
#include <Arduino.h>
#include <string>

using namespace plotter;

namespace vessel_link {

static vessel::Sync st;

vessel::Data local() {
    const settings::Values& v = settings::get();
    vessel::Data d;
    snprintf(d.name, sizeof(d.name), "%s", v.vesselName);
    snprintf(d.call, sizeof(d.call), "%s", v.vesselCall);
    d.mmsi = v.vesselMmsi;
    d.type = v.vesselType;
    d.lengthM = v.vesselLen;
    d.beamM = v.vesselBeam;
    d.draftDm = v.vesselDraftDm;
    return d;
}

static void adopt(const vessel::Data& r) {
    settings::Values& v = settings::get();
    snprintf(v.vesselName, sizeof(v.vesselName), "%s", r.name);
    char call[10];                                          // sin el SSID que pone el transpondedor
    size_t k = 0;
    for (; r.call[k] && r.call[k] != '-' && k + 1 < sizeof(call); k++) call[k] = r.call[k];
    call[k] = 0;
    snprintf(v.vesselCall, sizeof(v.vesselCall), "%s", call);
    v.vesselMmsi = r.mmsi;
    v.vesselType = (r.type >= 0 && r.type < vessel::typeCount()) ? r.type : 0;
    v.vesselLen = r.lengthM;
    v.vesselBeam = r.beamM;
    v.vesselDraftDm = r.draftDm;
    settings::save();
}

static void act(vessel::Action a) {
    if (a == vessel::QUERY) ble::command(vessel::queryCommand());
    else if (a == vessel::PUSH) {
        char b[256];
        if (vessel::setCommand(local(), st.classBOnly(), b, sizeof(b))) ble::command(b);
    } else if (a == vessel::ADOPT) adopt(st.last());
}

void loop() {
    const ble::Status& bs = ble::status();
    act(st.link(bs.state == ble::CONNECTED && bs.canCommand));
    static char line[520];
    for (int i = 0; i < 4; i++) {                           // otros eventos del tracker: no se usan aqui
        if (ble::readEvent(line, sizeof(line)) <= 0) break;
        if (!strstr(line, "\"e\":\"vessel\"")) continue;
        vessel::Data r;
        bool classB = false;
        if (vessel::parseEvent(line, r, classB)) act(st.remote(r, classB, local()));
        else st.noProfile();
    }
}

void changed() { act(st.localChanged(local())); }

const vessel::Sync& sync() { return st; }

}
