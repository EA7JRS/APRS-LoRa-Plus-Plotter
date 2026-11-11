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
    testGeo(); testMotion(); testWind(); testUnits(); testAstro(); testStore(); testNmea(); testGpx(); testStream();
    std::printf("%d comprobaciones, %d fallos\n", total, failed);
    return failed ? 1 : 0;
}
