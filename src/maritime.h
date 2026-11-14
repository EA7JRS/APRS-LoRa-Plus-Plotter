/*
 * Copyright (C) 2026 Manuel - EA7JRS
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Distintivo de llamada maritimo del MMSI (Ajustes > Embarcacion), comprobado con MarineTraffic (clave de su API, de pago)
 * o, si MarineTraffic no lo tiene, confirmado a mano en el MARS de la UIT. Sin comprobar, el transponder no lo emite (salvo
 * que en su WebUI se permita con aviso).
 */
#pragma once
#include <stddef.h>

namespace maritime {
void loop();                                    // desde links::loop()
bool checkMarineTraffic();                      // false si no se puede pedir ahora
void confirmMars(bool on);                      // confirmado (o no) a mano en el MARS de la UIT
bool verified();
bool busy();
void text(char* out, size_t size);
}
