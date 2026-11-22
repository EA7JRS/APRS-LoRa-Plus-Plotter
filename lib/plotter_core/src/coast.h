/* Copyright (C) 2026 Manuel - EA7JRS
 *
 * This file is part of APRS LoRa+ Plotter.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
// Estaciones costeras y centros de salvamento: una lista propia en la tarjeta, una por linea,
//   nombre;latitud;longitud;tipo;MMSI;canal;canal 2;kHz
// con el tipo V (costera de VHF: canal de trabajo, y canal 2 el de Salvamento Maritimo si lo hay), M (costera de onda
// media: kHz) o S (centro de coordinacion de salvamento). Cualquier dato puede ir vacio menos el nombre, la posicion
// y el tipo. De donde salgan los datos es cosa del usuario (tools/radiosenales los saca de su libro de Radiosenales
// del IHM): el programa no lleva ninguno. Sirve para decir a quien se llama desde donde esta el barco. Sin Arduino.
#pragma once
#include <cstdint>
#include <string>

namespace plotter {

struct CoastStation {
    char name[32] = "";
    double lat = 0, lon = 0;
    char kind = 'V';             // 'V', 'M' o 'S'
    uint32_t mmsi = 0;           // 0 = no se sabe; las costeras empiezan por 00 (002241022): se escribe con 9 cifras
    int channel = 0;             // canal de trabajo de VHF; 0 = no se sabe
    int channel2 = 0;            // segundo canal (Salvamento Maritimo); 0 = no hay
    double khz = 0;              // frecuencia de onda media; 0 = no se sabe
};

// Lee una linea de la lista; false si no es una estacion (comentario con #, linea vacia o datos imposibles).
bool parseCoastRecord(const char* line, CoastStation& out);
// La escribe en ese formato; devuelve los caracteres escritos (0 si no cabe).
int formatCoastRecord(char* out, int size, const CoastStation& s);

// La estacion de ese tipo mas cercana a la situacion; -1 si no hay ninguna. distNm y bearingDeg (verdadera, desde el
// barco) salen de ella.
int nearestCoast(const CoastStation* list, int n, char kind, double lat, double lon, double& distNm, double& bearingDeg);

// Una linea para la pantalla: "Tarifa a 12 mn · canal 83 (Salvamento 6) · LSD 002241026". Con rangeNm > 0 y la
// estacion mas lejos, anade "fuera de alcance": el VHF llega unas 35 a 40 millas.
std::string coastLine(const CoastStation& s, double distNm, double rangeNm, bool english);

}  // namespace plotter
