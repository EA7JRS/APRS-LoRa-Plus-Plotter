/* Copyright (C) 2026 Manuel - EA7JRS
 *
 * This file is part of APRS LoRa+ Plotter.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
// Pruebas del nucleo en el PC: tools/run_host_tests.sh
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>

#include "ais.h"
#include "astro.h"
#include "geo.h"
#include "gpx.h"
#include "motion.h"
#include "nmea.h"
#include "store.h"
#include "inputs.h"
#include "route.h"
#include "signalk.h"
#include "stream.h"
#include "units.h"
#include "wind.h"

using namespace plotter;

static int total = 0, failed = 0;
#define CHECK(c) do { total++; if (!(c)) { failed++; std::printf("FALLO %s:%d  %s\n", __FILE__, __LINE__, #c); } } while (0)
#define NEAR(a, b, tol) do { total++; double _a = (a), _b = (b); if (!(std::fabs(_a - _b) <= (tol))) { failed++; std::printf("FALLO %s:%d  %s = %.6f, esperado %.6f +-%g\n", __FILE__, __LINE__, #a, _a, _b, double(tol)); } } while (0)

static double dms(double d, double m, double s) { return d + m / 60 + s / 3600; }

static std::string sentence(const std::string& body) {       // anade '$' o '!' y la suma
    char cs[8];
    std::snprintf(cs, sizeof cs, "*%02X", nmeaChecksum(body.c_str() + 1));
    return body + cs;
}

static void testGeo() {
    // Vincenty: Flinders Peak - Buninyong (caso de referencia de Geoscience Australia)
    LatLon fp{-dms(37, 57, 3.72030), dms(144, 25, 29.52440)}, bu{-dms(37, 39, 10.15610), dms(143, 55, 35.38390)};
    double m, brg;
    CHECK(vincentyInverse(fp, bu, m, brg));
    NEAR(m, 54972.271, 0.01);
    NEAR(brg, dms(306, 52, 5.37), 0.0001);
    CHECK(vincentyInverse(fp, fp, m, brg));
    NEAR(m, 0, 1e-9);

    // Circulo maximo: Land's End - John o' Groats (968,9 km, demora inicial 009 07 11)
    LatLon le{dms(50, 3, 59), -dms(5, 42, 53)}, jg{dms(58, 38, 38), -dms(3, 4, 12)};
    NEAR(gcDistanceNm(le, jg) * 1.852, 968.9, 0.3);
    NEAR(gcInitialBearing(le, jg), dms(9, 7, 11), 0.01);
    LatLon back = gcDestination(le, gcInitialBearing(le, jg), gcDistanceNm(le, jg));
    NEAR(back.lat, jg.lat, 1e-6);
    NEAR(back.lon, jg.lon, 1e-6);

    // Loxodromica: Plymouth - Boston (unos 5198 km, rumbo 260 07 38)
    LatLon pl{dms(50, 21, 59), -dms(4, 8, 2)}, bo{dms(42, 21, 4), -dms(71, 2, 27)};
    NEAR(rhumbDistanceNm(pl, bo) * 1.852, 5198, 6);
    NEAR(rhumbBearing(pl, bo), dms(260, 7, 38), 0.02);
    NEAR(rhumbBearing({10, 0}, {10, 5}), 90, 1e-9);                       // paralelo
    NEAR(rhumbDistanceNm({0, 179}, {0, -179}), 2 * 60.0 * kEarthRadiusM * kPi / 180 / 60 / kMetersPerNm, 0.01);   // cruce del meridiano 180

    // Desvio de ruta: tramo sobre el ecuador hacia el este, punto 1 grado al sur = a estribor
    double xte, along;
    crossTrack({0, 0}, {0, 10}, {-1, 5}, xte, along);
    NEAR(xte, kEarthRadiusM * kPi / 180 / kMetersPerNm, 0.01);
    NEAR(along, 5 * kEarthRadiusM * kPi / 180 / kMetersPerNm, 0.05);
    crossTrack({0, 0}, {0, 10}, {1, 5}, xte, along);
    CHECK(xte < 0);
    NEAR(norm360(-10), 350, 1e-12);
    NEAR(norm180(190), -170, 1e-12);
}

static void testMotion() {
    double c, t;
    // De vuelta encontrada: 10 millas, 10 nudos cada uno
    CHECK(cpa({{0, 0}, 10, 0}, {{10.0 / 60.0, 0}, 10, 180}, c, t));
    NEAR(c, 0, 0.02);
    NEAR(t, 30, 0.1);
    // Cruce en angulo recto: minimo a los 30 min, 7,07 millas
    CHECK(cpa({{0, 0}, 10, 0}, {{0, 10.0 / 60.0}, 10, 270}, c, t));
    NEAR(c, 7.071, 0.02);
    NEAR(t, 30, 0.1);
    // Mismo rumbo y velocidad: sin movimiento relativo
    CHECK(!cpa({{0, 0}, 10, 0}, {{0, 0.1}, 10, 0}, c, t));
    // Alejandose
    CHECK(cpa({{0, 0}, 5, 180}, {{0.1, 0}, 5, 0}, c, t));
    CHECK(t < 0);

    double twa, tws, twd;
    trueWind(0, 10, 6, 90, twa, tws, twd);
    NEAR(tws, 4, 1e-9); NEAR(twa, 0, 1e-9); NEAR(twd, 90, 1e-9);
    trueWind(180, 4, 6, 0, twa, tws, twd);
    NEAR(tws, 10, 1e-9); NEAR(std::fabs(twa), 180, 1e-9);
    trueWind(38, 14.8, 6.2, 247, twa, tws, twd);               // el ejemplo de la maqueta
    NEAR(tws, 10.6, 0.05); NEAR(twa, 59, 0.5); NEAR(twd, 306, 0.5);
    double awa, aws;
    apparentWind(twd, tws, 6.2, 247, awa, aws);                // ida y vuelta
    NEAR(awa, 38, 1e-6); NEAR(aws, 14.8, 1e-6);
    trueWind(-90, 10, 0, 10, twa, tws, twd);                   // parado, viento por babor
    NEAR(twd, 280, 1e-9);

    NEAR(vmgToWind(6.2, 59), 3.19, 0.01);
    NEAR(vmgToWaypoint(6, 90, 150), 3, 1e-9);
    double set, drift;
    current(0, 5, 0, 6, set, drift);                           // corriente a favor de 1 nudo
    NEAR(drift, 1, 1e-9); NEAR(set, 0, 1e-9);
    current(0, 5, 11.3099, 5.0990, set, drift);                // 1 nudo hacia el este
    NEAR(drift, 1, 0.001); NEAR(set, 90, 0.1);
}

static void testWind() {
    CHECK(beaufort(0.4) == 0); CHECK(beaufort(1) == 1); CHECK(beaufort(3.4) == 1); CHECK(beaufort(6) == 2); CHECK(beaufort(10.4) == 3);
    CHECK(beaufort(10.6) == 4); CHECK(beaufort(14.8) == 4); CHECK(beaufort(21) == 5); CHECK(beaufort(34) == 8); CHECK(beaufort(63) == 11);
    CHECK(beaufort(64) == 12); CHECK(beaufort(120) == 12); CHECK(beaufort(-3) == 0);
    CHECK(!std::strcmp(beaufortNameEs(4), "Bonancible"));

    PulseAnemometer an(1.2);
    uint64_t t = 1000000;
    CHECK(an.knots(t) == 0);                                   // sin pulsos
    for (int i = 0; i < 40; i++) { CHECK(an.pulse(t)); t += 100000; }      // 10 Hz
    NEAR(an.knots(t), 12.0, 0.01);
    CHECK(an.pulse(t)); CHECK(!an.pulse(t + 2000)); CHECK(an.pulse(t + 6000));     // rebote a 2 ms: descartado; a 6 ms: valido
    NEAR(an.gustKnots(), 12.0, 0.01);
    CHECK(an.knots(t + 6000 + 6000000) == 0);                  // calma tras 5 s sin pulsos
    PulseAnemometer slow(1.2);
    slow.pulse(0); slow.pulse(2000000); slow.pulse(4000000);   // 0,5 Hz
    NEAR(slow.knots(4100000), 0.6, 0.01);
    CHECK(slow.pulseCount() == 3);
}

static void testUnits() {
    Units user{DistUnit::Nm, SpeedUnit::Kn, SpeedUnit::Beaufort, DepthUnit::Fathom, DepthUnit::M, TempUnit::C, PressUnit::Mb, VolUnit::L};
    Units n = unitsFor(UnitSystem::Nautical, user), me = unitsFor(UnitSystem::Metric, user), im = unitsFor(UnitSystem::Imperial, user), us = unitsFor(UnitSystem::User, user);
    CHECK(n.dist == DistUnit::Nm && n.depth == DepthUnit::M && n.windSpeed == SpeedUnit::Kn);
    CHECK(me.dist == DistUnit::Km && me.windSpeed == SpeedUnit::Ms);
    CHECK(im.depth == DepthUnit::Ft && im.temp == TempUnit::F && im.press == PressUnit::InHg);
    CHECK(us.windSpeed == SpeedUnit::Beaufort && us.depth == DepthUnit::Fathom);
    NEAR(distFromNm(1, DistUnit::Km), 1.852, 1e-12);
    NEAR(distFromNm(1, DistUnit::Mi), 1.150779, 1e-6);
    NEAR(speedFromKn(10, SpeedUnit::Ms), 5.14444, 1e-5);
    NEAR(speedFromKn(10, SpeedUnit::Kmh), 18.52, 1e-12);
    NEAR(speedFromKn(14.8, SpeedUnit::Beaufort), 4, 0);
    NEAR(depthFromM(10, DepthUnit::Ft), 32.8084, 1e-4);
    NEAR(depthFromM(1.8288, DepthUnit::Fathom), 1, 1e-12);
    NEAR(tempFromC(100, TempUnit::F), 212, 1e-12);
    NEAR(pressFromHpa(1013.25, PressUnit::InHg), 29.9213, 1e-4);
    NEAR(pressFromHpa(1013.25, PressUnit::Mb), 1013.25, 0);
    NEAR(volFromL(3.785411784, VolUnit::GalUs), 1, 1e-12);
    CHECK(!std::strcmp(label(SpeedUnit::Beaufort), "Bft") && !std::strcmp(label(DepthUnit::Fathom), "brazas"));
}

static void testAstro() {
    CHECK(dayOfYear(2026, 11, 11) == 315); CHECK(dayOfYear(2024, 3, 1) == 61); CHECK(dayOfYear(2026, 1, 1) == 1);
    // Ecuador en el equinoccio: unas 12 h de sol y mediodia cerca de las 12 UTC en el meridiano 0
    double r, s;
    CHECK(sunTimes(2026, 3, 20, 0, 0, -0.833, r, s));
    NEAR(s - r, 12 * 60 + 7, 3);
    NEAR(solarNoonUtcMin(2026, 3, 20, 0), 12 * 60 + 7.5, 1.5);
    // Cadiz, 11 de noviembre: coherencia interna (orto < mediodia < ocaso, dia de unas 10 h 20 min)
    CHECK(sunTimes(2026, 11, 11, 36.53, -6.28, -0.833, r, s));
    NEAR(s - r, 10 * 60 + 21, 4);
    NEAR((r + s) / 2, solarNoonUtcMin(2026, 11, 11, -6.28), 1e-9);
    double r2, s2;
    CHECK(sunTimes(2026, 11, 11, 36.53, -6.28, -12, r2, s2));
    CHECK(r2 < r && s2 > s);
    // Noche polar y sol de medianoche
    CHECK(!sunTimes(2026, 12, 21, 80, 0, -0.833, r, s));
    CHECK(!sunTimes(2026, 6, 21, 80, 0, -0.833, r, s));
    // Al mediodia solar el sol esta al sur y a 90 - lat + dec
    double alt, az, noon = solarNoonUtcMin(2026, 11, 11, -6.28) / 60;
    sunPosition(2026, 11, 11, noon, 36.53, -6.28, alt, az);
    NEAR(az, 180, 0.2);
    NEAR(alt, 90 - 36.53 - 17.3, 0.3);
    // Luna: nueva el 9 de noviembre de 2026 hacia las 07 UTC; llena unos 14,8 dias despues
    double age = moonAgeDays(2026, 11, 9, 7);                  // el mes sinodico medio se desvia hasta unas 14 h
    CHECK(age < 0.7 || age > 29.530588853 - 0.7);
    NEAR(moonAgeDays(2026, 11, 11, 12) - moonAgeDays(2026, 11, 10, 12), 1, 1e-6);
    NEAR(moonIllumination(0), 0, 1e-12);
    NEAR(moonIllumination(29.530588853 / 2), 1, 1e-12);
}

static void testStore() {
    DataStore st;
    double v;
    CHECK(!st.get(Key::Sog, 0, v));
    CHECK(st.source(Key::Sog, 0) == Source::None);
    CHECK(st.set(Key::Aws, 11, Source::Internet, 1000, Quality::Estimated));
    CHECK(st.get(Key::Aws, 2000, v) && v == 11 && st.quality(Key::Aws) == Quality::Estimated);
    CHECK(st.set(Key::Aws, 14.8, Source::Sensor, 3000));            // una fuente mejor sustituye
    CHECK(!st.set(Key::Aws, 9, Source::Internet, 4000));            // una peor no pisa a la vigente
    CHECK(!st.set(Key::Aws, 9, Source::Tcp, 4000));
    CHECK(st.get(Key::Aws, 5000, v) && v == 14.8 && st.source(Key::Aws, 5000) == Source::Sensor);
    CHECK(!st.get(Key::Aws, 3000 + 10001, v));                      // el sensor se calla: caduca
    CHECK(st.set(Key::Aws, 9, Source::Internet, 14000));            // y entra la siguiente
    CHECK(st.get(Key::Aws, 14000 + 7200000, v) && !st.get(Key::Aws, 14000 + 7200001, v));
    CHECK(st.set(Key::Sog, 6, Source::Can, 100) && st.set(Key::Sog, 6.2, Source::Can, 200));   // misma fuente: actualiza
    CHECK(st.set(Key::Depth, 5, Source::Serial, 0xFFFFFFF0u) && st.get(Key::Depth, 5, v));     // desbordamiento de millis()
    st.setTimeout(Source::Can, 500);
    CHECK(!st.get(Key::Sog, 800, v));
    CHECK(st.ageMs(Key::Sog, 800) == 600);
}

static void testNmea() {
    CHECK(nmeaChecksumOk("$GPRMC,123519,A,4807.038,N,01131.000,E,022.4,084.4,230394,003.1,W*6A"));
    CHECK(!nmeaChecksumOk("$GPRMC,123519,A,4807.038,N,01131.000,E,022.4,084.4,230394,003.1,W*6B"));
    CHECK(!nmeaChecksumOk("$GPRMC,123519,A") && !nmeaChecksumOk("GPRMC*00") && !nmeaChecksumOk(nullptr));

    Nmea0183 nm;
    DataStore st;
    double v;
    CHECK(nm.feed("$GPRMC,123519,A,4807.038,N,01131.000,E,022.4,084.4,230394,003.1,W*6A", Source::Serial, 10, st) == NmeaResult::Data);
    CHECK(st.get(Key::Lat, 10, v)); NEAR(v, 48.1173, 1e-4);
    CHECK(st.get(Key::Lon, 10, v)); NEAR(v, 11.516667, 1e-5);
    CHECK(st.get(Key::Sog, 10, v)); NEAR(v, 22.4, 1e-9);
    CHECK(st.get(Key::Cog, 10, v)); NEAR(v, 84.4, 1e-9);
    CHECK(nm.feed(sentence("$GPRMC,123519,V,,,,,,,230394,,").c_str(), Source::Serial, 10, st) == NmeaResult::Ignored);   // sin fix
    CHECK(nm.feed(sentence("$GPGLL,3631.84,N,00617.02,W,112500,A").c_str(), Source::Serial, 20, st) == NmeaResult::Data);
    CHECK(st.get(Key::Lon, 20, v)); NEAR(v, -(6 + 17.02 / 60), 1e-9);
    CHECK(nm.feed(sentence("$SDDPT,11.9,0.5").c_str(), Source::Serial, 20, st) == NmeaResult::Data);
    CHECK(st.get(Key::Depth, 20, v)); NEAR(v, 12.4, 1e-9);
    CHECK(nm.feed(sentence("$SDDBT,40.7,f,12.4,M,6.8,F").c_str(), Source::Serial, 20, st) == NmeaResult::Data);
    CHECK(nm.feed(sentence("$WIMWV,38.0,R,14.8,N,A").c_str(), Source::Serial, 20, st) == NmeaResult::Data);
    CHECK(st.get(Key::Awa, 20, v)); NEAR(v, 38, 1e-9);
    CHECK(st.get(Key::Aws, 20, v)); NEAR(v, 14.8, 1e-9);
    CHECK(nm.feed(sentence("$WIMWV,322.0,R,7.6,M,A").c_str(), Source::Serial, 30, st) == NmeaResult::Data);
    CHECK(st.get(Key::Awa, 30, v)); NEAR(v, -38, 1e-9);                         // babor
    CHECK(st.get(Key::Aws, 30, v)); NEAR(v, 14.773, 0.001);                     // m/s a nudos
    CHECK(nm.feed(sentence("$WIMWV,38.0,R,14.8,N,V").c_str(), Source::Serial, 30, st) == NmeaResult::Ignored);   // dato no valido
    CHECK(nm.feed(sentence("$WIMWV,59.0,T,10.6,N,A").c_str(), Source::Serial, 30, st) == NmeaResult::Ignored);
    CHECK(nm.feed(sentence("$HEHDT,247.0,T").c_str(), Source::Serial, 30, st) == NmeaResult::Data);
    CHECK(nm.feed(sentence("$VWVHW,247.0,T,249.0,M,5.9,N,10.9,K").c_str(), Source::Serial, 30, st) == NmeaResult::Data);
    CHECK(st.get(Key::Stw, 30, v)); NEAR(v, 5.9, 1e-9);
    CHECK(nm.feed(sentence("$YXMTW,18.6,C").c_str(), Source::Serial, 30, st) == NmeaResult::Data);
    CHECK(nm.feed(sentence("$GPVTG,247.0,T,249.0,M,6.2,N,11.5,K,A").c_str(), Source::Serial, 30, st) == NmeaResult::Data);
    CHECK(nm.feed(sentence("$GPZDA,112500,11,11,2026,00,00").c_str(), Source::Serial, 30, st) == NmeaResult::Ignored);
    CHECK(nm.feed("basura", Source::Serial, 30, st) == NmeaResult::Bad);

    // AIS: ejemplos de la documentacion AIVDM/AIVDO de gpsd
    CHECK(nm.feed("!AIVDM,1,1,,B,177KQJ5000G?tO`K>RA1wUbN0TKH,0*5C", Source::Serial, 40, st) == NmeaResult::Ais);
    const AisReport& a = nm.ais();
    CHECK(a.type == 1 && a.mmsi == 477553000 && a.navStatus == 5 && !a.own);
    CHECK(a.hasPos); NEAR(a.lat, 47.582833, 1e-5); NEAR(a.lon, -122.345833, 1e-5);
    CHECK(a.hasSog && a.sogKn == 0); CHECK(a.hasCog); NEAR(a.cogDeg, 51.0, 1e-9); CHECK(a.hasHeading && a.heading == 181);
    // Mensaje 5 en dos fragmentos
    CHECK(nm.feed("!AIVDM,2,1,3,B,55P5TL01VIaAL@7WKO@mBplU@<PDhh000000001S;AJ::4A80?4i@E53,0*3E", Source::Serial, 50, st) == NmeaResult::Ignored);
    CHECK(nm.feed("!AIVDM,2,2,3,B,1@0000000000000,2*55", Source::Serial, 50, st) == NmeaResult::Ais);
    CHECK(nm.ais().type == 5 && nm.ais().mmsi == 369190000 && nm.ais().hasStatic);
    CHECK(!std::strcmp(nm.ais().name, "MT.MITCHELL"));
    CHECK(!std::strcmp(nm.ais().callsign, "WDA9674"));
    // Segundo fragmento suelto: se descarta
    CHECK(nm.feed("!AIVDM,2,2,3,B,1@0000000000000,2*55", Source::Serial, 60, st) == NmeaResult::Ignored);
    // VDO = barco propio
    CHECK(nm.feed(sentence("!AIVDO,1,1,,B,177KQJ5000G?tO`K>RA1wUbN0TKH,0").c_str(), Source::Serial, 70, st) == NmeaResult::Ais);
    CHECK(nm.ais().own);
}

static void testAisExtra() {
    Nmea0183 nm;
    DataStore st;
    auto feed = [&](const char* body) { return nm.feed(sentence(body).c_str(), Source::Serial, 1, st); };
    // Ejemplos de la documentacion AIVDM/AIVDO de gpsd
    CHECK(feed("!AIVDM,1,1,,A,403OviQuMGCqWrRO9>E6fE700@GO,0") == NmeaResult::Ais);            // 4: estacion base
    CHECK(nm.ais().type == 4 && nm.ais().kind == AisKind::BaseStation && nm.ais().mmsi == 3669702);
    NEAR(nm.ais().lat, 36.883767, 1e-5); NEAR(nm.ais().lon, -76.352362, 1e-5);
    CHECK(feed("!AIVDM,1,1,,B,91b55wi;hbOS@OdQAC062Ch2089h,0") == NmeaResult::Ais);            // 9: aeronave SAR
    CHECK(nm.ais().kind == AisKind::SarAircraft && nm.ais().mmsi == 111232511 && nm.ais().hasAlt && nm.ais().altM == 303);
    CHECK(nm.ais().hasSog && nm.ais().sogKn == 42); NEAR(nm.ais().cogDeg, 154.5, 1e-9);
    NEAR(nm.ais().lat, 58.144, 1e-5); NEAR(nm.ais().lon, -6.2788383, 1e-5);
    CHECK(feed("!AIVDM,1,1,,A,>5?Per18=HB1U:1@E=B0m<L,2") == NmeaResult::Ais);                 // 14: mensaje de seguridad
    CHECK(nm.ais().kind == AisKind::Safety && nm.ais().mmsi == 351809000 && !std::strcmp(nm.ais().text, "RCVD YR TEST MSG"));
    CHECK(feed("!AIVDM,2,1,5,B,E1mg=5J1T4W0h97aRh6ba84<h2d;W:Te=eLvH50```q,0") == NmeaResult::Ignored);   // 21: ayuda a la navegacion
    CHECK(feed("!AIVDM,2,2,5,B,:D44QDlp0C1DU00,2") == NmeaResult::Ais);
    CHECK(nm.ais().kind == AisKind::Aton && nm.ais().mmsi == 123456789 && nm.ais().atonType == 20 && !nm.ais().atonVirtual);
    CHECK(!std::strcmp(nm.ais().name, "CHINA ROSE MURPHY EXPRESS ALERT"));                     // nombre con ampliacion
    NEAR(nm.ais().lat, 47.920618, 1e-5); NEAR(nm.ais().lon, -122.698592, 1e-5);
    CHECK(nm.ais().hasDim && nm.ais().toBow == 5 && nm.ais().toStarboard == 5);
    CHECK(feed("!AIVDM,1,1,,B,KC5E2b@U19PFdLbMuc5=ROv62<7m,0") == NmeaResult::Ais);            // 27: largo alcance (ejemplo con bits de mas)
    CHECK(nm.ais().type == 27 && nm.ais().mmsi == 206914217 && nm.ais().oversized);
    NEAR(nm.ais().lat, 4.84, 1e-9); NEAR(nm.ais().lon, 137.023333, 1e-5);
    CHECK(nm.ais().sogKn == 57 && nm.ais().cogDeg == 167);
    CHECK(feed("!AIVDM,1,1,,B,H42O55lti4hhhilD3nink000?050,0") == NmeaResult::Ais);            // 24 parte B: dimensiones
    CHECK(!std::strcmp(nm.ais().callsign, "TC6163") && nm.ais().hasDim && nm.ais().toStern == 15 && nm.ais().toStarboard == 5);
    // Mensaje 5: dimensiones, calado y destino
    feed("!AIVDM,2,1,3,B,55P5TL01VIaAL@7WKO@mBplU@<PDhh000000001S;AJ::4A80?4i@E53,0");
    CHECK(feed("!AIVDM,2,2,3,B,1@0000000000000,2") == NmeaResult::Ais);
    CHECK(nm.ais().hasDim && nm.ais().toBow == 90 && nm.ais().toStern == 90 && nm.ais().toPort == 10 && nm.ais().toStarboard == 10);
    CHECK(nm.ais().hasVoyage); NEAR(nm.ais().draughtM, 6.0, 1e-9); CHECK(!std::strcmp(nm.ais().destination, "SEATTLE"));

    // Calidad: corto, MMSI 0, tipo sin uso, suma mal
    NmeaStats before = nm.stats();
    CHECK(feed("!AIVDM,1,1,,B,177KQJ5000G?tO`K,0") == NmeaResult::Ignored);
    CHECK(feed("!AIVDM,1,1,,B,1000000000G?tO`K>RA1wUbN0TKH,0") == NmeaResult::Ignored);
    CHECK(nm.stats().aisRejected == before.aisRejected + 2);
    CHECK(feed("!AIVDM,1,1,,B,:5?Per18=HB1U:1@E=B0m<L,0") == NmeaResult::Ignored);             // tipo 10
    CHECK(nm.stats().aisUnsupported == before.aisUnsupported + 1);
    CHECK(nm.feed("!AIVDM,1,1,,B,177KQJ5000G?tO`K>RA1wUbN0TKH,0*5D", Source::Serial, 1, st) == NmeaResult::Bad);
    CHECK(nm.stats().badChecksum == before.badChecksum + 1);
    CHECK(nm.stats().aisOversized >= 1 && nm.stats().good > 8);

    // Bloque de etiquetas delante de la trama
    const char* tagged = "\\s:EA7JRS-1,c:1794415252*00\\!AIVDM,1,1,,B,177KQJ5000G?tO`K>RA1wUbN0TKH,0*5C";
    CHECK(!std::strcmp(nmeaSentenceStart(tagged), "!AIVDM,1,1,,B,177KQJ5000G?tO`K>RA1wUbN0TKH,0*5C"));
    CHECK(nm.feed(tagged, Source::Tcp, 1, st) == NmeaResult::Ais && nm.ais().mmsi == 477553000);
    CHECK(nm.tagTime() == 1794415252u && !std::strcmp(nm.tagSource(), "EA7JRS-1"));
    CHECK(nm.feed("\\c:1794415252123*00\\!AIVDM,1,1,,B,177KQJ5000G?tO`K>RA1wUbN0TKH,0*5C", Source::Tcp, 1, st) == NmeaResult::Ais);
    CHECK(nm.tagTime() == 1794415252u && nm.tagSource()[0] == 0);                               // hora en milisegundos
    CHECK(nm.feed("!AIVDM,1,1,,B,177KQJ5000G?tO`K>RA1wUbN0TKH,0*5C", Source::Tcp, 1, st) == NmeaResult::Ais && nm.tagTime() == 0);
    CHECK(nm.feed("\\sin cierre", Source::Tcp, 1, st) == NmeaResult::Bad);
    const char* plain = "$GPGLL,3631.84,N,00617.02,W,112500,A*3A";
    CHECK(nmeaSentenceStart(plain) == plain);

    // Posiciones: saltos imposibles
    double la = 36.53, lo = -6.30;
    CHECK(aisJudgePosition(false, 0, 0, false, 10, la, lo, false) == AisPosVerdict::Unknown);
    CHECK(aisJudgePosition(true, la, lo, false, 60, la + 0.01, lo, false) == AisPosVerdict::Valid);       // 0,6 mn en 1 min
    CHECK(aisJudgePosition(true, la, lo, false, 60, la + 0.1, lo, false) == AisPosVerdict::Jump);         // 6 mn en 1 min
    CHECK(aisJudgePosition(true, la, lo, false, 60, la + 0.1, lo, true) == AisPosVerdict::Valid);         // una aeronave si puede
    CHECK(aisJudgePosition(true, la, lo, true, 60, la + 0.005, lo, false) == AisPosVerdict::Jump);        // parado y aparece a 0,3 mn
    CHECK(aisJudgePosition(true, la, lo, true, 60, la + 0.0005, lo, false) == AisPosVerdict::Valid);      // borneo en el fondeo
    CHECK(aisJudgePosition(true, la, lo, false, 3600, la + 5, lo, false) == AisPosVerdict::Unknown);      // la anterior es vieja
    CHECK(aisJudgePosition(true, la, lo, false, -60, la + 0.01, lo, false) == AisPosVerdict::Valid);      // hora desordenada
}

static void testInputs() {
    // Cuentavueltas: 2 pulsos por vuelta a 100 Hz = 3000 rpm
    PulseInput tach(2.0, 2000);
    uint64_t t = 1000000;
    for (int i = 0; i < 200; i++) { CHECK(tach.pulse(t)); t += 10000; }
    NEAR(tach.rpm(t), 3000.0, 1.0);
    CHECK(tach.pulses() == 200); NEAR(tach.revolutions(), 100.0, 1e-9);
    CHECK(!tach.pulse(t - 10000 + 500));                             // rebote
    tach.resetCount();
    CHECK(tach.pulses() == 0);
    CHECK(tach.rpm(t + 6000000) == 0);                               // parado
    // Corredera de 20000 pulsos por milla: 5,5556 Hz = 1 nudo
    NEAR(logSpeedKn(20000.0 / 3600.0, 20000), 1.0, 1e-9);
    NEAR(logSpeedKn(33.3333333, 20000), 6.0, 1e-6);
    NEAR(logDistanceNm(50000, 20000), 2.5, 1e-12);
    CHECK(logSpeedKn(10, 0) == 0);
    // Rueda de 4 cm de radio a 5 vueltas por segundo: 1,2566 m/s; 100 vueltas = 25,13 m
    NEAR(wheelSpeedMs(5, 0.04), 1.256637, 1e-6);
    NEAR(wheelDistanceM(100, 0.04, 1.1), 27.646015, 1e-5);

    // Interruptor de sentina a masa, 50 ms de filtro
    DigitalInput bilge(true, 50);
    CHECK(!bilge.update(true, 0) && !bilge.active());
    CHECK(!bilge.update(false, 100));                                // empieza a cerrar
    CHECK(!bilge.update(true, 120));                                 // rebote
    CHECK(!bilge.update(false, 130));
    CHECK(!bilge.update(false, 170) && !bilge.active());
    CHECK(bilge.update(false, 180) && bilge.active());
    CHECK(!bilge.update(false, 500) && bilge.sinceMs(500) == 320);
    CHECK(!bilge.update(true, 600));
    CHECK(bilge.update(true, 650) && !bilge.active());
    DigitalInput door(false, 0);                                     // activo en alto, sin filtro
    door.update(false, 0);
    CHECK(door.update(true, 1) && door.active());

    // CRC de Dallas: ejemplo de la nota de aplicacion 27 de Maxim
    const uint8_t rom[] = {0x02, 0x1C, 0xB8, 0x01, 0x00, 0x00, 0x00};
    CHECK(dallasCrc8(rom, 7) == 0xA2);
    // DS18B20: valores de la tabla de la hoja de datos
    auto sp = [](uint16_t raw, uint8_t cfg) {
        static uint8_t b[9];
        uint8_t v[9] = {uint8_t(raw & 0xFF), uint8_t(raw >> 8), 0x4B, 0x46, cfg, 0xFF, 0x0C, 0x10, 0};
        for (int i = 0; i < 8; i++) b[i] = v[i];
        b[8] = dallasCrc8(b, 8);
        return b;
    };
    double c = 0;
    CHECK(ds18b20Celsius(sp(0x0191, 0x7F), c)); NEAR(c, 25.0625, 1e-9);
    CHECK(ds18b20Celsius(sp(0x07D0, 0x7F), c)); NEAR(c, 125.0, 1e-9);
    CHECK(ds18b20Celsius(sp(0xFF5E, 0x7F), c)); NEAR(c, -10.125, 1e-9);
    CHECK(ds18b20Celsius(sp(0xFC90, 0x7F), c)); NEAR(c, -55.0, 1e-9);
    CHECK(ds18b20Celsius(sp(0x0191, 0x1F), c)); NEAR(c, 25.0, 1e-9);         // a 9 bits solo valen los medios grados
    uint8_t bad[9];
    for (int i = 0; i < 9; i++) bad[i] = sp(0x0191, 0x7F)[i];
    bad[0] ^= 1;
    CHECK(!ds18b20Celsius(bad, c));
    const uint8_t none[9] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}, shorted[9] = {0};
    CHECK(!ds18b20Celsius(none, c) && !ds18b20Celsius(shorted, c));

    // Niveles de Signal K
    CHECK(alertLevelFromSignalK("alarm") == AlertLevel::Alarm && alertLevelFromSignalK("emergency") == AlertLevel::Emergency);
    CHECK(alertLevelFromSignalK("otra") == AlertLevel::Normal && alertLevelFromSignalK(nullptr) == AlertLevel::Normal);
    CHECK(!std::strcmp(alertLevelSignalK(AlertLevel::Warn), "warn") && !std::strcmp(alertLevelNameEs(AlertLevel::Warn), "Aviso"));
    CHECK(!alertSounds(AlertLevel::Alert) && alertSounds(AlertLevel::Warn) && alertSounds(AlertLevel::Emergency));
    CHECK(AlertLevel::Emergency > AlertLevel::Alarm && AlertLevel::Alarm > AlertLevel::Warn);
    // Bateria: aviso por debajo de 11,8 V, se quita al pasar de 12,0 V
    ThresholdAlert batt(11.8, 15.0, 0.2, AlertLevel::Warn);
    CHECK(batt.update(12.4) == AlertLevel::Normal);
    CHECK(batt.update(11.7) == AlertLevel::Warn);
    CHECK(batt.update(11.9) == AlertLevel::Warn);                    // dentro de la histeresis sigue
    CHECK(batt.update(12.05) == AlertLevel::Normal);
    CHECK(batt.update(15.3) == AlertLevel::Warn && batt.update(14.9) == AlertLevel::Warn && batt.update(14.7) == AlertLevel::Normal);
}

static void testPilot() {
    Nmea0183 nm;
    DataStore st;
    double v = 0;
    // Lo que manda Pypilot por su servidor NMEA (TCP 20220)
    CHECK(nm.feed(sentence("$APHDM,231.500,M").c_str(), Source::Tcp, 10, st) == NmeaResult::Data);
    CHECK(st.get(Key::HeadingMag, 20, v)); NEAR(v, 231.5, 1e-9);
    CHECK(nm.feed(sentence("$APXDR,A,-3.250,D,PTCH").c_str(), Source::Tcp, 10, st) == NmeaResult::Data);
    CHECK(nm.feed(sentence("$APXDR,A,12.500,D,ROLL").c_str(), Source::Tcp, 10, st) == NmeaResult::Data);
    CHECK(st.get(Key::Pitch, 20, v)); NEAR(v, -3.25, 1e-9);
    CHECK(st.get(Key::Roll, 20, v)); NEAR(v, 12.5, 1e-9);
    CHECK(nm.feed(sentence("$IIXDR,C,19.5,C,AIR,A,4.0,D,PITCH,A,-7.0,D,HEEL").c_str(), Source::Serial, 30, st) == NmeaResult::Data);
    CHECK(st.get(Key::Roll, 40, v)); NEAR(v, -7.0, 1e-9);
    CHECK(nm.feed(sentence("$IIXDR,C,19.5,C,AIR").c_str(), Source::Serial, 30, st) == NmeaResult::Ignored);
    CHECK(nm.feed(sentence("$APRSA,-4.500,A,,").c_str(), Source::Tcp, 10, st) == NmeaResult::Data);
    CHECK(st.get(Key::RudderAngle, 20, v)); NEAR(v, -4.5, 1e-9);
    CHECK(nm.feed(sentence("$APRSA,-4.500,V,,").c_str(), Source::Tcp, 10, st) == NmeaResult::Ignored);
    // APB hacia el piloto
    char b[100];
    int n = nmeaApb(b, sizeof(b), 0.034, false, false, 271.4, "CADIZ,1*", 268.9, 265.2, true);
    CHECK(n > 0 && n == int(std::strlen(b)) && b[n - 2] == '\r' && b[n - 1] == '\n');
    std::string line(b, size_t(n - 2));
    CHECK(nmeaChecksumOk(line.c_str()));
    CHECK(line.rfind("$ECAPB,A,A,0.03,L,N,V,V,271.4,M,CADIZ1,268.9,M,265.2,M*", 0) == 0);
    n = nmeaApb(b, sizeof(b), -0.5, true, true, -10, nullptr, 370, 0, false);
    CHECK(std::string(b, size_t(n - 2)).rfind("$ECAPB,A,A,0.50,R,N,A,A,350.0,T,,10.0,T,0.0,T*", 0) == 0);
    CHECK(nmeaApb(b, 20, 0.1, false, false, 0, "A", 0, 0, true) == 0);     // no cabe
}

static void testSignalK() {
    DataStore st;
    double v = 0;
    CHECK(skApply("navigation.position.latitude", 36.5297, Source::Tcp, 10, st));
    CHECK(skApply("navigation.speedOverGround", 3.0866667, Source::Tcp, 10, st));          // 6 nudos
    CHECK(st.get(Key::Sog, 20, v)); NEAR(v, 6.0, 1e-5);
    CHECK(skApply("navigation.courseOverGroundTrue", 4.71238898, Source::Tcp, 10, st));
    CHECK(st.get(Key::Cog, 20, v)); NEAR(v, 270.0, 1e-5);
    CHECK(skApply("environment.wind.angleApparent", -0.5235988, Source::Tcp, 10, st));     // 30 grados por babor
    CHECK(st.get(Key::Awa, 20, v)); NEAR(v, -30.0, 1e-4);
    CHECK(skApply("environment.wind.angleApparent", 5.7595865, Source::Tcp, 11, st));      // lo mismo dado como 330
    CHECK(st.get(Key::Awa, 20, v)); NEAR(v, -30.0, 1e-4);
    CHECK(skApply("environment.water.temperature", 291.65, Source::Tcp, 10, st));
    CHECK(st.get(Key::WaterTemp, 20, v)); NEAR(v, 18.5, 1e-9);
    CHECK(skApply("environment.outside.pressure", 101325, Source::Tcp, 10, st));
    CHECK(st.get(Key::Pressure, 20, v)); NEAR(v, 1013.25, 1e-9);
    CHECK(skApply("environment.depth.belowTransducer", 7.4, Source::Tcp, 10, st));
    CHECK(st.get(Key::Depth, 20, v)); NEAR(v, 7.4, 1e-12);
    CHECK(skApply("propulsion.port.revolutions", 40.0, Source::Tcp, 10, st));              // 40 Hz = 2400 rpm
    CHECK(st.get(Key::Rpm, 20, v)); NEAR(v, 2400.0, 1e-9);
    CHECK(skApply("electrical.batteries.1.voltage", 12.6, Source::Tcp, 10, st));
    CHECK(st.get(Key::BatteryV, 20, v)); NEAR(v, 12.6, 1e-12);
    CHECK(skApply("navigation.attitude.roll", 0.1745329, Source::Tcp, 10, st));
    CHECK(st.get(Key::Roll, 20, v)); NEAR(v, 10.0, 1e-4);
    CHECK(!skApply("propulsion.port.oilPressure", 1, Source::Tcp, 10, st));
    CHECK(!skApply("propulsion.revolutions", 1, Source::Tcp, 10, st));
    CHECK(!skApply("propulsion.a.b.revolutions", 1, Source::Tcp, 10, st));
    CHECK(!skApply("tanks.fuel.0.currentLevel", 0.5, Source::Tcp, 10, st) && !skApply(nullptr, 1, Source::Tcp, 10, st));
    CHECK(!skApply("navigation.speedOverGround", std::nan(""), Source::Tcp, 10, st));

    char b[160];
    int n = skSubscribe(b, sizeof(b), 500);
    CHECK(n > 0 && !std::strcmp(b, "{\"context\":\"vessels.self\",\"subscribe\":[{\"path\":\"*\",\"period\":500}]}\r\n"));
    CHECK(skSubscribe(b, 10) == 0);

    CHECK(!std::strcmp(skNotificationKey("notifications.mob"), "mob"));
    CHECK(skNotificationKey("navigation.position") == nullptr && skNotificationKey("notifications.") == nullptr);
    CHECK(!std::strcmp(skNotificationTitleEs("mob"), "Hombre al agua"));
    CHECK(!std::strcmp(skNotificationTitleEs(skNotificationKey("notifications.navigation.anchor")), "Garreo"));
    CHECK(!std::strcmp(skNotificationTitleEs("electrical.batteries.house.voltage"), "Bateria"));
    CHECK(skNotificationTitleEs("otra.cosa") == nullptr);

    // Plotter como sensor: 12 nudos de viento aparente
    CHECK(!std::strcmp(skPathFor(Key::Aws), "environment.wind.speedApparent"));
    NEAR(skToSi(Key::Aws, 12.0), 6.173333, 1e-5);
    NEAR(skToSi(Key::AirTemp, 21.0), 294.15, 1e-9);
    NEAR(skToSi(Key::Awa, -30.0), -0.5235988, 1e-6);
    CHECK(skPathFor(Key::Rpm) == nullptr);
    n = skDelta(b, sizeof(b), skPathFor(Key::Aws), skToSi(Key::Aws, 12.0));
    CHECK(n > 0 && !std::strcmp(b, "{\"updates\":[{\"values\":[{\"path\":\"environment.wind.speedApparent\",\"value\":6.173333}]}]}\r\n"));
    DataStore back;                                                  // ida y vuelta
    CHECK(skApply(skPathFor(Key::Aws), skToSi(Key::Aws, 12.0), Source::Tcp, 1, back) && back.get(Key::Aws, 2, v));
    NEAR(v, 12.0, 1e-9);
}

static void testRoute() {
    // Ruta en L: al norte 1 mn y luego al este 1 mn
    const LatLon A{36.50, -6.30}, B{36.50 + 1.0 / 60, -6.30}, C = gcDestination(B, 90, 1.0);
    const LatLon pts[] = {B, C};
    auto at = [&](double northNm, double eastNm) { return gcDestination(gcDestination(A, 0, northNm), 90, eastNm); };

    RouteFollower rf;
    rf.setArrivalRadiusNm(0.05);
    rf.setMode(WpSwitch::Abeam);
    rf.start(pts, 2, A);
    const RouteStatus& s = rf.update(at(0.5, 0.02), 0);
    CHECK(s.active && s.target == 0 && !s.inArrival && !s.abeam && !s.switched);
    NEAR(s.distanceNm, 0.5004, 1e-3); NEAR(s.xteNm, 0.02, 1e-3); NEAR(s.legBearing, 0.0, 0.01);
    rf.update(at(0.97, 0.02), 1000);
    CHECK(s.inArrival && s.target == 0);                             // alarma de llegada, aun no cambia
    rf.update(at(1.01, 0.02), 2000);
    CHECK(s.switched && s.target == 1 && !s.finished);               // pasado el traves
    NEAR(s.legBearing, 90.0, 0.05); NEAR(s.distanceNm, 0.98, 2e-3);
    rf.update(at(1.0, 0.5), 3000);
    CHECK(!s.switched && s.target == 1 && !s.finished);
    rf.update(at(1.0, 0.98), 4000);
    CHECK(s.inArrival && s.finished && s.target == 1);

    // Pasa lejos del waypoint sin entrar en el circulo: por el traves cambia igual
    rf.start(pts, 2, A);
    rf.update(at(0.9, 0.3), 0);
    CHECK(s.target == 0);
    rf.update(at(1.02, 0.3), 1000);
    CHECK(s.switched && s.target == 1);

    // Pronto: 10 s despues de entrar en el circulo
    rf.setMode(WpSwitch::Early, 10);
    rf.start(pts, 2, A);
    rf.update(at(0.96, 0), 0);
    CHECK(s.inArrival && s.target == 0);
    rf.update(at(0.97, 0), 9000);
    CHECK(s.target == 0);
    rf.update(at(0.98, 0), 10000);
    CHECK(s.switched && s.target == 1);

    // Tardio: dentro del circulo, cuando la distancia vuelve a crecer
    rf.setMode(WpSwitch::Late);
    rf.start(pts, 2, A);
    rf.update(at(0.96, 0.01), 0);
    rf.update(at(0.99, 0.01), 1000);
    CHECK(s.target == 0);
    rf.update(at(1.02, 0.01), 2000);
    CHECK(s.switched && s.target == 1);
    rf.start(pts, 2, A);                                             // sin entrar en el circulo no cambia
    rf.update(at(1.1, 0.3), 0);
    CHECK(s.target == 0 && s.abeam);

    // A mano, y empezar por un waypoint intermedio
    rf.start(pts, 2, A);
    CHECK(rf.skip(A) && rf.status().target == 1 && !rf.skip(A));
    rf.start(pts, 2, A, 1);
    rf.update(at(1.0, 0.5), 0);
    CHECK(s.target == 1); NEAR(s.legBearing, 90.0, 0.05); NEAR(s.xteNm, 0.0, 2e-3);
    rf.stop();
    CHECK(!rf.update(A, 0).active);
    rf.start(nullptr, 0, A);
    CHECK(!rf.status().active);

    // Loxodromica frente a ortodromica en un tramo largo
    const LatLon far[] = {{40.0, -30.0}};
    rf.setMode(WpSwitch::Abeam);
    rf.setRhumbLine(false);
    rf.start(far, 1, LatLon{36.5, -6.3});
    double gcB = rf.update(LatLon{36.5, -6.3}, 0).bearing;
    rf.setRhumbLine(true);
    rf.start(far, 1, LatLon{36.5, -6.3});
    double rhB = rf.update(LatLon{36.5, -6.3}, 0).bearing;
    CHECK(gcB > rhB + 3 && rhB > 270 && rhB < 290);                  // la ortodromica sale mas al norte

    // Derrota: cada 10 s y solo si hay mas de 25 m
    TrackSampler ts;
    CHECK(ts.sample(A, 0));
    CHECK(!ts.sample(at(0.1, 0), 5000));                             // aun no toca
    CHECK(!ts.sample(at(0.005, 0), 10000));                          // 9 m: fondeado
    CHECK(ts.sample(at(0.02, 0), 20000));                            // 37 m
    CHECK(!ts.sample(at(0.05, 0), 25000));
    CHECK(ts.sample(at(0.05, 0), 30000));
    NEAR(ts.distanceNm(), 0.05, 1e-4);
    ts.reset();
    CHECK(ts.sample(A, 31000));

    char b[80];
    int n = depthLogLine(b, sizeof(b), "2026-11-11T17:06:00Z", LatLon{36.5297, -6.2927}, 7.44);
    CHECK(n > 0 && !std::strcmp(b, "2026-11-11T17:06:00Z,36.529700,-6.292700,7.4\n"));
    CHECK(depthLogLine(b, 10, "x", A, 1) == 0);

    CHECK(alertSound(AlertLevel::Normal).beeps == 0 && alertSound(AlertLevel::Alert).beeps == 1 && !alertSound(AlertLevel::Alert).needsAck);
    CHECK(alertSound(AlertLevel::Alarm).needsAck && alertSound(AlertLevel::Emergency).repeatS < alertSound(AlertLevel::Alarm).repeatS);
    CHECK(alertSound(AlertLevel::Emergency).beeps > alertSound(AlertLevel::Warn).beeps);
}

static void testGnss() {
    Nmea0183 nm;
    DataStore st;
    auto feed = [&](const char* body, uint32_t t) { return nm.feed(sentence(body).c_str(), Source::Serial, t, st); };
    GnssSky& sky = nm.sky();
    // GPS en 3 tramas y GLONASS en 1
    CHECK(feed("$GPGSV,3,1,11,03,03,111,00,04,15,270,00,06,01,010,00,13,06,292,00", 0) == NmeaResult::Data);
    feed("$GPGSV,3,2,11,14,25,170,00,16,57,208,39,18,67,296,40,19,40,246,00", 10);
    feed("$GPGSV,3,3,11,22,42,067,42,24,14,311,43,27,05,244,00,,,,", 20);
    feed("$GLGSV,1,1,03,65,30,100,35,66,60,200,41,88,10,330,", 30);
    CHECK(sky.count() == 14);
    CHECK(sky.sat(0).sys == GnssSystem::Gps && sky.sat(0).prn == 3 && sky.sat(0).elev == 3 && sky.sat(0).az == 111 && sky.sat(0).snr == 0);
    CHECK(sky.sat(11).sys == GnssSystem::Glonass && sky.sat(11).prn == 65 && sky.sat(13).snr == -1);
    // Usados: una GSA por constelacion
    feed("$GPGSA,A,3,16,18,22,24,,,,,,,,,3.6,2.1,2.2", 40);
    feed("$GLGSA,A,3,65,66,,,,,,,,,,,3.6,2.1,2.2", 50);
    CHECK(sky.fixMode() == 3 && sky.usedCount() == 6); NEAR(sky.hdop(), 2.1, 1e-9); NEAR(sky.pdop(), 3.6, 1e-9);
    int used = 0;
    for (int i = 0; i < sky.count(); i++) if (sky.sat(i).used) used += sky.sat(i).prn;
    CHECK(used == 16 + 18 + 22 + 24 + 65 + 66);
    // Ciclo siguiente: la lista de GPS se sustituye, la de GLONASS sigue
    feed("$GPGSV,1,1,02,16,58,209,40,18,66,297,41", 1000);
    CHECK(sky.count() == 5 && sky.sat(3).prn == 16 && sky.sat(3).used && sky.sat(3).elev == 58);
    feed("$GPGSA,A,3,16,18,,,,,,,,,,,5.0,4.0,3.0", 1040);
    CHECK(sky.usedCount() == 2);                                     // rafaga nueva: lo de antes ya no cuenta
    // GLONASS deja de llegar
    feed("$GPGSV,1,1,02,16,58,209,40,18,66,297,41", 12000);
    sky.expire(12000);
    CHECK(sky.count() == 2);
    // Emisor GN con sistema (NMEA 4.10) y senal al final; SBAS con emisor GP
    Nmea0183 n2;
    auto feed2 = [&](const char* body, uint32_t t) { return n2.feed(sentence(body).c_str(), Source::Serial, t, st); };
    feed2("$GAGSV,1,1,02,05,45,120,38,09,20,300,33,7", 0);
    feed2("$GPGSV,1,1,02,10,50,090,44,36,30,150,37,1", 5);
    feed2("$GNGSA,A,3,10,,,,,,,,,,,,1.5,0.9,1.2,1", 10);
    feed2("$GNGSA,A,3,05,09,,,,,,,,,,,1.5,0.9,1.2,3", 15);
    CHECK(n2.sky().count() == 4 && n2.sky().usedCount() == 3);
    CHECK(n2.sky().sat(0).sys == GnssSystem::Galileo && n2.sky().sat(3).sys == GnssSystem::Sbas && !n2.sky().sat(3).used);
    CHECK(!std::strcmp(gnssSystemName(GnssSystem::Galileo), "Galileo") && gnssSystemFromTalker("BD") == GnssSystem::Beidou);
    // GGA
    double v = 0;
    CHECK(feed2("$GPGGA,123519,3631.84,N,00617.02,W,1,08,0.9,12.4,M,46.9,M,,", 100) == NmeaResult::Data);
    CHECK(st.get(Key::SatsUsed, 110, v) && v == 8); CHECK(st.get(Key::Hdop, 110, v)); NEAR(v, 0.9, 1e-9);
    CHECK(st.get(Key::Altitude, 110, v)); NEAR(v, 12.4, 1e-9);
    CHECK(feed2("$GPGGA,123519,,,,,0,00,,,M,,M,,", 100) == NmeaResult::Ignored);
    // gpsd: sus lineas JSON se ignoran sin contar como malas
    NmeaStats b = n2.stats();
    CHECK(n2.feed("{\"class\":\"VERSION\",\"release\":\"3.26\"}", Source::Tcp, 1, st) == NmeaResult::Ignored);
    CHECK(n2.stats().badChecksum == b.badChecksum && n2.stats().malformed == b.malformed);
    CHECK(!std::strcmp(kGpsdWatchNmea, "?WATCH={\"enable\":true,\"nmea\":true};\r\n") && kGpsdPort == 2947);
}

static void testGpx() {
    // Estilo OpenCPN: extensiones con espacio de nombres propio, que se ignoran
    const char* in =
        "<?xml version=\"1.0\"?>\n<gpx version=\"1.1\" creator=\"OpenCPN\" xmlns:opencpn=\"http://www.opencpn.org\">\n"
        "<!-- comentario -->\n<metadata><name>no es un punto</name></metadata>\n"
        "<wpt lat=\"36.52\" lon=\"-6.3516667\"><time>2026-11-11T09:40:00Z</time><name>WPT 01</name><sym>circle</sym>"
        "<extensions><opencpn:guid>abc</opencpn:guid><name>dentro de extensiones</name></extensions></wpt>\n"
        "<wpt lon='-6.2366667' lat='36.5016667'><name>Fondeo &amp; boya &lt;1&gt;</name></wpt>\n"
        "<rte><name>A puerto</name><rtept lat=\"36.52\" lon=\"-6.35\"><name>R1</name></rtept><rtept lat=\"36.54\" lon=\"-6.38\"/></rte>\n"
        "<trk><name>Hoy</name><trkseg><trkpt lat=\"36.5\" lon=\"-6.2\"><time>2026-11-11T08:00:00Z</time></trkpt></trkseg>"
        "<trkseg><trkpt lat=\"36.51\" lon=\"-6.21\"></trkpt></trkseg></trk>\n</gpx>\n";
    GpxData d;
    CHECK(gpxParse(in, d));
    CHECK(d.waypoints.size() == 2 && d.routes.size() == 1 && d.tracks.size() == 1);
    CHECK(d.waypoints[0].name == "WPT 01" && d.waypoints[0].time == "2026-11-11T09:40:00Z");
    NEAR(d.waypoints[0].lon, -6.3516667, 1e-9);
    CHECK(d.waypoints[1].name == "Fondeo & boya <1>");
    NEAR(d.waypoints[1].lat, 36.5016667, 1e-9);
    CHECK(d.routes[0].name == "A puerto" && d.routes[0].pts.size() == 2 && d.routes[0].pts[0].name == "R1" && d.routes[0].pts[1].name.empty());
    CHECK(d.tracks[0].name == "Hoy" && d.tracks[0].pts.size() == 2 && d.tracks[0].pts[0].time == "2026-11-11T08:00:00Z");
    // Ida y vuelta
    GpxData d2;
    CHECK(gpxParse(gpxWrite(d), d2));
    CHECK(d2.waypoints.size() == 2 && d2.waypoints[1].name == d.waypoints[1].name && d2.routes[0].pts.size() == 2 && d2.tracks[0].pts.size() == 2);
    NEAR(d2.waypoints[0].lat, d.waypoints[0].lat, 1e-7);
    NEAR(d2.tracks[0].pts[1].lon, -6.21, 1e-7);
    CHECK(d2.routes[0].name == "A puerto" && d2.tracks[0].pts[0].time == d.tracks[0].pts[0].time);
    // Entradas no validas
    CHECK(!gpxParse("", d2));
    CHECK(!gpxParse("<kml></kml>", d2));
    CHECK(!gpxParse("<gpx><wpt lat=\"95\" lon=\"0\"/></gpx>", d2));           // latitud imposible
    CHECK(!gpxParse("<gpx><wpt lat=\"x\" lon=\"0\"/></gpx>", d2));
    CHECK(!gpxParse("<gpx><wpt lon=\"0\"/></gpx>", d2));                      // falta la latitud
    CHECK(!gpxParse("<gpx><rtept lat=\"1\" lon=\"1\"/></gpx>", d2));           // punto de ruta fuera de una ruta
    CHECK(!gpxParse("<gpx><wpt lat=\"1\" lon=\"1\"></gpx>", d2));              // sin cerrar
    CHECK(gpxParse("<gpx></gpx>", d2) && d2.waypoints.empty());
}

static void testStream() {
    // Un datagrama con dos tramas, lineas vacias y un trozo que se completa despues
    LineBuffer lb;
    std::string got[4];
    int n = 0;
    const char* dgram = "!AIVDM,1,1,,B,177KQJ5000G?tO`K>RA1wUbN0TKH,0*5C\r\n\r\n$GPGLL,3631.84,N,00617.02,W,112500,A*";
    for (const char* p = dgram; *p; p++) if (lb.push(*p) && n < 4) got[n++] = lb.line();
    CHECK(n == 1 && got[0] == "!AIVDM,1,1,,B,177KQJ5000G?tO`K>RA1wUbN0TKH,0*5C");
    for (const char* p = "3A\n"; *p; p++) if (lb.push(*p) && n < 4) got[n++] = lb.line();
    CHECK(n == 2 && got[1].rfind("$GPGLL", 0) == 0 && got[1].size() == 39);
    // Linea demasiado larga: se descarta entera y la siguiente entra bien
    for (int i = 0; i < 300; i++) CHECK(!lb.push('x'));
    CHECK(!lb.push('\n') && lb.dropped() == 1);
    for (const char* p = "$OK*00\r\n"; *p; p++) if (lb.push(*p) && n < 4) got[n++] = lb.line();
    CHECK(n == 3 && got[2] == "$OK*00");

    DupFilter df(3000);
    CHECK(!df.seen("!AIVDM,1,1,,B,AAA,0*00", 1000));
    CHECK(df.seen("!AIVDM,1,1,,B,AAA,0*00", 2000));               // repetida por otra via
    CHECK(!df.seen("!AIVDM,1,1,,B,BBB,0*00", 2000));
    CHECK(!df.seen("!AIVDM,1,1,,B,AAA,0*00", 1000 + 3001));       // pasada la ventana vuelve a valer
    for (int i = 0; i < 100; i++) { char b[24]; std::snprintf(b, sizeof b, "linea %d", i); CHECK(!df.seen(b, 9000)); }
    CHECK(df.seen("linea 99", 9500) && !df.seen("linea 0", 9500));  // solo recuerda las 64 ultimas
}

int main() {
    testGeo(); testMotion(); testWind(); testUnits(); testAstro(); testStore(); testNmea(); testAisExtra(); testInputs(); testPilot(); testSignalK(); testRoute(); testGnss(); testGpx(); testStream();
    std::printf("%d comprobaciones, %d fallos\n", total, failed);
    return failed ? 1 : 0;
}
