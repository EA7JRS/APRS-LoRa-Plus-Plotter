/*
 * Copyright (C) 2026 Manuel - EA7JRS
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Datos de la embarcacion (Ajustes > Embarcacion) sincronizados con el transpondedor APRS LoRa+ MAPRS (o un tracker con el
 * perfil nautico AIS) por BLE: orden y evento "vessel". El plotter manda; sin datos propios toma los del transpondedor.
 */
#pragma once
#include "vessel.h"

namespace vessel_link {
void loop();                                    // desde links::loop()
void changed();                                 // el usuario ha guardado los datos del barco
plotter::vessel::Data local();                  // los de los ajustes
const plotter::vessel::Sync& sync();
}
