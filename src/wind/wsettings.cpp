/*
 * Copyright (C) 2026 Manuel - EA7JRS
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "wsettings.h"

#include <Preferences.h>

namespace wsettings {
static Values v;
Values& get() { return v; }

void load() {
    Preferences p;
    p.begin("wind", true);
    v.simulator = p.getBool("sim", v.simulator);
    v.brightness = p.getInt("bright", v.brightness);
    v.roseStyle = p.getInt("rose", v.roseStyle);
    v.rs485Swap = p.getBool("swap", v.rs485Swap);
    v.bleOn = p.getBool("ble", v.bleOn);
    v.can = p.getBool("can", v.can);
    v.canOut = p.getBool("canOut", v.canOut);
    v.vaneOffset = p.getInt("vane", v.vaneOffset);
    v.wifiOn = p.getBool("wifi", v.wifiOn);
    p.getString("ssid", v.wifiSsid, sizeof(v.wifiSsid));
    p.getString("pass", v.wifiPass, sizeof(v.wifiPass));
    v.udpPort = p.getInt("udp", v.udpPort);
    v.night = p.getBool("night", v.night);
    v.twsMax = p.getInt("twsMax", v.twsMax);
    v.awsMax = p.getInt("awsMax", v.awsMax);
    v.shiftMax = p.getInt("shiftMax", v.shiftMax);
    p.end();
    if (v.brightness < 20 || v.brightness > 100) v.brightness = 100;
    if (v.roseStyle < 0 || v.roseStyle > 2) v.roseStyle = 2;
    if (v.vaneOffset < -180 || v.vaneOffset > 180) v.vaneOffset = 0;
    if (v.udpPort < 1 || v.udpPort > 65535) v.udpPort = 10110;
    if (v.twsMax < 0 || v.twsMax > 60) v.twsMax = 0;
    if (v.awsMax < 0 || v.awsMax > 60) v.awsMax = 0;
    if (v.shiftMax < 0 || v.shiftMax > 90) v.shiftMax = 0;
}

void save() {
    Preferences p;
    p.begin("wind", false);
    p.putBool("sim", v.simulator);
    p.putInt("bright", v.brightness);
    p.putInt("rose", v.roseStyle);
    p.putBool("swap", v.rs485Swap);
    p.putBool("ble", v.bleOn);
    p.putBool("can", v.can);
    p.putBool("canOut", v.canOut);
    p.putInt("vane", v.vaneOffset);
    p.putBool("wifi", v.wifiOn);
    p.putString("ssid", v.wifiSsid);
    p.putString("pass", v.wifiPass);
    p.putInt("udp", v.udpPort);
    p.putBool("night", v.night);
    p.putInt("twsMax", v.twsMax);
    p.putInt("awsMax", v.awsMax);
    p.putInt("shiftMax", v.shiftMax);
    p.end();
}
}
