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
    v.can = p.getBool("can", true);
    p.getString("ssid", v.wifiSsid, sizeof(v.wifiSsid));
    p.getString("wpass", v.wifiPass, sizeof(v.wifiPass));
    p.getString("host", v.tcpHost, sizeof(v.tcpHost));
    v.tcpKind = p.getInt("tcpKind", 0);
    v.tcpPort = p.getInt("tcpPort", 10110);
    v.udpPort = p.getInt("udpPort", 0);
    p.getString("owm", v.owmKey, sizeof(v.owmKey));
    v.alertsOn = p.getBool("alerts", true);
    p.getString("azones", v.alertZones, sizeof(v.alertZones));
    p.getString("acountry", v.alertCountry, sizeof(v.alertCountry));
    v.bleOn = p.getBool("ble", false);
    p.getString("bleAddr", v.bleAddr, sizeof(v.bleAddr));
    v.blePin = p.getInt("blePin", 123456);
    if (v.blePin < 0 || v.blePin > 999999) v.blePin = 123456;
    v.pilotOut = p.getInt("pilot", 0);
    v.pilotModel = p.getInt("pilotM", 1);
    v.units = p.getInt("units", 0);
    v.baseMap = p.getInt("map", 0);
    v.anchorOn = p.getBool("anc", false);
    v.anchorLat = p.getDouble("ancLat", 0);
    v.anchorLon = p.getDouble("ancLon", 0);
    v.anchorRadius = p.getInt("ancR", 40);
    v.anchorRode = p.getInt("ancRode", 30);
    p.getString("vName", v.vesselName, sizeof(v.vesselName));
    v.vesselMmsi = p.getUInt("vMmsi", 0);
    p.getString("vCall", v.vesselCall, sizeof(v.vesselCall));
    v.vesselType = p.getInt("vType", 0);
    v.vesselLen = p.getInt("vLen", 0);
    v.vesselBeam = p.getInt("vBeam", 0);
    v.vesselDraftDm = p.getInt("vDraft", 0);
    p.getString("licCall", v.licCall, sizeof(v.licCall));
    p.getString("licIds", v.licIds, sizeof(v.licIds));
    p.getString("licName", v.licName, sizeof(v.licName));
    p.getString("licCtry", v.licCountry, sizeof(v.licCountry));
    v.licAt = p.getUInt("licAt", 0);
    p.end();
    if (v.vesselType < 0 || v.vesselType > 16) v.vesselType = 0;
    if (v.vesselLen < 0 || v.vesselLen > 999) v.vesselLen = 0;
    if (v.vesselBeam < 0 || v.vesselBeam > 126) v.vesselBeam = 0;
    if (v.vesselDraftDm < 0 || v.vesselDraftDm > 255) v.vesselDraftDm = 0;
    if (v.serialBaud != 4800 && v.serialBaud != 38400) v.serialBaud = 38400;
    if (v.brightness < 10 || v.brightness > 100) v.brightness = 100;
    if (v.tcpKind < 0 || v.tcpKind > 3) v.tcpKind = 0;
    if (v.tcpPort < 1 || v.tcpPort > 65535) v.tcpPort = 10110;
    if (v.udpPort < 0 || v.udpPort > 65535) v.udpPort = 0;
    if (v.pilotOut < 0 || v.pilotOut > 3) v.pilotOut = 0;
    if (v.pilotModel < 0 || v.pilotModel > 1) v.pilotModel = 1;
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
    p.putBool("can", v.can);
    p.putString("ssid", v.wifiSsid);
    p.putString("wpass", v.wifiPass);
    p.putString("host", v.tcpHost);
    p.putInt("tcpKind", v.tcpKind);
    p.putInt("tcpPort", v.tcpPort);
    p.putInt("udpPort", v.udpPort);
    p.putString("owm", v.owmKey);
    p.putBool("alerts", v.alertsOn);
    p.putString("azones", v.alertZones);
    p.putString("acountry", v.alertCountry);
    p.putBool("ble", v.bleOn);
    p.putString("bleAddr", v.bleAddr);
    p.putInt("blePin", v.blePin);
    p.putInt("pilot", v.pilotOut);
    p.putInt("pilotM", v.pilotModel);
    p.putInt("units", v.units);
    p.putInt("map", v.baseMap);
    p.putBool("anc", v.anchorOn);
    p.putDouble("ancLat", v.anchorLat);
    p.putDouble("ancLon", v.anchorLon);
    p.putInt("ancR", v.anchorRadius);
    p.putInt("ancRode", v.anchorRode);
    p.putString("vName", v.vesselName);
    p.putUInt("vMmsi", v.vesselMmsi);
    p.putString("vCall", v.vesselCall);
    p.putInt("vType", v.vesselType);
    p.putInt("vLen", v.vesselLen);
    p.putInt("vBeam", v.vesselBeam);
    p.putInt("vDraft", v.vesselDraftDm);
    p.putString("licCall", v.licCall);
    p.putString("licIds", v.licIds);
    p.putString("licName", v.licName);
    p.putString("licCtry", v.licCountry);
    p.putUInt("licAt", v.licAt);
    p.end();
}

}
