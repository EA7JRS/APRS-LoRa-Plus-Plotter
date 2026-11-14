/*
 * Copyright (C) 2026 Manuel - EA7JRS
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Indicativo de radioaficionado validado en RadioID.net (DMR ID / CCS7), como en la app Android: el indicativo de
 * Ajustes > Embarcacion debe tener DMR ID o CCS7. Sin el, el plotter no manda al transponder APRS LoRa+ MAPRS ordenes que
 * cambien lo que emite. Se guarda en NVS y se vuelve a comprobar cada 90 dias (sin conexion, se conserva).
 */
#pragma once
#include <stddef.h>
#include <stdint.h>

namespace licence {
void loop();                                    // desde links::loop()
bool valid();                                   // el indicativo de los ajustes es el validado
bool validate(uint32_t dmrId);                  // pide la comprobacion (dmrId 0 = sin comprobar el ID); false si no se puede
bool busy();
void text(char* out, size_t size);              // estado para la pantalla
}
