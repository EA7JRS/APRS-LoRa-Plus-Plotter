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
    v.units = p.getInt("units", 0);
    v.baseMap = p.getInt("map", 0);
    v.anchorOn = p.getBool("anc", false);
    v.anchorLat = p.getDouble("ancLat", 0);
    v.anchorLon = p.getDouble("ancLon", 0);
    v.anchorRadius = p.getInt("ancR", 40);
    v.anchorRode = p.getInt("ancRode", 30);
    p.end();
    if (v.serialBaud != 4800 && v.serialBaud != 38400) v.serialBaud = 38400;
    if (v.brightness < 10 || v.brightness > 100) v.brightness = 100;
    if (v.units < 0 || v.units > 2) v.units = 0;
    if (v.baseMap < 0 || v.baseMap > 3) v.baseMap = 0;
    if (v.anchorRode < 5 || v.anchorRode > 200) v.anchorRode = 30;
}

void save() {
    Preferences p;
    p.begin("plotter", false);
    p.putBool("sim", v.simulator);
    p.putUInt("baud", v.serialBaud);
    p.putInt("bright", v.brightness);
    p.putInt("units", v.units);
    p.putInt("map", v.baseMap);
    p.putBool("anc", v.anchorOn);
    p.putDouble("ancLat", v.anchorLat);
    p.putDouble("ancLon", v.anchorLon);
    p.putInt("ancR", v.anchorRadius);
    p.putInt("ancRode", v.anchorRode);
    p.end();
}

}
