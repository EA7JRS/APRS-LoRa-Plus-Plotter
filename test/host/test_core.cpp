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
#include "anchor.h"
#include "aistargets.h"
#include "astro.h"
#include "chartui.h"
#include "geo.h"
#include "gpx.h"
#include "motion.h"
#include "navwarn.h"
#include "nmea.h"
#include "store.h"
#include "inputs.h"
#include "lights.h"
#include "n2k.h"
#include "route.h"
#include "signalk.h"
#include "sim.h"
#include "stream.h"
#include "tiles.h"
#include "units.h"
#include "wind.h"
#include "wxstation.h"

using namespace plotter;

static int total = 0, failed = 0;
#define CHECK(c) do { total++; if (!(c)) { failed++; std::printf("FALLO %s:%d  %s\n", __FILE__, __LINE__, #c); } } while (0)
#define NEAR(a, b, tol) do { total++; double _a = (a), _b = (b); if (!(std::fabs(_a - _b) <= (tol))) { failed++; std::printf("FALLO %s:%d  %s = %.6f, esperado %.6f +-%g\n", __FILE__, __LINE__, #a, _a, _b, double(tol)); } } while (0)

static bool near(double a, double b, double tol) { return std::fabs(a - b) <= tol; }
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
    CHECK(st.set(Key::Depth, 5, Source::Uart, 0xFFFFFFF0u) && st.get(Key::Depth, 5, v));     // desbordamiento de millis()
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
    CHECK(nm.feed("$GPRMC,123519,A,4807.038,N,01131.000,E,022.4,084.4,230394,003.1,W*6A", Source::Uart, 10, st) == NmeaResult::Data);
    CHECK(st.get(Key::Lat, 10, v)); NEAR(v, 48.1173, 1e-4);
    CHECK(st.get(Key::Lon, 10, v)); NEAR(v, 11.516667, 1e-5);
    CHECK(st.get(Key::Sog, 10, v)); NEAR(v, 22.4, 1e-9);
    CHECK(st.get(Key::Cog, 10, v)); NEAR(v, 84.4, 1e-9);
    CHECK(nm.feed(sentence("$GPRMC,123519,V,,,,,,,230394,,").c_str(), Source::Uart, 10, st) == NmeaResult::Ignored);   // sin fix
    CHECK(nm.feed(sentence("$GPGLL,3631.84,N,00617.02,W,112500,A").c_str(), Source::Uart, 20, st) == NmeaResult::Data);
    CHECK(st.get(Key::Lon, 20, v)); NEAR(v, -(6 + 17.02 / 60), 1e-9);
    CHECK(nm.feed(sentence("$SDDPT,11.9,0.5").c_str(), Source::Uart, 20, st) == NmeaResult::Data);
    CHECK(st.get(Key::Depth, 20, v)); NEAR(v, 12.4, 1e-9);
    CHECK(nm.feed(sentence("$SDDBT,40.7,f,12.4,M,6.8,F").c_str(), Source::Uart, 20, st) == NmeaResult::Data);
    CHECK(nm.feed(sentence("$WIMWV,38.0,R,14.8,N,A").c_str(), Source::Uart, 20, st) == NmeaResult::Data);
    CHECK(st.get(Key::Awa, 20, v)); NEAR(v, 38, 1e-9);
    CHECK(st.get(Key::Aws, 20, v)); NEAR(v, 14.8, 1e-9);
    CHECK(nm.feed(sentence("$WIMWV,322.0,R,7.6,M,A").c_str(), Source::Uart, 30, st) == NmeaResult::Data);
    CHECK(st.get(Key::Awa, 30, v)); NEAR(v, -38, 1e-9);                         // babor
    CHECK(st.get(Key::Aws, 30, v)); NEAR(v, 14.773, 0.001);                     // m/s a nudos
    CHECK(nm.feed(sentence("$WIMWV,38.0,R,14.8,N,V").c_str(), Source::Uart, 30, st) == NmeaResult::Ignored);   // dato no valido
    CHECK(nm.feed(sentence("$WIMWV,59.0,T,10.6,N,A").c_str(), Source::Uart, 30, st) == NmeaResult::Ignored);
    CHECK(nm.feed(sentence("$HEHDT,247.0,T").c_str(), Source::Uart, 30, st) == NmeaResult::Data);
    CHECK(nm.feed(sentence("$VWVHW,247.0,T,249.0,M,5.9,N,10.9,K").c_str(), Source::Uart, 30, st) == NmeaResult::Data);
    CHECK(st.get(Key::Stw, 30, v)); NEAR(v, 5.9, 1e-9);
    CHECK(nm.feed(sentence("$YXMTW,18.6,C").c_str(), Source::Uart, 30, st) == NmeaResult::Data);
    CHECK(nm.feed(sentence("$GPVTG,247.0,T,249.0,M,6.2,N,11.5,K,A").c_str(), Source::Uart, 30, st) == NmeaResult::Data);
    CHECK(nm.feed(sentence("$GPZDA,112500,11,11,2026,00,00").c_str(), Source::Uart, 30, st) == NmeaResult::Ignored);
    CHECK(nm.feed("basura", Source::Uart, 30, st) == NmeaResult::Bad);

    // AIS: ejemplos de la documentacion AIVDM/AIVDO de gpsd
    CHECK(nm.feed("!AIVDM,1,1,,B,177KQJ5000G?tO`K>RA1wUbN0TKH,0*5C", Source::Uart, 40, st) == NmeaResult::Ais);
    const AisReport& a = nm.ais();
    CHECK(a.type == 1 && a.mmsi == 477553000 && a.navStatus == 5 && !a.own);
    CHECK(a.hasPos); NEAR(a.lat, 47.582833, 1e-5); NEAR(a.lon, -122.345833, 1e-5);
    CHECK(a.hasSog && a.sogKn == 0); CHECK(a.hasCog); NEAR(a.cogDeg, 51.0, 1e-9); CHECK(a.hasHeading && a.heading == 181);
    // Mensaje 5 en dos fragmentos
    CHECK(nm.feed("!AIVDM,2,1,3,B,55P5TL01VIaAL@7WKO@mBplU@<PDhh000000001S;AJ::4A80?4i@E53,0*3E", Source::Uart, 50, st) == NmeaResult::Ignored);
    CHECK(nm.feed("!AIVDM,2,2,3,B,1@0000000000000,2*55", Source::Uart, 50, st) == NmeaResult::Ais);
    CHECK(nm.ais().type == 5 && nm.ais().mmsi == 369190000 && nm.ais().hasStatic);
    CHECK(!std::strcmp(nm.ais().name, "MT.MITCHELL"));
    CHECK(!std::strcmp(nm.ais().callsign, "WDA9674"));
    // Segundo fragmento suelto: se descarta
    CHECK(nm.feed("!AIVDM,2,2,3,B,1@0000000000000,2*55", Source::Uart, 60, st) == NmeaResult::Ignored);
    // VDO = barco propio
    CHECK(nm.feed(sentence("!AIVDO,1,1,,B,177KQJ5000G?tO`K>RA1wUbN0TKH,0").c_str(), Source::Uart, 70, st) == NmeaResult::Ais);
    CHECK(nm.ais().own);
}

