/* Copyright (C) 2026 Manuel - EA7JRS
 *
 * This file is part of APRS LoRa+ Plotter.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
// Meteorologia aeronautica de la API de datos del Aviation Weather Center de NOAA (aviationweather.gov, dominio publico,
// 100 peticiones por minuto): METAR y TAF de los aeropuertos de una zona y SIGMET internacionales, para las capas de la
// carta. Las respuestas se leen con minijson y se prueban en el PC.
#ifndef PLOTTER_AVIATION_H_
#define PLOTTER_AVIATION_H_

#include <cstdint>
#include <string>
#include <vector>

#include "geo.h"

namespace plotter {

enum class FlightCat : uint8_t { Unknown, Vfr, Mvfr, Ifr, Lifr };
const char* flightCatName(FlightCat c);                 // "VFR"... ("" si no se sabe)
uint32_t flightCatColor(FlightCat c);                   // verde, azul, rojo, magenta (0x9DB2C7 si no se sabe)
// Categoria de vuelo de la FAA: techo (capa BKN / OVC / OVX o visibilidad vertical, en pies; < 0 = sin techo) y
// visibilidad en millas terrestres (< 0 = no se sabe)
FlightCat flightCategory(int ceilingFt, double visibSm);

struct CloudLayer { std::string cover; int baseFt = -1; };

struct Metar {
    std::string icao, name, raw;
    double lat = 0, lon = 0;
    int64_t obsTime = 0;            // segundos Unix
    int wdir = -1;                  // grados; -1 sin dato, -2 variable (VRB)
    int wspdKt = -1, wgstKt = -1;
    double tempC = -999, dewpC = -999, qnhHpa = -1;
    double visibSm = -1;            // millas terrestres; 10 = «10+»
    bool visibPlus = false;         // «10+»: 10 millas o mas
    std::string wx;                 // tiempo presente («RA BR»)
    std::vector<CloudLayer> clouds;
    int vertVisFt = -1;
    int ceilingFt = -1;             // calculado de las nubes
    FlightCat cat = FlightCat::Unknown;
};

struct Taf { std::string icao, raw; int64_t issueTime = 0; };

struct Sigmet {
    std::string id;                 // firId + serie
    std::string fir, firName, hazard, qualifier, raw;
    int baseFt = -1, topFt = -1;    // -1 sin dato
    int64_t from = 0, to = 0;
    std::string dir;                // movimiento
    int spdKt = -1;
    std::vector<LatLon> pts;        // poligono (cerrado o no), linea o punto
};

constexpr const char* kAwcBase = "https://aviationweather.gov/api/data/";
// Direcciones para un recuadro (bbox de la API: latMin,lonMin,latMax,lonMax)
std::string awcMetarUrl(double latMin, double lonMin, double latMax, double lonMax);
std::string awcTafUrl(double latMin, double lonMin, double latMax, double lonMax);
std::string awcSigmetUrl(const char* loc = "all");     // SIGMET internacionales, GeoJSON (loc: all, eur, afr, namer...)
const char* awcSigmetRegion(double lat, double lon);   // la region de la API para una posicion

bool metarParseJson(const std::string& json, std::vector<Metar>& out);
bool tafParseJson(const std::string& json, std::vector<Taf>& out);
bool sigmetParseGeoJson(const std::string& json, std::vector<Sigmet>& out);

// Textos para la ficha, en espanol (los de la pantalla) e ingles (la app)
std::string metarWindText(const Metar& m, bool es);    // «Viento 100° 22 kn, rachas 32 kn»
std::string metarSkyText(const Metar& m, bool es);     // visibilidad y nubes
std::string metarTempText(const Metar& m, bool es);    // temperatura, rocio y QNH
const char* sigmetHazardName(const std::string& hazard, bool es);   // «Tormentas», «Turbulencia»...
// Distancia en millas de un punto al SIGMET (0 si esta dentro del poligono)
double sigmetDistanceNm(const Sigmet& s, LatLon p);

}  // namespace plotter
#endif
