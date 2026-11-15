/* Copyright (C) 2026 Manuel - EA7JRS
 *
 * This file is part of APRS LoRa+ Plotter.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
// Navegacion astronomica: lo que da un almanaque nautico, calculado a bordo. Angulo horario en Greenwich (AHG) y
// declinacion del Sol, AHG de Aries y angulo sidereo (AHS) y declinacion de las 57 estrellas de navegacion y la Polar,
// con precision de almanaque (0,1'). Sol: serie VSOP87 recortada (Meeus, "Astronomical Algorithms", ap. III), nutacion y
// aberracion. Estrellas: movimiento propio, precesion, nutacion y aberracion anual (Meeus, cap. 21 a 23).
// Contrastado con PyEphem y con un almanaque nautico de 2026 (test/host). Sin Arduino.
#pragma once

namespace plotter {

struct NavStar {
    const char* name;
    float mag;
    double raHours, decDeg;        // J2000
    double pmRaMas, pmDecMas;      // movimiento propio, milisegundos de arco por anio (el de AR, ya por cos dec)
};

int navStarCount();                              // 58: las 57 de navegacion por orden alfabetico y la Polar
const NavStar& navStar(int i);
int navStarFind(const char* name);               // sin distinguir mayusculas; -1 si no esta

// jd: dia juliano en UTC (julianDay de astro.h). Angulos en grados: AHG y AHS de 0 a 360 hacia el oeste, dec + al norte.
double ariesGha(double jd);                      // AHG de Aries (tiempo sidereo aparente en Greenwich)
void sunGhaDec(double jd, double& ghaDeg, double& decDeg);
double sunSemiDiameterArcmin(double jd);
double equationOfTimeMin(double jd);             // tiempo verdadero - tiempo medio, en minutos
void starShaDec(const NavStar& s, double jd, double& shaDeg, double& decDeg);   // lugar aparente de la fecha

// Altura verdadera y azimut de un astro (AHG y declinacion) desde una posicion; lon + al este
void bodyAltAz(double ghaDeg, double decDeg, double lat, double lon, double& altDeg, double& azDeg);

}  // namespace plotter