static void testAisExtra() {
    Nmea0183 nm;
    DataStore st;
    auto feed = [&](const char* body) { return nm.feed(sentence(body).c_str(), Source::Uart, 1, st); };
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
    CHECK(nm.feed("!AIVDM,1,1,,B,177KQJ5000G?tO`K>RA1wUbN0TKH,0*5D", Source::Uart, 1, st) == NmeaResult::Bad);
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
    CHECK(nm.feed(sentence("$IIXDR,C,19.5,C,AIR,A,4.0,D,PITCH,A,-7.0,D,HEEL").c_str(), Source::Uart, 30, st) == NmeaResult::Data);
    CHECK(st.get(Key::Roll, 40, v)); NEAR(v, -7.0, 1e-9);
    CHECK(nm.feed(sentence("$IIXDR,C,19.5,C,AIR").c_str(), Source::Uart, 30, st) == NmeaResult::Ignored);
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
    // RMB: barco 0,03 mn a estribor (gobernar a babor), destino 36 31,842 N 006 17,502 W a 1,2 mn y 251 grados
    {
        char r[120];
        int rn = nmeaRmb(r, sizeof(r), 0.034, "ORIGEN", "WPT 01", 36.5307, -6.2917, 1.2, 251.4, 5.8, false);
        CHECK(rn > 0 && nmeaChecksumOk(r));
        CHECK(std::string(r).find("$ECRMB,A,0.03,L,ORIGEN,WPT 01,3631.8420,N,00617.5020,W,1.2,251.4,5.8,V,A*") == 0);
        rn = nmeaRmb(r, sizeof(r), -0.2, "", "A,B*", -33.99999999, 151.5, 2000, 361, -1.5, true);
        CHECK(rn > 0 && nmeaChecksumOk(r));
        CHECK(std::string(r).find("$ECRMB,A,0.20,R,,AB,3400.0000,S,15130.0000,E,999.9,1.0,-1.5,A,A*") == 0);
        CHECK(nmeaRmb(r, 30, 0, "A", "B", 0, 0, 1, 0, 0, false) == 0);
    }
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
    auto feed = [&](const char* body, uint32_t t) { return nm.feed(sentence(body).c_str(), Source::Uart, t, st); };
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
    auto feed2 = [&](const char* body, uint32_t t) { return n2.feed(sentence(body).c_str(), Source::Uart, t, st); };
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

static void testLights() {
    // Alcance geografico: ojo a 2 m y faro de 49 m -> 2,03 x (1,414 + 7) = 17,08 mn
    NEAR(geographicRangeNm(2, 49), 17.08, 0.01);
    NEAR(geographicRangeNm(0, 100), 20.3, 1e-9);
    // Alcance luminoso: con 10 mn de visibilidad es el nominal; con menos, menos; con mas, mas
    NEAR(luminousRangeNm(10, 10), 10.0, 1e-6);
    NEAR(luminousRangeNm(20, 10), 20.0, 1e-6);
    double fog = luminousRangeNm(20, 2), clear = luminousRangeNm(20, 27);
    CHECK(fog > 3.5 && fog < 6.5 && clear > 30 && clear < 45);
    CHECK(luminousRangeNm(5, 5) < 5 && luminousRangeNm(5, 5) > 3 && luminousRangeNm(0, 10) == 0 && luminousRangeNm(10, 0) == 0);
    // Coherencia con Allard: la iluminancia relativa a esa distancia es la del umbral
    { double d = luminousRangeNm(15, 4), T = std::pow(0.05, 1 / 4.0);
      NEAR(std::pow(T, d) / (d * d), std::pow(std::pow(0.05, 0.1), 15) / 225.0, 1e-9); }
    NEAR(sightingRangeNm(20, 10, 2, 49), 17.08, 0.01);               // lo tapa el horizonte
    NEAR(sightingRangeNm(10, 10, 2, 49), 10.0, 1e-6);                // no da mas de si la luz

    // Borneo: 30 m de cadena en 6 m de agua -> 29,39 m; con la roldana a 1 m, 29,17 m
    NEAR(swingRadiusM(30, 6), 29.3939, 1e-3);
    NEAR(swingRadiusM(30, 6, 1), 29.1719, 1e-3);
    CHECK(swingRadiusM(5, 6) == 0);
    NEAR(scopeRatio(30, 6), 5.0, 1e-12); NEAR(scopeRatio(28, 6, 1), 4.0, 1e-12);

    // Ritmos
    LightCharacter c;
    CHECK(parseLightCharacter("Fl(2) W 10s", c) && c.cls == LightClass::Flashing && c.group == 2 && c.color == LightColor::White && c.periodS == 10);
    CHECK(ialaMarkFromLight(c) == IalaMark::IsolatedDanger);
    CHECK(parseLightCharacter("Q(6)+LFl 15s", c) && c.cls == LightClass::Quick && c.group == 6 && c.longFlashAfter && c.periodS == 15);
    CHECK(ialaMarkFromLight(c) == IalaMark::CardinalS);
    CHECK(parseLightCharacter("VQ(3) 5s", c) && c.cls == LightClass::VeryQuick && ialaMarkFromLight(c) == IalaMark::CardinalE);
    CHECK(parseLightCharacter("Q(9) 15s", c) && ialaMarkFromLight(c) == IalaMark::CardinalW);
    CHECK(parseLightCharacter("Q", c) && c.periodS == 0 && ialaMarkFromLight(c) == IalaMark::CardinalN);
    CHECK(parseLightCharacter("Iso R 4s", c) && c.cls == LightClass::Isophase && c.color == LightColor::Red && ialaMarkFromLight(c) == IalaMark::PortHand);
    CHECK(ialaMarkFromLight(c, true) == IalaMark::StarboardHand);     // region B
    CHECK(parseLightCharacter("Fl G 5s", c) && ialaMarkFromLight(c) == IalaMark::StarboardHand);
    CHECK(parseLightCharacter("Fl(2+1) R 12s", c) && c.group == 2 && c.group2 == 1 && ialaMarkFromLight(c) == IalaMark::PreferredChannelPort);
    CHECK(parseLightCharacter("Mo(A) 6s", c) && c.cls == LightClass::Morse && c.morse == 'A' && ialaMarkFromLight(c) == IalaMark::SafeWater);
    CHECK(parseLightCharacter("LFl 10s", c) && c.cls == LightClass::LongFlash && ialaMarkFromLight(c) == IalaMark::SafeWater);
    CHECK(parseLightCharacter("Fl(4) Y 12s", c) && ialaMarkFromLight(c) == IalaMark::Special);
    CHECK(parseLightCharacter("F R", c) && c.cls == LightClass::Fixed && c.color == LightColor::Red);
    CHECK(parseLightCharacter("Oc(2) G 8s", c) && c.cls == LightClass::Occulting && c.group == 2 && c.color == LightColor::Green);
    CHECK(parseLightCharacter("Fl(3) 20s", c) && ialaMarkFromLight(c) == IalaMark::Light);
    // En espanol
    CHECK(parseLightCharacter("GpD(2) B 10s", c) && c.cls == LightClass::Flashing && c.group == 2 && c.color == LightColor::White);
    CHECK(parseLightCharacter("DL 10s", c) && c.cls == LightClass::LongFlash);
    CHECK(parseLightCharacter("Ct", c) && c.cls == LightClass::Quick);
    CHECK(parseLightCharacter("Rp(3) 5s", c) && c.cls == LightClass::VeryQuick && c.group == 3);
    CHECK(parseLightCharacter("GpOc(2) V 8s", c) && c.cls == LightClass::Occulting && c.color == LightColor::Green);
    CHECK(parseLightCharacter("D A 2,5s", c) && c.color == LightColor::Yellow); NEAR(c.periodS, 2.5, 1e-9);
    CHECK(!parseLightCharacter("xyz", c) && !parseLightCharacter("Fl(0)", c) && !parseLightCharacter("Fl(2", c) && !parseLightCharacter(nullptr, c));

    // Animacion: cuenta de destellos y tiempo encendida en un periodo
    auto count = [](const LightCharacter& lc, double seconds, double* onS) {
        int rises = 0; bool prev = false; int onTicks = 0;
        for (uint32_t t = 0; t < uint32_t(seconds * 1000); t += 10) {
            bool on = lightIsOn(lc, t);
            if (on && !prev) rises++;
            if (on) onTicks++;
            prev = on;
        }
        if (onS) *onS = onTicks / 100.0;
        return rises;
    };
    double onS = 0;
    parseLightCharacter("Fl(2) W 10s", c);
    CHECK(count(c, 10, &onS) == 2); NEAR(onS, 1.0, 0.05);
    parseLightCharacter("Q(6)+LFl 15s", c);
    CHECK(count(c, 15, &onS) == 7 && onS > 3.5 && onS < 4.1);        // 6 centelleos y uno largo de 2 s
    parseLightCharacter("VQ(9) 10s", c);
    CHECK(count(c, 10, nullptr) == 9 && count(c, 30, nullptr) == 27);
    parseLightCharacter("Q", c);
    CHECK(count(c, 10, nullptr) == 10);
    parseLightCharacter("Iso 4s", c);
    CHECK(count(c, 8, &onS) == 2); NEAR(onS, 4.0, 0.05);
    parseLightCharacter("Oc(2) 8s", c);
    count(c, 8, &onS);
    CHECK(onS > 5.9 && onS < 6.1 && !lightIsOn(c, 500) && lightIsOn(c, 1500) && lightIsOn(c, 7000));   // mas luz que oscuridad
    parseLightCharacter("Mo(A) 6s", c);
    CHECK(count(c, 6, &onS) == 2); NEAR(onS, 2.0, 0.05);
    parseLightCharacter("Fl(2+1) R 12s", c);
    CHECK(count(c, 12, nullptr) == 3);
    parseLightCharacter("Fl(5) 4s", c);                              // no cabe a 2 s por destello: se aprieta
    CHECK(count(c, 4, nullptr) == 5);
    parseLightCharacter("F R", c);
    CHECK(lightIsOn(c, 0) && lightIsOn(c, 12345));

    // Lista de luces de la microSD
    LightRecord lr;
    CHECK(parseLightRecord("Faro de ejemplo;36.5297;-6.3147;Fl(2) W 10s;49;20", lr));
    CHECK(!std::strcmp(lr.name, "Faro de ejemplo") && lr.character.cls == LightClass::Flashing && lr.character.group == 2 && lr.heightM == 49 && lr.nominalNm == 20);
    NEAR(lr.lat, 36.5297, 1e-9); NEAR(lr.lon, -6.3147, 1e-9);
    NEAR(lightSightingRangeNm(lr, 10, 2), 17.08, 0.01);              // manda el horizonte
    NEAR(lightSightingRangeNm(lr, 6, 2), 13.557, 0.01);              // manda la visibilidad
    CHECK(parseLightRecord("  Baliza;36,5;-6,3;Q(3) 10s;;\r\n", lr) && lr.heightM == 0 && lightSightingRangeNm(lr, 10, 2) == 0);
    NEAR(lr.lat, 36.5, 1e-9);
    CHECK(parseLightRecord("Solo posicion;36.5;-6.3", lr) && lr.character.cls == LightClass::Unknown);
    CHECK(!parseLightRecord("# comentario", lr) && !parseLightRecord("", lr) && !parseLightRecord("Mal;x;y", lr) && !parseLightRecord("Lejos;95;0", lr));

    // Luces de OpenStreetMap (etiquetas seamark:light:*)
    char txt[32];
    CHECK(lightCharacterFromSeamark("Fl", "2", "10", "white", c, txt, sizeof(txt)) && !std::strcmp(txt, "Fl(2) W 10s"));
    CHECK(c.cls == LightClass::Flashing && c.group == 2 && c.periodS == 10 && c.color == LightColor::White);
    CHECK(lightCharacterFromSeamark("Q+LFl", "6", "15", "white", c, txt, sizeof(txt)) && !std::strcmp(txt, "Q(6)+LFl W 15s"));
    CHECK(c.cls == LightClass::Quick && c.group == 6 && c.longFlashAfter && ialaMarkFromLight(c) == IalaMark::CardinalS);
    CHECK(lightCharacterFromSeamark("Fl", "2+1", "12", "red", c) && c.group == 2 && c.group2 == 1 && c.color == LightColor::Red);
    CHECK(lightCharacterFromSeamark("Mo", "A", "6", "white", c) && c.cls == LightClass::Morse && c.morse == 'A');
    CHECK(lightCharacterFromSeamark("Iso", nullptr, "4", "green", c) && c.cls == LightClass::Isophase && c.color == LightColor::Green);
    CHECK(lightCharacterFromSeamark("VQ", "", "", "", c) && c.cls == LightClass::VeryQuick && c.periodS == 0);
    CHECK(lightCharacterFromSeamark("Fl", "1", "5", "yellow", c) && c.group == 1 && ialaMarkFromLight(c) == IalaMark::Special);
    CHECK(lightCharacterFromSeamark("Oc", "2", "2.5", "white;red", c, txt, sizeof(txt)) && c.cls == LightClass::Occulting); NEAR(c.periodS, 2.5, 1e-9);
    CHECK(!lightCharacterFromSeamark(nullptr, "2", "10", "white", c) && !lightCharacterFromSeamark("", "", "", "", c));
    CHECK(lightRecordFromSeamark("Faro; de ejemplo", 36.5297, -6.3147, "Fl", "2", "10", "white", "49", "20", lr));
    CHECK(!std::strcmp(lr.name, "Faro, de ejemplo") && !std::strcmp(lr.characterText, "Fl(2) W 10s") && lr.heightM == 49 && lr.nominalNm == 20);
    char ln[120];
    CHECK(formatLightRecord(ln, sizeof(ln), lr) > 0 && !std::strcmp(ln, "Faro, de ejemplo;36.52970;-6.31470;Fl(2) W 10s;49;20"));
    LightRecord lr2;
    CHECK(parseLightRecord(ln, lr2) && lr2.character.group == 2 && lr2.heightM == 49);     // ida y vuelta a la microSD
    NEAR(lr2.lon, -6.3147, 1e-9);
    CHECK(lightRecordFromSeamark(nullptr, 36.5, -6.3, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, lr) && !std::strcmp(lr.name, "Luz"));
    CHECK(formatLightRecord(ln, sizeof(ln), lr) > 0 && !std::strcmp(ln, "Luz;36.50000;-6.30000;;;"));
    char ov[220];
    CHECK(overpassLightsUrl(ov, sizeof(ov), 36.3, -6.6, 36.8, -6.0) > 0);
    CHECK(!std::strcmp(ov, "https://overpass-api.de/api/interpreter?data=%5Bout%3Ajson%5D%3Bnode%5B%22seamark%3Alight%3Acharacter%22%5D(36.3000,-6.6000,36.8000,-6.0000)%3Bout%3B"));
    CHECK(overpassLightsUrl(ov, 40, 36.3, -6.6, 36.8, -6.0) == 0);

    // Ayudas AIS (mensaje 21)
    bool fl = false;
    CHECK(ialaMarkFromAisAton(20, &fl) == IalaMark::CardinalN && fl);
    CHECK(ialaMarkFromAisAton(9, &fl) == IalaMark::CardinalN && !fl);
    CHECK(ialaMarkFromAisAton(24) == IalaMark::PortHand && ialaMarkFromAisAton(14) == IalaMark::StarboardHand);
    CHECK(ialaMarkFromAisAton(28) == IalaMark::IsolatedDanger && ialaMarkFromAisAton(29) == IalaMark::SafeWater && ialaMarkFromAisAton(30) == IalaMark::Special);
    CHECK(ialaMarkFromAisAton(2) == IalaMark::Racon && ialaMarkFromAisAton(6) == IalaMark::Light && ialaMarkFromAisAton(0) == IalaMark::Unknown);
    CHECK(!std::strcmp(ialaMarkNameEs(IalaMark::CardinalE), "Cardinal este") && !std::strcmp(ialaMarkNameEs(IalaMark::SafeWater), "Aguas navegables"));
}

static void testNavWarn() {
    // Mismo formato que el XML del IHM, con avisos inventados
    const std::string xml =
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<dataroot xmlns:od=\"urn:schemas-microsoft-com:officedata\" generated=\"2026-11-10T09:31:41\">\n"
        "<NAVAREASVIGOR>\n<nnuna>260501</nnuna>\n<nnunaf>0501/26</nnunaf>\n<nfemi>2026-06-03T00:00:00</nfemi>\n"
        "<nlocae>MAR DE ALBOR\xC3\x81N</nlocae>\n<nlocai>ALBORAN SEA</nlocai>\n"
        "<ntees>EJERCICIOS DE TIRO EN ZONA LIMITADA POR:\n36-10.0N 004-30.5W\n36-10.0N 004-10.0W\n36-00,5N 004-10.0W\n36 00,500N 004 30,500W</ntees>\n"
        "<ntein>FIRING EXERCISES IN AREA BOUNDED BY:\n36-10.0N 004-30.5W\n36-10.0N 004-10.0W\n36-00,5N 004-10.0W\n36 00,500N 004 30,500W</ntein>\n"
        "<nasun>EJERCICIOS &amp; TIRO</nasun>\n<nasuni>FIRING EXERCISES</nasuni>\n</NAVAREASVIGOR>\n"
        "<NAVAREASVIGOR><nnuna>260502</nnuna><nnunaf>0502/26</nnunaf><nfemi>2026-07-01T00:00:00</nfemi><nlocae>MEDITERR\xC3\x81NEO</nlocae>"
        "<nlocai>MEDITERRANEAN</nlocai><ntees>AVISO GENERAL SIN POSICIONES. CANAL 16.</ntees><ntein>GENERAL NOTICE. 2 VESSELS, 24 HOURS.</ntein>"
        "<nasun>AVISO GENERAL</nasun><nasuni>GENERAL NOTICE</nasuni></NAVAREASVIGOR>\n</dataroot>\n";
    std::vector<NavWarning> w;
    std::string gen;
    CHECK(navWarningsParseIhm(xml, w, &gen) && w.size() == 2 && gen == "2026-11-10T09:31:41");
    CHECK(w[0].number == "0501/26" && w[0].date == "2026-06-03" && w[0].areaEn == "ALBORAN SEA" && w[0].areaEs == "MAR DE ALBOR\xC3\x81N");
    CHECK(w[0].titleEs == "EJERCICIOS & TIRO" && w[0].titleEn == "FIRING EXERCISES" && w[0].textEs.rfind("EJERCICIOS DE TIRO", 0) == 0);
    CHECK(w[0].positions.size() == 4);
    NEAR(w[0].positions[0].lat, 36.0 + 10.0 / 60, 1e-9); NEAR(w[0].positions[0].lon, -(4.0 + 30.5 / 60), 1e-9);
    NEAR(w[0].positions[2].lat, 36.0 + 0.5 / 60, 1e-9);                         // coma decimal
    NEAR(w[0].positions[3].lon, -(4.0 + 30.5 / 60), 1e-9);                      // separado por espacios
    CHECK(w[1].positions.empty() && w[1].number == "0502/26");
    NEAR(navWarningDistanceNm(w[0], LatLon{36.0 + 10.0 / 60, -(4.0 + 30.5 / 60)}), 0.0, 1e-6);
    double d = navWarningDistanceNm(w[0], LatLon{36.5297, -6.2927});
    CHECK(d > 80 && d < 100 && navWarningDistanceNm(w[1], LatLon{36, -6}) < 0);
    CHECK(!navWarningsParseIhm("<html>no</html>", w) && w.empty());
    CHECK(navWarningsParseIhm("<dataroot></dataroot>", w) && w.empty());

    // Posiciones sueltas: formatos de radioaviso y cosas que no lo son
    std::vector<LatLon> p;
    CHECK(navWarningPositions("WRECK IN 46-04.0N 033-12.8E AND 42 11,718N 009 59,778E.", p) == 2);
    NEAR(p[0].lat, 46.0 + 4.0 / 60, 1e-9); NEAR(p[0].lon, 33.0 + 12.8 / 60, 1e-9);
    NEAR(p[1].lat, 42.0 + 11.718 / 60, 1e-9); NEAR(p[1].lon, 9.0 + 59.778 / 60, 1e-9);
    p.clear();
    CHECK(navWarningPositions("36-31N 006-17W", p) == 1); NEAR(p[0].lon, -(6.0 + 17.0 / 60), 1e-9);
    p.clear();
    CHECK(navWarningPositions("12-05.5S 077-10.0W", p) == 1 && p[0].lat < 0 && p[0].lon < 0);
    p.clear();
    CHECK(navWarningPositions("CHANNEL 16, 24 HOURS, 2026-06-03, 10-20 NM, 36-70.0N 004-10.0W, 136-10.0N 004-10.0W, 36-10.0NE 004-10.0W", p) == 0);
}

static void testTiles() {
    int tx, ty, ix, iy;
    lonLatToTile(LatLon{0, 0}, 1, tx, ty, &ix, &iy);
    CHECK(tx == 1 && ty == 1 && ix == 0 && iy == 0);
    // Cadiz a zoom 12, calculado aparte con la formula de OpenStreetMap: x = 1976,403, y = 1600,966
    lonLatToTile(LatLon{36.5297, -6.2927}, 12, tx, ty, &ix, &iy);
    CHECK(tx == 1976 && ty == 1600 && ix == 103 && iy == 247 && ix >= 0 && ix < 256 && iy >= 0 && iy < 256);
    CHECK(tmsY(1600, 12) == 2495 && tmsY(0, 0) == 0);
    // Ida y vuelta
    double px, py;
    lonLatToWorldPx(LatLon{36.5297, -6.2927}, 15, px, py);
    LatLon b = worldPxToLonLat(px, py, 15);
    NEAR(b.lat, 36.5297, 1e-9); NEAR(b.lon, -6.2927, 1e-9);
    lonLatToTile(LatLon{89.9, 179.999}, 3, tx, ty);
    CHECK(tx == 7 && ty == 0);
    lonLatToTile(LatLon{-89.9, -180}, 3, tx, ty);
    CHECK(tx == 0 && ty == 7);
    // Escala: 156543 m por pixel en el ecuador a zoom 0
    NEAR(metersPerPixel(0, 0), 156543.03, 0.01);
    NEAR(metersPerPixel(60, 10), 156543.03 / 1024 / 2, 0.01);
    // 3 millas en 824 px a 36,5 N
    int z = zoomForWidth(36.5, 3.0, 824);
    CHECK(z == 14 && metersPerPixel(36.5, z) * 824 >= 3 * 1852.0 && metersPerPixel(36.5, z + 1) * 824 < 3 * 1852.0);
    char u[120];
    CHECK(tileUrl(u, sizeof(u), "https://tile.example/{z}/{x}/{y}.png", 12, 1976, 1600) > 0 && !std::strcmp(u, "https://tile.example/12/1976/1600.png"));
    CHECK(tileUrl(u, sizeof(u), kTilesIgnBase, 12, 1976, 1600) > 0 && !std::strcmp(u, "https://tms-ign-base.idee.es/1.0.0/IGNBaseTodo/12/1976/2495.jpeg"));
    CHECK(tileUrl(u, 20, kTilesPnoa, 12, 1976, 1600) == 0 && tileUrl(u, sizeof(u), "https://sin.plantilla/", 1, 1, 1) == 0);
    // WMS: recuadro de la tesela en Web Mercator
    double x0, y0, x1, y1;
    tileBounds3857(0, 0, 0, x0, y0, x1, y1);
    NEAR(x0, -20037508.34, 0.01); NEAR(y1, 20037508.34, 0.01); NEAR(x1, 20037508.34, 0.01); NEAR(y0, -20037508.34, 0.01);
    tileBounds3857(1, 1, 0, x0, y0, x1, y1);
    NEAR(x0, 0.0, 1e-6); NEAR(y0, 0.0, 1e-6); NEAR(x1, 20037508.34, 0.01);
    tileBounds3857(12, 1976, 1600, x0, y0, x1, y1);
    { double px, py; lonLatToWorldPx(LatLon{36.5297, -6.2927}, 12, px, py);      // Cadiz cae dentro del recuadro
      double mx = (px / (256.0 * 4096) - 0.5) * 2 * 20037508.342789244, my = (0.5 - py / (256.0 * 4096)) * 2 * 20037508.342789244;
      CHECK(mx > x0 && mx < x1 && my > y0 && my < y1); NEAR(x1 - x0, 9783.94, 0.01); }
    char w[320];
    CHECK(wmsTileUrl(w, sizeof(w), kWmsIhmPaperCharts, "capa1,capa2", 1, 1, 0) > 0);
    CHECK(!std::strcmp(w, "https://ideihm.covam.es/wms/CartaOF?SERVICE=WMS&VERSION=1.3.0&REQUEST=GetMap&LAYERS=capa1,capa2&STYLES=&CRS=EPSG:3857"
                          "&BBOX=0.00,0.00,20037508.34,20037508.34&WIDTH=256&HEIGHT=256&FORMAT=image%2Fpng&TRANSPARENT=TRUE"));
    CHECK(wmsTileUrl(w, sizeof(w), "https://servidor.example/wms?map=a", "c", 0, 0, 0, false, "image/jpeg") > 0);
    CHECK(std::strstr(w, "wms?map=a&SERVICE=WMS") && std::strstr(w, "FORMAT=image%2Fjpeg&TRANSPARENT=FALSE"));
    CHECK(wmsTileUrl(w, 60, kWmsIhmTides, "c", 1, 1, 0) == 0 && wmsTileUrl(w, sizeof(w), nullptr, "c", 1, 1, 0) == 0);
    char q[400];
    CHECK(wmsFeatureInfoUrl(q, sizeof(q), kWmsIhmLights, "luces", LatLon{36.5297, -6.2927}, 12) > 0);
    CHECK(std::strstr(q, "https://ideihm.covam.es/wms/lucesIHM?SERVICE=WMS&VERSION=1.3.0&REQUEST=GetFeatureInfo&LAYERS=luces&QUERY_LAYERS=luces"));
    CHECK(std::strstr(q, "&WIDTH=256&HEIGHT=256&I=103&J=247&INFO_FORMAT=text%2Fplain&FEATURE_COUNT=5"));
    CHECK(wmsFeatureInfoUrl(q, sizeof(q), kWmsIhmLights, "luces", LatLon{36.5297, -6.2927}, 12, "application/vnd.ogc.gml+xml", 1) > 0);
    CHECK(std::strstr(q, "INFO_FORMAT=application%2Fvnd.ogc.gml%2Bxml&FEATURE_COUNT=1") && wmsFeatureInfoUrl(q, 80, kWmsIhmLights, "l", LatLon{0, 0}, 3) == 0);
    CHECK(tilePath(u, sizeof(u), "/charts/osm", 12, 1976, 1600, "png") > 0 && !std::strcmp(u, "/charts/osm/12/1976/1600.png"));
}

static void testChartUi() {
    const LatLon a{36.5, -6.3};
    Measure m = measure(a, gcDestination(a, 90, 6.0), false, 6.0);
    NEAR(m.distanceNm, 6.0, 1e-6); NEAR(m.bearing, 90.0, 1e-6); NEAR(m.hours, 1.0, 1e-6); NEAR(m.reciprocal, 270.0, 0.2);
    m = measure(a, LatLon{37.5, -6.3});
    NEAR(m.distanceNm, 60.0, 0.1); NEAR(m.bearing, 0.0, 1e-9); NEAR(m.reciprocal, 180.0, 1e-9); CHECK(m.hours < 0);
    const LatLon path[] = {a, LatLon{36.5 + 1.0 / 60, -6.3}, LatLon{36.5 + 3.0 / 60, -6.3}};
    NEAR(pathLengthNm(path, 3), 3.0, 0.01); CHECK(pathLengthNm(path, 1) == 0);

    char b[48];
    CHECK(formatLatLon(b, sizeof(b), LatLon{36.0 + 31.84 / 60, -(6.0 + 17.02 / 60)}) > 0);
    CHECK(!std::strcmp(b, "36\xC2\xB0" "31,840\xE2\x80\xB2N 006\xC2\xB0" "17,020\xE2\x80\xB2W"));
    formatLatLon(b, sizeof(b), LatLon{-(12.0 + 5.5 / 60), 77.0 + 10.0 / 60}, 1, false);
    CHECK(!std::strcmp(b, "12\xC2\xB0" "05.5\xE2\x80\xB2S 077\xC2\xB0" "10.0\xE2\x80\xB2" "E"));
    formatLatLon(b, sizeof(b), LatLon{36.99999999, -5.99999999}, 2);                 // el redondeo no deja "60,00"
    CHECK(!std::strcmp(b, "37\xC2\xB0" "00,00\xE2\x80\xB2N 006\xC2\xB0" "00,00\xE2\x80\xB2W"));
    formatLatLon(b, sizeof(b), LatLon{36.5, -6.25}, 0);
    CHECK(!std::strcmp(b, "36\xC2\xB0" "30\xE2\x80\xB2N 006\xC2\xB0" "15\xE2\x80\xB2W"));
    CHECK(formatLatLon(b, 12, a) == 0);
    CHECK(formatDuration(b, sizeof(b), 0.75) > 0 && !std::strcmp(b, "45 min"));
    formatDuration(b, sizeof(b), 2.5834);
    CHECK(!std::strcmp(b, "02 h 35 min"));
    formatDuration(b, sizeof(b), 76.2);
    CHECK(!std::strcmp(b, "3 d 04 h") && formatDuration(b, sizeof(b), -1) == 0);

    ViewHistory h;
    ChartView v;
    CHECK(!h.back(v));
    h.push({{36.5, -6.3}, 12});
    h.push({{36.5, -6.3}, 12});                                      // repetida: no cuenta
    h.push({{36.6, -6.2}, 14});
    CHECK(h.size() == 2 && h.back(v) && v.zoom == 14 && h.back(v) && v.zoom == 12 && !h.back(v));
    for (int i = 0; i < 20; i++) h.push({{36.0 + i * 0.01, -6.0}, 10});
    CHECK(h.size() == 12 && h.back(v));
    NEAR(v.center.lat, 36.19, 1e-9);
}

static void testWxStation() {
    const LatLon here{36.5297, -6.2927};
    WxObservation ob[3];
    ob[0].pos = gcDestination(here, 45, 4.2);  ob[0].timeUnix = 1000000; ob[0].windMs = 6.0; ob[0].windDirDeg = 250; ob[0].gustMs = 9.5;
    ob[0].pressureHpa = 1014.2; ob[0].tempC = 18.4; ob[0].humidityPct = 71; ob[0].visKm = 18.52;
    ob[1].pos = gcDestination(here, 180, 1.0); ob[1].timeUnix = 1000000; ob[1].tempC = 19.0;             // mas cerca, sin viento
    ob[2].pos = gcDestination(here, 0, 50.0);  ob[2].timeUnix = 1000000; ob[2].windMs = 3.0; ob[2].windDirDeg = 90;
    double d = 0;
    CHECK(nearestStation(ob, 3, here, 30, true, &d) == 0); NEAR(d, 4.2, 1e-6);
    CHECK(nearestStation(ob, 3, here, 30, false) == 1);
    CHECK(nearestStation(ob, 3, here, 3, true) == -1 && nearestStation(ob, 0, here) == -1);
    CHECK(nearestStation(ob, 3, here, 60, true) == 0);

    DataStore st;
    uint32_t now = 1000000 + 25 * 60;
    NEAR(observationAgeMin(ob[0], now), 25.0, 1e-9);
    CHECK(applyObservation(ob[0], now, 5000, st) == 7);
    double v = 0;
    CHECK(st.get(Key::Tws, 6000, v)); NEAR(v, 6.0 * 3600 / 1852, 1e-9);
    CHECK(st.get(Key::Twd, 6000, v) && v == 250); CHECK(st.get(Key::Gust, 6000, v)); NEAR(v, 18.466523, 1e-5);
    CHECK(st.get(Key::Pressure, 6000, v)); NEAR(v, 1014.2, 1e-9);
    CHECK(st.get(Key::Visibility, 6000, v)); NEAR(v, 10.0, 1e-9);
    CHECK(st.get(Key::Humidity, 6000, v) && v == 71);
    CHECK(st.source(Key::Twd, 6000) == Source::Internet && st.quality(Key::Twd) == Quality::Estimated);
    // Un dato medido a bordo manda sobre el de la estacion
    DataStore st2;
    st2.set(Key::Tws, 12.0, Source::Sensor, 5000);
    CHECK(applyObservation(ob[0], now, 5000, st2) == 6);
    CHECK(st2.get(Key::Tws, 6000, v) && v == 12.0 && st2.quality(Key::Tws) == Quality::Measured);
    // Vieja, sin hora o en calma
    DataStore st3;
    CHECK(applyObservation(ob[0], 1000000 + 91 * 60, 5000, st3) == 0);
    WxObservation calm = ob[0];
    calm.windMs = 0.1;
    CHECK(applyObservation(calm, now, 5000, st3) == 6 && !st3.get(Key::Twd, 6000, v));
    WxObservation none;
    CHECK(applyObservation(none, now, 5000, st3) == 0);

    char b[80];
    CHECK(describeObservation(b, sizeof(b), ob[0], here, now) > 0);
    CHECK(!std::strcmp(b, "Estaci\xC3\xB3n de tierra a 4,2 mn al NE \xC2\xB7 hace 25 min"));
    CHECK(describeObservation(b, 20, ob[0], here, now) == 0);
}

static void testSim() {
    NmeaSimulator sim;
    Nmea0183 nm;
    DataStore st;
    char line[100];
    LatLon start = sim.position();
    uint32_t now = 1000;
    for (int sec = 0; sec <= 600; sec++, now += 1000) {
        sim.step(now);
        for (int i = 0; i < NmeaSimulator::count(); i++) {
            int n = sim.sentence(i, line, sizeof(line));
            if (sec == 0 || sec == 600) { CHECK(n > 0 && n <= 82 && nmeaChecksumOk(line)); }
            NmeaResult r = nm.feed(line, Source::Sensor, now, st);
            if (sec == 600) CHECK(r == NmeaResult::Data);
        }
    }
    CHECK(nm.stats().badChecksum == 0 && nm.stats().malformed == 0);
    // 10 minutos a unos 6,2 nudos: alrededor de 1,03 millas hacia el WSW
    double d = gcDistanceNm(start, sim.position()), b = gcInitialBearing(start, sim.position());
    CHECK(d > 0.95 && d < 1.12 && b > 240 && b < 255);
    double v = 0, la = 0, lo = 0;
    CHECK(st.get(Key::Lat, now, la) && st.get(Key::Lon, now, lo));
    NEAR(la, sim.position().lat, 2e-6); NEAR(lo, sim.position().lon, 2e-6);       // lo leido es donde esta el barco
    CHECK(st.get(Key::Sog, now, v) && v > 5.7 && v < 6.7);
    CHECK(st.get(Key::Cog, now, v) && v > 242 && v < 252);
    CHECK(st.get(Key::Heading, now, v) && v > 238 && v < 256);
    CHECK(st.get(Key::Depth, now, v) && v > 9.8 && v < 15.0);
    CHECK(st.get(Key::Aws, now, v) && v > 6 && v < 20);
    CHECK(st.get(Key::Awa, now, v) && v > 10 && v < 80);                          // viento del WNW con rumbo WSW: por estribor
    CHECK(st.get(Key::Stw, now, v) && st.get(Key::WaterTemp, now, v) && v == 18.5);
    CHECK(st.get(Key::SatsUsed, now, v) && v == 9);
    CHECK(sim.sentence(99, line, sizeof(line)) == 0 && sim.sentence(0, line, 10) == 0);

    // Blancos AIS del simulador: entran en la tabla y el que viene de vuelta encontrada acaba dando alarma
    NmeaSimulator s2;
    Nmea0183 n2;
    AisTargets tg;
    bool danger = false, encoded = true;
    for (uint32_t t = 1000; t <= 600000; t += 1000) {
        s2.step(t);
        for (int i = 0; i < NmeaSimulator::aisCount(); i++) {
            if (s2.aisSentence(i, line, sizeof(line)) <= 0) encoded = false;
            if (n2.feed(line, Source::Sensor, t, st) == NmeaResult::Ais) tg.update(n2.ais(), t);
        }
        tg.compute(true, Mover{s2.position(), s2.sog(), s2.cog()}, t);
        if (tg.dangerousCount() > 0) danger = true;
    }
    CHECK(tg.count() == 5 && danger && encoded && tg.rejectedJumps() == 0);
    CHECK(tg.find(224900440) >= 0 && !std::strcmp(tg.at(tg.find(224900440)).name, "FONDEADO CUATRO") && tg.at(tg.find(224900440)).navStatus == 1);
    CHECK(s2.aisSentence(99, line, sizeof(line)) == 0);
}

static void testAisTargets() {
    // Codificador contra el decodificador (ya contrastado con gpsd)
    Nmea0183 nm;
    DataStore st;
    char line[100];
    CHECK(aisEncodePosition(line, sizeof(line), 224123450, 36.5512, -6.3321, 12.3, 87.4, 88, 0) > 0 && nmeaChecksumOk(line));
    CHECK(nm.feed(line, Source::Uart, 1, st) == NmeaResult::Ais);
    CHECK(nm.ais().type == 1 && nm.ais().mmsi == 224123450 && nm.ais().navStatus == 0 && nm.ais().heading == 88);
    NEAR(nm.ais().lat, 36.5512, 1e-6); NEAR(nm.ais().lon, -6.3321, 1e-6); NEAR(nm.ais().sogKn, 12.3, 1e-9); NEAR(nm.ais().cogDeg, 87.4, 1e-9);
    CHECK(aisEncodeName(line, sizeof(line), 224123450, "Mar de Cadiz") > 0 && nm.feed(line, Source::Uart, 1, st) == NmeaResult::Ais);
    CHECK(nm.ais().type == 24 && !std::strcmp(nm.ais().name, "MAR DE CADIZ"));
    CHECK(aisEncodePosition(line, 20, 1, 0, 0, 0, 0, 0) == 0);

    const LatLon me{36.53, -6.29};
    Mover own{me, 6.0, 270.0};
    AisTargets tg;
    auto report = [](uint32_t mmsi, LatLon p, double sog, double cog) {
        AisReport r; r.mmsi = mmsi; r.type = 1; r.hasPos = true; r.lat = p.lat; r.lon = p.lon;
        r.hasSog = r.hasCog = true; r.sogKn = sog; r.cogDeg = cog; return r;
    };
    // 1: de vuelta encontrada por la proa, a 3 mn: peligroso (se juntan en 10 min)
    CHECK(tg.update(report(111, gcDestination(me, 270, 3.0), 12.0, 90.0), 1000) == 0);
    // 2: lejos y alejandose; 3: fondeado a 1 mn por el traves; 4: sin posicion (solo nombre)
    tg.update(report(222, gcDestination(me, 45, 5.0), 10.0, 45.0), 1000);
    tg.update(report(333, gcDestination(me, 0, 1.0), 0.0, 0.0), 1000);
    AisReport nameOnly; nameOnly.mmsi = 444; nameOnly.type = 24; std::strcpy(nameOnly.name, "SIN POSICION");
    tg.update(nameOnly, 1000);
    AisReport ownShip = report(999, me, 6, 270); ownShip.own = true;
    CHECK(tg.update(ownShip, 1000) == -1 && tg.count() == 4);
    tg.compute(true, own, 2000);
    const AisTarget& a = tg.at(tg.find(111));
    NEAR(a.rangeNm, 3.0, 1e-3); NEAR(a.bearing, 270.0, 0.1); CHECK(a.hasCpa && a.dangerous);
    NEAR(a.cpaNm, 0.0, 0.02); NEAR(a.tcpaMin, 10.0, 0.1);
    CHECK(!tg.at(tg.find(222)).dangerous && tg.at(tg.find(222)).tcpaMin < 0);
    const AisTarget& c = tg.at(tg.find(333));
    CHECK(!c.dangerous && c.hasCpa); NEAR(c.cpaNm, 1.0, 0.01);
    CHECK(!tg.at(tg.find(444)).hasRange && tg.at(tg.find(444)).classB && tg.dangerousCount() == 1);
    int idx[10];
    CHECK(tg.sorted(idx, 10) == 4 && tg.at(idx[0]).mmsi == 111 && tg.at(idx[1]).mmsi == 333 && tg.at(idx[2]).mmsi == 222 && tg.at(idx[3]).mmsi == 444);
    CHECK(tg.sorted(idx, 2) == 2 && tg.at(idx[0]).mmsi == 111 && tg.at(idx[1]).mmsi == 333);
    CHECK(tg.silence(111, true) && tg.dangerousCount() == 0 && !tg.silence(12345, true));
    // Sin posicion propia no hay distancias ni alarmas
    tg.compute(false, own, 2000);
    CHECK(!tg.at(tg.find(111)).hasRange && !tg.at(tg.find(111)).dangerous);

    // Salto imposible: se ignora; si la siguiente lo confirma, se acepta
    tg.update(report(333, gcDestination(me, 0, 9.0), 0.0, 0.0), 11000);
    tg.compute(true, own, 11000);
    NEAR(tg.at(tg.find(333)).rangeNm, 1.0, 0.01); CHECK(tg.rejectedJumps() == 1);
    tg.update(report(333, gcDestination(me, 0, 9.01), 0.0, 0.0), 21000);
    tg.compute(true, own, 21000);
    NEAR(tg.at(tg.find(333)).rangeNm, 9.01, 0.01);

    // Perdido a los 6 min sin posicion, fuera a los 12
    tg.compute(true, own, 21000 + 7 * 60000);
    CHECK(tg.at(tg.find(333)).lost && !tg.at(tg.find(333)).dangerous && tg.count() == 4);   // perdidos, pero siguen en la tabla
    CHECK(tg.at(tg.find(111)).lost && !tg.at(tg.find(111)).dangerous);
    tg.compute(true, own, 21000 + 13 * 60000);
    CHECK(tg.count() == 0);

    // Tabla llena: no entra uno nuevo si todos son recientes; si hay uno viejo, lo sustituye
    AisTargets full;
    for (int i = 0; i < AisTargets::kMax; i++) full.update(report(1000 + i, me, 1, 1), 1000 + i);
    CHECK(full.count() == AisTargets::kMax && full.update(report(5000, me, 1, 1), 2000) == -1);
    CHECK(full.update(report(5000, me, 1, 1), 70000) >= 0 && full.find(5000) >= 0 && full.find(1000) < 0);
    CHECK(!std::strcmp(aisShipTypeNameEs(36), "Velero") && !std::strcmp(aisShipTypeNameEs(74), "Carguero") && !std::strcmp(aisShipTypeNameEs(-1), "Buque"));
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

static void testAnchor() {
    AnchorWatch w;
    CHECK(!w.state().set && !w.alarm());
    // Radio recomendado: 30 m de cadena en 5 m de agua con la roldana a 1 m: borneo sqrt(900 - 36) = 29,4 m, mas 10 m
    CHECK(near(AnchorWatch::suggestedRadiusM(30, 5, 1, 10), 39.39, 0.02));
    CHECK(near(AnchorWatch::suggestedRadiusM(30, 0), 40, 1e-9));          // sin sonda: toda la cadena
    CHECK(near(AnchorWatch::suggestedRadiusM(10, 20), 20, 1e-9));         // la cadena no llega: toda la cadena
    LatLon a{36.53, -6.28};
    w.drop(a, 40, 1000);
    CHECK(w.state().set && !w.state().hasFix && !w.alarm());
    LatLon in = gcDestination(a, 90, 25.0 / kMetersPerNm);
    w.update(in, 2000);
    CHECK(w.state().hasFix && near(w.state().distanceM, 25, 0.05) && near(w.state().bearing, 90, 0.1));
    CHECK(!w.state().dragging);
    // Sale del circulo: no avisa hasta llevar 10 s fuera
    LatLon out = gcDestination(a, 200, 55.0 / kMetersPerNm);
    w.update(out, 3000);
    CHECK(!w.state().dragging && near(w.state().maxDistanceM, 55, 0.05));
    w.update(out, 12000);
    CHECK(!w.state().dragging);
    w.update(out, 13000);
    CHECK(w.state().dragging && w.alarm());
    // Un salto suelto del GPS no dispara: fuera, dentro, fuera
    w.update(in, 14000);
    CHECK(!w.state().dragging && !w.alarm());
    w.update(out, 15000);
    w.update(in, 20000);
    w.update(out, 26000);
    CHECK(!w.state().dragging);
    CHECK(near(w.state().maxDistanceM, 55, 0.05));
    // Agrandar el circulo quita el aviso
    w.update(out, 40000);
    CHECK(w.state().dragging);
    w.setRadius(60);
    w.update(out, 41000);
    CHECK(!w.state().dragging && near(w.radiusM(), 60, 1e-9));
    w.setRadius(1);
    CHECK(near(w.radiusM(), 5, 1e-9));
    w.setRadius(9999);
    CHECK(near(w.radiusM(), 500, 1e-9));
    // Sin posicion durante un minuto
    w.setRadius(60);
    w.tick(41000 + 59000);
    CHECK(!w.state().noFix);
    w.tick(41000 + 60000);
    CHECK(w.state().noFix && w.alarm());
    w.update(in, 41000 + 61000);
    CHECK(!w.state().noFix && !w.alarm());
    // El contador de milisegundos da la vuelta
    AnchorWatch r;
    r.drop(a, 40, 0xFFFFF000u);
    r.update(out, 0xFFFFF800u);
    r.update(out, 0x00002000u);
    CHECK(r.state().dragging);
    r.weigh();
    CHECK(!r.state().set && !r.alarm());
    r.update(out, 0x00009000u);
    CHECK(!r.state().hasFix);
}

// Convierte "00,56,79,..." (formato de canboat) en bytes
static int hexBytes(const char* txt, uint8_t* out, int max) {
    int n = 0;
    for (const char* p = txt; *p && n < max;) {
        while (*p == ',' || *p == ' ') p++;
        if (!*p) break;
        out[n++] = (uint8_t)std::strtoul(p, nullptr, 16);
        while (*p && *p != ',' && *p != ' ') p++;
    }
    return n;
}

static N2kResult n2kMsg(N2kReader& r, DataStore& st, uint32_t pgn, const char* hex, uint32_t now = 1000) {
    uint8_t b[223];
    int n = hexBytes(hex, b, sizeof b);
    return r.feedMessage(pgn, b, n, Source::Can, now, st);
}

// Valores de referencia: ejemplos y salidas esperadas de las pruebas de canboat (analyzer/tests)
static void testN2k() {
    // Identificador: 09F20101 = prioridad 2, PGN 127489, fuente 1
    N2kId id = n2kParseId(0x09F20101);
    CHECK(id.prio == 2 && id.pgn == 127489 && id.src == 1 && id.dst == 255);
    id = n2kParseId(0x09F8012B);
    CHECK(id.pgn == 129025 && id.src == 0x2B);
    id = n2kParseId(0x18EAFF23);                                   // peticion ISO a todos: PGN 59904, destino 255
    CHECK(id.prio == 6 && id.pgn == 59904 && id.dst == 255 && id.src == 0x23);
    id = n2kParseId(0x18EA0523);                                   // dirigida a la direccion 5
    CHECK(id.pgn == 59904 && id.dst == 5);
    CHECK(n2kIsFastPacket(129038) && n2kIsFastPacket(129029) && !n2kIsFastPacket(129025));

    DataStore st;
    N2kReader r;
    double v;
    // 127250: rumbo 178,0 magnetico, sin declinacion
    CHECK(n2kMsg(r, st, 127250, "00,56,79,ff,7f,ff,7f,fd") == N2kResult::Data);
    CHECK(st.get(Key::HeadingMag, 1000, v) && near(v, 178.0, 0.05));
    CHECK(!st.get(Key::Heading, 1000, v));
    // con declinacion de -1,0 grados (0xFF51 = -175 x 0,0001 rad): verdadero 177,0
    CHECK(n2kMsg(r, st, 127250, "00,56,79,ff,7f,51,ff,fd") == N2kResult::Data);
    CHECK(st.get(Key::Heading, 1000, v) && near(v, 177.0, 0.05));
    // 129025: 53,1801876 N 5,4284652 E
    CHECK(n2kMsg(r, st, 129025, "14,a7,b2,1f,6c,51,3c,03") == N2kResult::Data);
    CHECK(st.get(Key::Lat, 1000, v) && near(v, 53.1801876, 1e-7));
    CHECK(st.get(Key::Lon, 1000, v) && near(v, 5.4284652, 1e-7));
    CHECK(n2kMsg(r, st, 129025, "ff,ff,ff,7f,ff,ff,ff,7f") == N2kResult::None);       // sin dato
    // 129026: COG 124,2 verdadero, SOG 0
    CHECK(n2kMsg(r, st, 129026, "ff,fc,a6,54,00,00,ff,ff") == N2kResult::Data);
    CHECK(st.get(Key::Cog, 1000, v) && near(v, 124.2, 0.05));
    CHECK(st.get(Key::Sog, 1000, v) && near(v, 0, 1e-9));
    // 130306: viento aparente 3,37 m/s a 112,1 grados
    CHECK(n2kMsg(r, st, 130306, "07,51,01,74,4c,fa,ff,ff") == N2kResult::Data);
    CHECK(st.get(Key::Aws, 1000, v) && near(v, 3.37 * 3600 / 1852, 0.001));
    CHECK(st.get(Key::Awa, 1000, v) && near(v, 112.1, 0.05));
    // el mismo por babor: 247,9 grados -> -112,1
    CHECK(n2kMsg(r, st, 130306, "07,51,01,03,a9,fa,ff,ff") == N2kResult::Data);
    CHECK(st.get(Key::Awa, 1000, v) && near(v, -112.1, 0.05));
    // 127488: 30 vueltas por segundo = 1800 rpm
    CHECK(n2kMsg(r, st, 127488, "00,20,1c,ff,ff,7f,ff,ff") == N2kResult::Data);
    CHECK(st.get(Key::Rpm, 1000, v) && near(v, 1800, 0.01));
    CHECK(n2kMsg(r, st, 127488, "01,20,1c,ff,ff,7f,ff,ff") == N2kResult::None);        // segundo motor: no se usa
    // 127508: 12,50 V
    CHECK(n2kMsg(r, st, 127508, "00,e2,04,00,80,ff,ff,ff") == N2kResult::Data);
    CHECK(st.get(Key::BatteryV, 1000, v) && near(v, 12.5, 1e-9));
    // 130312: temperatura del agua 0x728B x 0,01 K = 20,08 C
    CHECK(n2kMsg(r, st, 130312, "ff,00,00,8b,72,ff,ff,ff") == N2kResult::Data);
    CHECK(st.get(Key::WaterTemp, 1000, v) && near(v, 293.23 - 273.15, 0.001));
    // 128267: 12,34 m bajo el transductor y 0,50 m de desfase
    CHECK(n2kMsg(r, st, 128267, "00,d2,04,00,00,f4,01,ff") == N2kResult::Data);
    CHECK(st.get(Key::Depth, 1000, v) && near(v, 12.84, 1e-9));
    // 128259: 2,50 m/s por el agua
    CHECK(n2kMsg(r, st, 128259, "00,fa,00,ff,ff,00,ff,ff") == N2kResult::Data);
    CHECK(st.get(Key::Stw, 1000, v) && near(v, 2.5 * 3600 / 1852, 1e-6));
    // 130314: 101325,0 Pa
    CHECK(n2kMsg(r, st, 130314, "00,00,00,02,76,0f,00,ff") == N2kResult::Data);
    CHECK(st.get(Key::Pressure, 1000, v) && near(v, 1013.25, 0.001));
    // 127245: timon -5,0 grados (-873 x 0,0001 rad)
    CHECK(n2kMsg(r, st, 127245, "00,ff,ff,7f,97,fc,ff,ff") == N2kResult::Data);
    CHECK(st.get(Key::RudderAngle, 1000, v) && near(v, -5.0, 0.01));
    // 129029: 32,1140327 N 138,0258288 E, 4,5 m, 14 satelites, HDOP 0,52
    DataStore st2;
    CHECK(n2kMsg(r, st2, 129029, "48,a4,50,d0,c9,df,0d,c0,02,05,f6,6c,eb,74,04,00,a4,65,ae,99,a9,27,13,20,aa,44,00,00,00,00,00,13,fc,0e,34,00,5e,00,1e,0f,00,00,00,ff,ff,ff,ff") == N2kResult::Data);
    CHECK(st2.get(Key::Lat, 1000, v) && near(v, 32.1140327, 1e-7));
    CHECK(st2.get(Key::Lon, 1000, v) && near(v, 138.0258288, 1e-7));
    CHECK(st2.get(Key::Altitude, 1000, v) && near(v, 4.5, 1e-6));
    CHECK(st2.get(Key::SatsUsed, 1000, v) && near(v, 14, 1e-9));
    CHECK(st2.get(Key::Hdop, 1000, v) && near(v, 0.52, 1e-9));
    CHECK(st2.source(Key::Lat, 1000) == Source::Can);
    CHECK(n2kMsg(r, st, 126992, "00,00,00,00,00,00,00,00") == N2kResult::Unknown);

    // AIS clase A: MMSI 244700322, 53,17006 N 5,412295 E, COG 276,7, parado, estado 13
    CHECK(n2kMsg(r, st, 129038, "01,a2,d4,95,0e,c6,d9,39,03,78,1b,b1,1f,01,ad,bc,00,00,8e,00,01,ff,ff,ff,7f,cd,f8,ff") == N2kResult::Ais);
    CHECK(r.ais().mmsi == 244700322u && r.ais().type == 1 && r.ais().hasPos);
    CHECK(near(r.ais().lat, 53.17006, 1e-7) && near(r.ais().lon, 5.412295, 1e-7));
    CHECK(r.ais().hasCog && near(r.ais().cogDeg, 276.7, 0.05) && r.ais().hasSog && near(r.ais().sogKn, 0, 1e-9));
    CHECK(!r.ais().hasHeading && r.ais().navStatus == 13);
    // AIS clase B: MMSI 244180106, 52,9061666 N 5,3134516 E, COG 171,7, 1,80 m/s
    CHECK(n2kMsg(r, st, 129039, "12,8a,e4,8d,0e,b4,c4,2a,03,22,d7,88,1f,77,09,75,b4,00,f8,08,00,ff,ff,00,f0,fe,ff") == N2kResult::Ais);
    CHECK(r.ais().mmsi == 244180106u && r.ais().type == 18);
    CHECK(near(r.ais().lat, 52.9061666, 1e-7) && near(r.ais().lon, 5.3134516, 1e-7));
    CHECK(near(r.ais().cogDeg, 171.7, 0.05) && near(r.ais().sogKn, 1.80 * 3600 / 1852, 1e-6) && r.ais().navStatus == -1);
    AisTargets tg;
    int ti = tg.update(r.ais(), 1000);
    CHECK(ti >= 0 && tg.at(ti).classB && tg.at(ti).hasPos);
    // AIS clase A, datos del buque
    CHECK(n2kMsg(r, st, 129794, "05,80,26,aa,0e,f8,8e,8c,00,50,43,48,51,20,00,ff,55,4b,31,34,37,20,4d,4f,52,47,45,4e,53,54,45,52,00,ff,ff,ff,1e,a4,01,50,00,28,00,40,01,31,4a,00,00,00,00,e0,01,46,49,53,48,49,4e,47,20,47,52,4f,55,4e,44,53,20,20,20,00,ff,3d,e1,ff") == N2kResult::Ais);
    CHECK(r.ais().mmsi == 246032000u && r.ais().type == 5 && r.ais().hasStatic);
    CHECK(!std::strcmp(r.ais().callsign, "PCHQ") && !std::strcmp(r.ais().name, "UK147 MORGENSTER") && r.ais().shipType == 30);
    CHECK(r.ais().hasDim && r.ais().toBow == 32 && r.ais().toStern == 10 && r.ais().toStarboard == 4 && r.ais().toPort == 4);
    CHECK(near(r.ais().draughtM, 4.8, 1e-9) && !std::strcmp(r.ais().destination, "FISHING GROUNDS"));
    // AIS clase B, parte B: tipo 37, DD7558, 13 x 4 m
    CHECK(n2kMsg(r, st, 129810, "18,f6,b5,9c,0c,25,53,52,54,45,51,23,2d,44,44,37,35,35,38,00,82,00,28,00,0a,00,64,00,00,00,00,00,03,e0,ff") == N2kResult::Ais);
    CHECK(r.ais().mmsi == 211596790u && r.ais().type == 24 && r.ais().shipType == 37 && !std::strcmp(r.ais().callsign, "DD7558"));
    CHECK(r.ais().toBow + r.ais().toStern == 13 && r.ais().toPort + r.ais().toStarboard == 4);
    // AIS clase B, parte A: el nombre
    CHECK(n2kMsg(r, st, 129809, "18,f6,b5,9c,0c,53,45,41,20,42,52,45,45,5a,45,40,40,40,40,40,40,40,40,40,40,e0,ff") == N2kResult::Ais);
    CHECK(r.ais().mmsi == 211596790u && !std::strcmp(r.ais().name, "SEA BREEZE"));

    // Mensajes de varias tramas: grabacion real de canboat (samples/candump-fast.txt), 26 bytes en 4 tramas
    N2kFastPacket fp;
    uint8_t f[8];
    hexBytes("60 1A 00 B7 06 67 0D 0B", f, 8); CHECK(!fp.feed(127489, 1, f, 8, 100));
    hexBytes("61 86 BA 04 26 00 10 5F", f, 8); CHECK(!fp.feed(127489, 1, f, 8, 101));
    hexBytes("62 09 00 00 00 8F 02 7F", f, 8); CHECK(!fp.feed(127489, 1, f, 8, 102));
    hexBytes("63 00 00 00 00 7F 7F FF", f, 8); CHECK(fp.feed(127489, 1, f, 8, 103));
    CHECK(fp.length() == 26 && fp.data()[0] == 0x00 && fp.data()[1] == 0xB7 && fp.data()[6] == 0x86 && fp.data()[25] == 0x7F);
    CHECK(fp.dropped() == 0);
    // Falta una trama: el mensaje se descarta y el siguiente entra bien
    hexBytes("80 1A 00 B7 06 67 0D 0B", f, 8); CHECK(!fp.feed(127489, 1, f, 8, 200));
    hexBytes("82 09 00 00 00 8F 02 7F", f, 8); CHECK(!fp.feed(127489, 1, f, 8, 201));
    CHECK(fp.dropped() == 1);
    hexBytes("83 00 00 00 00 7F 7F FF", f, 8); CHECK(!fp.feed(127489, 1, f, 8, 202));
    hexBytes("A0 03 11 22 33 FF FF FF", f, 8); CHECK(fp.feed(127489, 1, f, 8, 300));     // cabe en una trama
    CHECK(fp.length() == 3 && fp.data()[2] == 0x33);
    // Dos emisores a la vez, tramas mezcladas
    hexBytes("00 09 01 02 03 04 05 06", f, 8); CHECK(!fp.feed(129038, 7, f, 8, 400));
    hexBytes("20 09 A1 A2 A3 A4 A5 A6", f, 8); CHECK(!fp.feed(129038, 9, f, 8, 401));
    hexBytes("01 07 08 09 FF FF FF FF", f, 8); CHECK(fp.feed(129038, 7, f, 8, 402));
    CHECK(fp.length() == 9 && fp.data()[0] == 0x01 && fp.data()[8] == 0x09);
    hexBytes("21 A7 A8 A9 FF FF FF FF", f, 8); CHECK(fp.feed(129038, 9, f, 8, 403));
    CHECK(fp.length() == 9 && fp.data()[0] == 0xA1 && fp.data()[8] == 0xA9);
    // Continuacion que llega tarde (mas de 750 ms): fuera
    hexBytes("40 09 01 02 03 04 05 06", f, 8); CHECK(!fp.feed(129038, 7, f, 8, 1000));
    hexBytes("41 07 08 09 FF FF FF FF", f, 8); CHECK(!fp.feed(129038, 7, f, 8, 1800));

    // De la trama CAN al dato: el AIS de clase B de antes, troceado en tramas
    N2kReader rr;
    DataStore st3;
    uint8_t msg[64];
    int n = hexBytes("12,8a,e4,8d,0e,b4,c4,2a,03,22,d7,88,1f,77,09,75,b4,00,f8,08,00,ff,ff,00,f0,fe,ff", msg, sizeof msg);
    uint32_t canId = (4u << 26) | (129039u << 8) | 23;
    N2kResult res = N2kResult::None;
    for (int off = 0, fr = 0; off < n; fr++) {
        uint8_t c[8];
        std::memset(c, 0xFF, 8);
        c[0] = (uint8_t)((3 << 5) | fr);
        int k = 1;
        if (fr == 0) c[k++] = (uint8_t)n;
        while (k < 8 && off < n) c[k++] = msg[off++];
        res = rr.feedFrame(canId, c, 8, Source::Can, 500 + fr, st3);
        CHECK(res == (off < n ? N2kResult::Pending : N2kResult::Ais));
    }
    CHECK(rr.ais().mmsi == 244180106u && rr.stats().frames == 4 && rr.stats().ais == 1);
    uint8_t one[8];
    hexBytes("14,a7,b2,1f,6c,51,3c,03", one, 8);
    CHECK(rr.feedFrame(0x09F8012B, one, 8, Source::Can, 600, st3) == N2kResult::Data);
    CHECK(st3.get(Key::Lat, 600, v) && near(v, 53.1801876, 1e-7));
}

int main() {
    testGeo(); testMotion(); testWind(); testUnits(); testAstro(); testStore(); testNmea(); testAisExtra(); testInputs(); testPilot(); testSignalK(); testRoute(); testGnss(); testLights(); testNavWarn(); testTiles(); testChartUi(); testWxStation(); testSim(); testAisTargets(); testGpx(); testStream(); testAnchor(); testN2k();
    std::printf("%d comprobaciones, %d fallos\n", total, failed);
    return failed ? 1 : 0;
}
