/* Copyright (C) 2026 Manuel - EA7JRS
 *
 * This file is part of APRS LoRa+ Plotter.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
// NAVTEX: mensajes que llegan ya decodificados por NMEA 0183 en sentencias NRX (un receptor NAVTEX de a bordo, o la app
// APRS LoRa+ con su decodificador por el servidor NMEA del movil). Dos formas: la de los receptores Furuno
// ($CRNRX,TA21,linea,lineas,518,dia,mes,anio,hhmmss,error,texto: una linea de mensaje por sentencia; si acaba en espacio,
// la siguiente la continua: asi parte la app las lineas largas) y la de IEC 61162-1
// ($CRNRX,total,num,secuencia,TA21,indice frecuencia,hhmmss.ss,dia,mes,anio,caracteres,malos,A/V,cuerpo con ^hh).
// Ademas: asuntos (letra B2), horario de cada estacion (letra B1 x 10 min, cada 4 h) y lista de los ultimos mensajes.
// La lista sigue las reglas de un receptor NAVTEX (OMI MSC.508(105), 6.2; Manual NAVTEX de 2023, 6.2 y 7.5; UIT-R M.540-2):
// la identidad B1B2B3B4 de un mensaje bien recibido (entero y con menos de un 4 % de caracteres erroneos) se recuerda 72 h y
// su repeticion no vuelve a mostrarse; una copia mejor sustituye a una peor; B3B4 = 00 se muestra siempre; lo que el usuario
// marca para conservar no se pierde al llenarse la lista; el usuario no puede borrar mensajes (MSC.508(105), 6.1.1): salen
// solos, el mas antiguo sin conservar cuando la lista se llena. Alarma de busqueda y salvamento para otros equipos: ALR 003
// de IEC 61097-6 ($CRALR,hhmmss.00,003,A,V,texto), que se reconoce con $--ACK,003; y, si el receptor habla la gestion de
// alertas del puente (MSC.302(87), IEC 62923), la sentencia ALF, que se reconoce con ACN. Filtros del receptor: NRM y CRQ.
#ifndef PLOTTER_NAVTEX_H_
#define PLOTTER_NAVTEX_H_

#include <cstdint>
#include <string>
#include <vector>

#include "geo.h"

namespace plotter {

struct NavtexMessage {
    char station = '?';             // B1
    char subject = '?';             // B2
    int number = 0;                 // B3B4
    double khz = 0;                 // 518, 490, 4209.5 (0 = no se sabe)
    std::string text;               // lineas separadas por '\n'
    int64_t utc = 0;                // hora de recepcion (UNIX, 0 = no viene)
    bool complete = true;           // todas las lineas y estado A
    double errorPct = -1;           // tasa de error del receptor (-1 = no viene)
    int64_t heard = 0;              // primera vez que llego a esta lista (UNIX, 0 = sin reloj)
    int repeats = 0;                // veces que se ha vuelto a oir sin sustituir a la copia guardada
    bool kept = false;              // marcado para conservar
    std::vector<LatLon> positions;  // sacadas del texto (navWarningPositions)
    bool urgent() const { return subject == 'D'; }   // busqueda y salvamento, socorro, pirateria
    bool satisfactory() const { return complete && !text.empty() && (errorPct < 0 || errorPct < 4.0); }   // MSC.508(105), 6.2.3
    bool alwaysShown() const { return number == 0; }   // B3B4 = 00
    std::string id() const;         // "GA05"
};

const char* navtexSubjectEs(char b2);
const char* navtexSubjectEn(char b2);
int navtexFirstSlotMin(char b1);                    // minutos UTC de la primera emision (A = 0 ... X = 230); -1 si no es letra de estacion
int64_t navtexNextSlot(char b1, int64_t nowUnix);   // UNIX de la siguiente emision (la que esta en curso cuenta 10 min); -1 si no hay

// 1 si el mensaje llego en los diez minutos de su letra (se admiten hasta 13 desde el comienzo), 0 si no, -1 si no se sabe
int navtexInOwnSlot(char b1, int64_t utc);

const int64_t kNavtexIdentitySec = 72 * 3600;
enum class NavtexVerdict { New, Repeat, Better };
bool navtexBetterCopy(const NavtexMessage& fresh, const NavtexMessage& old);
// Que hacer con uno nuevo cuando old es la copia guardada con la misma identidad y frecuencia (nullptr = no hay)
NavtexVerdict navtexVerdict(const NavtexMessage& fresh, const NavtexMessage* old, int64_t nowUnix);

// Alarma 003 de IEC 61097-6: lee $CRALR,...,003,A|V,A|V,... (active = hay alarma, acknowledged = ya reconocida)
bool parseNavtexAlr(const char* sentence, bool& active, bool& acknowledged);
std::string navtexAckSentence();                    // "$IIACK,003*hh\r\n": reconoce la alarma en el receptor

// Gestion de alertas del puente: $CRALF,1,1,0,hhmmss.00,B,W,V,,3,1,rev,0,NAVTEX SAR (solo la alerta 3 de un receptor NAVTEX,
// sin codigo de fabricante). state: V sin reconocer, S silenciada, A reconocida, N normal, U, O. unack = V o S.
bool parseNavtexAlf(const char* sentence, char& state, int& instance);
inline bool navtexAlfUnack(char state) { return state == 'V' || state == 'S'; }
std::string navtexAcnSentence(int instance = 1);    // "$IIACN,,,3,1,A,C*hh\r\n": reconoce la alerta (orden A)

// Filtros del receptor (IEC 61097-6, anexo C.2): $--NRM,funcion,frecuencia,estaciones,asuntos; funcion 1 = lo que se guarda;
// frecuencia 1 = 490, 2 = 518, 3 = 4209,5 kHz; mascaras de 32 bits en hexadecimal, bit 0 = letra A. Los asuntos A, B, D y L
// no se pueden excluir (IEC 61097-6, 4.3.7).
const uint32_t kNavtexAllLetters = 0x03FFFFFFu;
const uint32_t kNavtexFixedSubjects = (1u << 0) | (1u << 1) | (1u << 3) | (1u << 11);   // A, B, D, L
struct NavtexMask { int function = 1; double khz = 518; uint32_t stations = kNavtexAllLetters; uint32_t subjects = kNavtexAllLetters; };
bool parseNavtexNrm(const char* sentence, NavtexMask& out);
std::string navtexNrmSentence(const NavtexMask& m);  // "$IINRM,1,2,03FFFFFF,0000083F*hh\r\n"
std::string navtexNrmQuery();                        // "$IICRQ,NRM*hh\r\n": el receptor contesta con sus tres NRM
int navtexFreqIndex(double khz);                     // 1, 2 o 3

// Monta mensajes a partir de sentencias NRX (con o sin "$", con o sin suma). feed() devuelve true cuando hay un mensaje en take().
class NrxAssembler {
public:
    bool feed(const char* sentence);
    bool take(NavtexMessage& out);
private:
    struct Part { std::string key; int total = 0; std::vector<std::string> lines; NavtexMessage m; uint32_t age = 0; };
    std::vector<Part> parts_;
    std::vector<NavtexMessage> ready_;
    uint32_t tick_ = 0;
    Part& part(const std::string& key);
};

// Ultimos mensajes, el mas nuevo primero; uno por estacion + id + frecuencia, con las reglas del receptor de arriba
class NavtexLog {
public:
    static const size_t kMax = 60;
    static const size_t kMaxKept = kMax / 4;        // el 25 % de la memoria, como pide MSC.508(105) para los conservados
    NavtexVerdict add(const NavtexMessage& m, int64_t nowUnix = 0);
    const std::vector<NavtexMessage>& items() const { return items_; }
    uint32_t seq() const { return seq_; }           // cambia con cada cambio de la lista
    uint32_t fresh() const { return fresh_; }       // cambia solo con un mensaje nuevo o una copia mejor (lo que hay que mostrar)
    bool keep(size_t i, bool on);                   // false si ya hay kMaxKept conservados
    size_t keptCount() const;
    // Para guardar la lista en la tarjeta: una linea por mensaje ("M;B1B2B3B4;kHz;utc;oido;entero;error;repetido;conservado;texto",
    // con \n y \\ escapados en el texto)
    std::string save() const;
    int load(const std::string& text, int64_t nowUnix = 0);   // sustituye la lista; devuelve los mensajes leidos
private:
    std::vector<NavtexMessage> items_;
    uint32_t seq_ = 0, fresh_ = 0;
};

}  // namespace plotter
#endif
