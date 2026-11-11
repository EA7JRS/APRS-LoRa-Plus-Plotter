/*
 * Copyright (C) 2026 Manuel - EA7JRS
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "settings.h"

#include <Preferences.h>

namespace settings {

static Values v;

Values& get() { return v; }

void load() {
    Preferences p;
    p.begin("plotter", true);
    v.simulator = p.getBool("sim", true);
    v.serialBaud = p.getUInt("baud", 38400);
    v.brightness = p.getInt("bright", 100);
    p.end();
    if (v.serialBaud != 4800 && v.serialBaud != 38400) v.serialBaud = 38400;
    if (v.brightness < 10 || v.brightness > 100) v.brightness = 100;
}

void save() {
    Preferences p;
    p.begin("plotter", false);
    p.putBool("sim", v.simulator);
    p.putUInt("baud", v.serialBaud);
    p.putInt("bright", v.brightness);
    p.end();
}

}
