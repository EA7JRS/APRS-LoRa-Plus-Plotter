/* Copyright (C) 2026 Manuel - EA7JRS
 *
 * This file is part of APRS LoRa+ Plotter.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "navtex.h"

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "astro.h"
#include "navwarn.h"

namespace plotter {

std::string NavtexMessage::id() const {
    char b[8];
    snprintf(b, sizeof b, "%c%c%02d", station, subject, number % 100);
    return b;
}

const char* navtexSubjectEs(char s) {
    switch (s) {
        case 'A': return "Aviso a la navegaci\xC3\xB3n";
        case 'B': return "Aviso meteorol\xC3\xB3gico";
        case 'C': return "Hielos";
        case 'D': return "Salvamento, socorro, pirater\xC3\xAD" "a";
        case 'E': return "Previsi\xC3\xB3n meteorol\xC3\xB3gica";
        case 'F': return "Pr\xC3\xA1" "cticos";
        case 'G': return "AIS";
        case 'H': return "LORAN";
        case 'J': return "Navegaci\xC3\xB3n por sat\xC3\xA9lite";
        case 'K': return "Otras radioayudas";
        case 'L': return "Aviso a la navegaci\xC3\xB3n (adicional)";
        case 'T': return "Prueba";
        case 'V': return "Aviso a pescadores";
        case 'W': return "Medio ambiente";
        case 'X': case 'Y': return "Servicio especial";
        case 'Z': return "Sin mensajes";
        default: return "Asunto desconocido";
    }
}

const char* navtexSubjectEn(char s) {
    switch (s) {
        case 'A': return "Navigational warning";
        case 'B': return "Meteorological warning";
        case 'C': return "Ice report";
        case 'D': return "Search and rescue, piracy";
        case 'E': return "Meteorological forecast";
        case 'F': return "Pilot service";
        case 'G': return "AIS";
        case 'H': return "LORAN";
        case 'J': return "Satellite navigation";
        case 'K': return "Other navaids";
        case 'L': return "Navigational warning (extra)";
        case 'T': return "Test";
        case 'V': return "Notice to fishermen";
        case 'W': return "Environmental";
        case 'X': case 'Y': return "Special service";
        case 'Z': return "No messages on hand";
        default: return "Unknown subject";
    }
}

int navtexFirstSlotMin(char b1) {
    int k = toupper((unsigned char)b1) - 'A';
    return k >= 0 && k <= 23 ? k * 10 : -1;
}

int64_t navtexNextSlot(char b1, int64_t now) {
    int first = navtexFirstSlotMin(b1);
    if (first < 0) return -1;
    int64_t day = now - ((now % 86400) + 86400) % 86400;
    for (int k = -1; k <= 6; k++) {
        int64_t t = day + (int64_t)(first + k * 240) * 60;
        if (t + 600 > now) return t;
    }
    return -1;
}

int navtexInOwnSlot(char b1, int64_t utc) {
    int first = navtexFirstSlotMin(b1);
    if (first < 0 || utc <= 0) return -1;
    int min = (int)((utc / 60) % 240);
    return ((min - first + 240) % 240) <= 13 ? 1 : 0;
}

// ---- reglas del receptor

namespace {
int quality(const NavtexMessage& m) { return m.satisfactory() ? 2 : m.complete ? 1 : 0; }   // 2 bien recibido, 1 entero con demasiados errores, 0 cortado
double errOf(const NavtexMessage& m) { return m.errorPct < 0 ? 0 : m.errorPct; }
}  // namespace

bool navtexBetterCopy(const NavtexMessage& fresh, const NavtexMessage& old) {
    int qn = quality(fresh), qo = quality(old);
    if (qn != qo) return qn > qo;
    if (qn == 0) return fresh.text.size() * (100.0 - errOf(fresh)) > old.text.size() * (100.0 - errOf(old));   // cortados: el que trae mas texto bueno
    return errOf(fresh) < errOf(old);
}

NavtexVerdict navtexVerdict(const NavtexMessage& fresh, const NavtexMessage* old, int64_t now) {
    if (!old) return NavtexVerdict::New;
    if (now > 0 && old->heard > 0 && now - old->heard >= kNavtexIdentitySec) return NavtexVerdict::New;   // el numero se ha vuelto a usar
    if (fresh.alwaysShown()) return NavtexVerdict::Better;
    if (old->satisfactory()) return NavtexVerdict::Repeat;
    return navtexBetterCopy(fresh, *old) ? NavtexVerdict::Better : NavtexVerdict::Repeat;
}

// ---- NRX

namespace {

bool isCode(const std::string& s) {
    return s.size() == 4 && isalnum((unsigned char)s[0]) && isalpha((unsigned char)s[1]) && isdigit((unsigned char)s[2]) && isdigit((unsigned char)s[3]);
}

int hexv(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    c = (char)toupper((unsigned char)c);
    return c >= 'A' && c <= 'F' ? c - 'A' + 10 : -1;
}

// "^0D^0A" y demas caracteres reservados de IEC 61162-1
std::string unescape(const std::string& s) {
    std::string o;
    for (size_t i = 0; i < s.size(); i++) {
        if (s[i] == '^' && i + 2 < s.size() && hexv(s[i + 1]) >= 0 && hexv(s[i + 2]) >= 0) {
            char c = (char)(hexv(s[i + 1]) * 16 + hexv(s[i + 2]));
            i += 2;
            if (c == '\r') continue;
            o += c;
        } else o += s[i];
    }
    return o;
}

double khzOf(const std::string& f) {
    if (f == "1" || f == "490") return 490;
    if (f == "2" || f == "518") return 518;
    if (f == "3" || f == "4209" || f == "4209.5") return 4209.5;
    return 0;
}

int64_t utcOf(const std::string& d, const std::string& mo, const std::string& y, const std::string& t) {
    if (d.empty() || mo.empty() || y.empty() || t.size() < 6) return 0;
    int yy = atoi(y.c_str()), mm = atoi(mo.c_str()), dd = atoi(d.c_str());
    if (yy < 100) yy += 2000;
    if (yy < 2000 || mm < 1 || mm > 12 || dd < 1 || dd > 31) return 0;
    int h = (t[0] - '0') * 10 + (t[1] - '0'), mi = (t[2] - '0') * 10 + (t[3] - '0'), se = (t[4] - '0') * 10 + (t[5] - '0');
    double jd = julianDay(yy, mm, dd, h + mi / 60.0 + se / 3600.0);
    return (int64_t)((jd - 2440587.5) * 86400.0 + 0.5);
}

}  // namespace

NrxAssembler::Part& NrxAssembler::part(const std::string& key) {
    for (Part& p : parts_) if (p.key == key) return p;
    if (parts_.size() >= 4) parts_.erase(parts_.begin());          // mensajes a medias que no se terminaron
    parts_.push_back(Part());
    parts_.back().key = key;
    return parts_.back();
}

bool NrxAssembler::feed(const char* sentence) {
    if (!sentence) return false;
    std::string s(sentence);
    while (!s.empty() && (s.back() == '\r' || s.back() == '\n')) s.pop_back();
    size_t star = s.rfind('*');
    if (star != std::string::npos && star + 3 == s.size()) {        // suma: se comprueba si viene
        unsigned sum = 0;
        size_t from = (s[0] == '$' || s[0] == '!') ? 1 : 0;
        for (size_t i = from; i < star; i++) sum ^= (unsigned char)s[i];
        if (hexv(s[star + 1]) < 0 || hexv(s[star + 2]) < 0 || (unsigned)(hexv(s[star + 1]) * 16 + hexv(s[star + 2])) != sum) return false;
        s.resize(star);
    }
    size_t c = s.find(',');
    if (c == std::string::npos || c < 3 || s.compare(c - 3, 3, "NRX") != 0) return false;
    std::vector<std::string> f;
    size_t p = c + 1;
    while (true) {
        size_t q = s.find(',', p);
        if (q == std::string::npos) { f.push_back(s.substr(p)); break; }
        f.push_back(s.substr(p, q - p));
        p = q + 1;
    }
    tick_++;
    if (f.size() >= 10 && isCode(f[0])) {                           // Furuno: codigo, linea, lineas, frecuencia, dia, mes, anio, hora, error, texto
        std::string text = f[9];
        for (size_t i = 10; i < f.size(); i++) text += "," + f[i];
        int line = atoi(f[1].c_str()), total = atoi(f[2].c_str());
        if (line < 1 || total < 1 || line > total || total > 999) return false;
        Part& pt = part("F" + f[0] + f[3]);
        if (line == 1 || pt.total == 0) {
            pt.total = total;
            pt.lines.assign((size_t)total, std::string());
            pt.m = NavtexMessage();
            pt.m.station = f[0][0]; pt.m.subject = f[0][1]; pt.m.number = atoi(f[0].c_str() + 2);
            pt.m.khz = khzOf(f[3]);
            pt.m.utc = utcOf(f[4], f[5], f[6], f[7]);
            pt.m.errorPct = f[8].empty() ? -1 : atof(f[8].c_str());
        }
        if ((size_t)line <= pt.lines.size()) pt.lines[(size_t)line - 1] = unescape(text) + "\x01";   // marca de linea recibida
        if (line == pt.total) {
            NavtexMessage m = pt.m;
            m.complete = true;
            for (size_t i = 0; i < pt.lines.size(); i++) {
                std::string l = pt.lines[i];
                if (l.empty()) { m.complete = false; continue; }
                l.pop_back();
                if (!m.text.empty() && m.text.back() != ' ') m.text += '\n';   // un trozo que acaba en espacio sigue en la misma linea
                m.text += l;
            }
            navWarningPositions(m.text, m.positions);
            ready_.push_back(m);
            for (size_t i = 0; i < parts_.size(); i++) if (&parts_[i] == &pt) { parts_.erase(parts_.begin() + (long)i); break; }
            return true;
        }
        return false;
    }
    if (f.size() >= 13) {                                           // IEC 61162-1: total, numero, secuencia, codigo, frecuencia, hora, dia, mes, anio, caracteres, malos, estado, cuerpo
        int total = atoi(f[0].c_str()), num = atoi(f[1].c_str());
        if (num < 1 || total < 1 || num > total || total > 999) return false;
        std::string body = f[12];
        for (size_t i = 13; i < f.size(); i++) body += "," + f[i];
        Part& pt = part("I" + f[2]);
        if (num == 1 || pt.total == 0) {
            if (!isCode(f[3])) return false;
            pt.total = total;
            pt.lines.assign((size_t)total, std::string());
            pt.m = NavtexMessage();
            pt.m.station = f[3][0]; pt.m.subject = f[3][1]; pt.m.number = atoi(f[3].c_str() + 2);
            pt.m.khz = khzOf(f[4]);
            pt.m.utc = utcOf(f[6], f[7], f[8], f[5]);
            pt.m.complete = f[11] != "V";
            const int chars = atoi(f[9].c_str()), badc = atoi(f[10].c_str());
            if (chars > 0 && !f[10].empty() && badc >= 0 && badc <= chars) pt.m.errorPct = 100.0 * badc / chars;
        }
        pt.lines[(size_t)num - 1] = body + "\x01";
        if (num == pt.total) {
            NavtexMessage m = pt.m;
            std::string all;
            for (std::string l : pt.lines) {
                if (l.empty()) { m.complete = false; continue; }
                l.pop_back();
                all += l;
            }
            m.text = unescape(all);
            while (!m.text.empty() && (m.text.back() == '\n' || m.text.back() == ' ')) m.text.pop_back();
            while (!m.text.empty() && (m.text[0] == '\n' || m.text[0] == ' ')) m.text.erase(0, 1);
            navWarningPositions(m.text, m.positions);
            ready_.push_back(m);
            for (size_t i = 0; i < parts_.size(); i++) if (&parts_[i] == &pt) { parts_.erase(parts_.begin() + (long)i); break; }
            return true;
        }
    }
    return false;
}

bool NrxAssembler::take(NavtexMessage& out) {
    if (ready_.empty()) return false;
    out = ready_.front();
    ready_.erase(ready_.begin());
    return true;
}

namespace {
bool sameId(const NavtexMessage& a, const NavtexMessage& b) { return a.station == b.station && a.subject == b.subject && a.number == b.number && a.khz == b.khz; }
}  // namespace

NavtexVerdict NavtexLog::add(const NavtexMessage& in, int64_t now) {
    NavtexMessage m = in;
    if (m.heard == 0) m.heard = now;
    size_t at = items_.size();
    for (size_t i = 0; i < items_.size(); i++) if (sameId(items_[i], m)) { at = i; break; }
    const NavtexVerdict v = navtexVerdict(m, at < items_.size() ? &items_[at] : nullptr, now);
    seq_++;
    if (v == NavtexVerdict::Repeat) { items_[at].repeats++; return v; }
    if (at < items_.size()) {
        if (v == NavtexVerdict::Better) { m.kept = items_[at].kept; if (!m.alwaysShown()) m.heard = items_[at].heard; m.repeats = items_[at].repeats; }
        if (v == NavtexVerdict::Better || !items_[at].kept) items_.erase(items_.begin() + (long)at);   // un conservado no lo pisa el numero reutilizado
    }
    items_.insert(items_.begin(), m);
    while (items_.size() > kMax) {                                    // sale el mas antiguo sin conservar
        size_t k = items_.size();
        while (k > 1 && items_[k - 1].kept) k--;
        items_.erase(items_.begin() + (long)(k - 1));
    }
    fresh_++;
    return v;
}

size_t NavtexLog::keptCount() const {
    size_t n = 0;
    for (const NavtexMessage& m : items_) if (m.kept) n++;
    return n;
}

bool NavtexLog::keep(size_t i, bool on) {
    if (i >= items_.size()) return false;
    if (on && !items_[i].kept && keptCount() >= kMaxKept) return false;
    items_[i].kept = on;
    seq_++;
    return true;
}

std::string NavtexLog::save() const {
    std::string o = "# NAVTEX 1\n";
    char b[160];
    for (const NavtexMessage& m : items_) {
        snprintf(b, sizeof b, "M;%s;%.1f;%lld;%lld;%d;%.1f;%d;%d;", m.id().c_str(), m.khz, (long long)m.utc, (long long)m.heard, m.complete ? 1 : 0,
                 m.errorPct, m.repeats, m.kept ? 1 : 0);
        o += b;
        for (char c : m.text) { if (c == '\n') o += "\\n"; else if (c == '\\') o += "\\\\"; else if (c != '\r') o += c; }
        o += '\n';
    }
    o += "# END\n";                                                  // lo que quede detras (un fichero anterior mas largo) no se lee
    return o;
}

int NavtexLog::load(const std::string& text, int64_t now) {
    items_.clear();
    (void)now;
    size_t p = 0;
    while (p < text.size()) {
        size_t e = text.find('\n', p);
        if (e == std::string::npos) e = text.size();
        std::string l = text.substr(p, e - p);
        p = e + 1;
        if (!l.empty() && l.back() == '\r') l.pop_back();
        if (l.compare(0, 5, "# END") == 0) break;
        if (l.size() < 4 || l[1] != ';' || l[0] != 'M') continue;        // (las lineas "G" de una version anterior se ignoran)
        std::vector<std::string> f;
        size_t q = 2;
        const size_t want = 8;                                        // campos antes del ultimo (el texto puede llevar ';')
        while (f.size() < want) {
            size_t r = l.find(';', q);
            if (r == std::string::npos) break;
            f.push_back(l.substr(q, r - q));
            q = r + 1;
        }
        if (f.size() != want || !isCode(f[0])) continue;
        const std::string last = l.substr(q);
        if (items_.size() >= kMax) continue;
        NavtexMessage m;
        m.station = f[0][0]; m.subject = f[0][1]; m.number = atoi(f[0].c_str() + 2);
        m.khz = atof(f[1].c_str());
        m.utc = atoll(f[2].c_str()); m.heard = atoll(f[3].c_str());
        m.complete = f[4] == "1";
        m.errorPct = atof(f[5].c_str());
        m.repeats = atoi(f[6].c_str());
        m.kept = f[7] == "1" && keptCount() < kMaxKept;
        for (size_t i = 0; i < last.size(); i++) {
            if (last[i] == '\\' && i + 1 < last.size()) { m.text += last[i + 1] == 'n' ? '\n' : last[i + 1]; i++; }
            else m.text += last[i];
        }
        navWarningPositions(m.text, m.positions);
        items_.push_back(m);
    }
    seq_++;
    return (int)items_.size();
}

// ---- ALR / ACK

namespace {
std::string withSum(const std::string& body) {
    unsigned x = 0;
    for (char c : body) x ^= (unsigned char)c;
    char b[8];
    snprintf(b, sizeof b, "*%02X\r\n", x);
    return "$" + body + b;
}
}  // namespace

namespace {
// Trocea una sentencia del tipo dado ("ALR", "ALF", "NRM"), con o sin "$" y suma (si viene, tiene que cuadrar); talker = sus dos letras
bool fieldsOf(const char* sentence, const char* type, std::string& talker, std::vector<std::string>& f) {
    if (!sentence) return false;
    std::string s(sentence);
    while (!s.empty() && (s.back() == '\r' || s.back() == '\n' || s.back() == ' ')) s.pop_back();
    size_t star = s.rfind('*');
    if (star != std::string::npos && star + 3 == s.size()) {
        unsigned sum = 0;
        size_t from = (s[0] == '$' || s[0] == '!') ? 1 : 0;
        for (size_t i = from; i < star; i++) sum ^= (unsigned char)s[i];
        if (hexv(s[star + 1]) < 0 || hexv(s[star + 2]) < 0 || (unsigned)(hexv(s[star + 1]) * 16 + hexv(s[star + 2])) != sum) return false;
        s.resize(star);
    }
    if (!s.empty() && (s[0] == '$' || s[0] == '!')) s.erase(0, 1);
    if (s.size() < 6 || s[5] != ',' || s.compare(2, 3, type) != 0) return false;
    talker = s.substr(0, 2);
    f.clear();
    size_t p = 6;
    while (true) {
        size_t q = s.find(',', p);
        if (q == std::string::npos) { f.push_back(s.substr(p)); break; }
        f.push_back(s.substr(p, q - p));
        p = q + 1;
    }
    return true;
}
bool hex8(const std::string& t, uint32_t& v) {
    if (t.size() != 8) return false;
    v = 0;
    for (char c : t) { int h = hexv(c); if (h < 0) return false; v = (v << 4) | (uint32_t)h; }
    return true;
}
}  // namespace

bool parseNavtexAlr(const char* sentence, bool& active, bool& acknowledged) {
    std::string talker;
    std::vector<std::string> f;
    if (!fieldsOf(sentence, "ALR", talker, f)) return false;
    if (talker != "CR" || f.size() < 4 || f[1].empty() || atoi(f[1].c_str()) != 3) return false;   // la 003 es de NAVTEX solo si la dice un receptor NAVTEX (CR)
    active = f[2] == "A";
    acknowledged = f[3] == "A";
    return true;
}

bool parseNavtexAlf(const char* sentence, char& state, int& instance) {
    std::string talker;
    std::vector<std::string> f;
    if (!fieldsOf(sentence, "ALF", talker, f)) return false;
    // total, numero, secuencia, hora, categoria, prioridad, estado, fabricante, identificador, instancia, revision, escalado, texto
    if (talker != "CR" || f.size() < 12 || f[1] != "1" || !f[7].empty() || f[8].empty() || atoi(f[8].c_str()) != 3 || f[6].size() != 1) return false;
    if (!strchr("VSANUO", f[6][0])) return false;
    state = f[6][0];
    instance = f[9].empty() ? 1 : atoi(f[9].c_str());
    return true;
}

std::string navtexAcnSentence(int instance) {
    char b[40];
    snprintf(b, sizeof b, "IIACN,,,3,%d,A,C", instance < 1 ? 1 : instance);
    return withSum(b);
}

int navtexFreqIndex(double khz) { return khz > 4000 ? 3 : khz < 500 ? 1 : 2; }

bool parseNavtexNrm(const char* sentence, NavtexMask& out) {
    std::string talker;
    std::vector<std::string> f;
    if (!fieldsOf(sentence, "NRM", talker, f)) return false;
    if (f.size() != 4 || f[0].size() != 1 || f[1].size() != 1 || f[0][0] < '0' || f[0][0] > '3' || f[1][0] < '1' || f[1][0] > '3') return false;
    uint32_t a = 0, t = 0;
    if (!hex8(f[2], a) || !hex8(f[3], t)) return false;
    out.function = f[0][0] - '0';
    out.khz = f[1][0] == '1' ? 490 : f[1][0] == '2' ? 518 : 4209.5;
    out.stations = a & kNavtexAllLetters;                             // los bits 26 a 31 estan reservados
    out.subjects = t & kNavtexAllLetters;
    return true;
}

std::string navtexNrmSentence(const NavtexMask& m) {
    char b[48];
    snprintf(b, sizeof b, "IINRM,%d,%d,%08lX,%08lX", m.function, navtexFreqIndex(m.khz), (unsigned long)(m.stations & kNavtexAllLetters),
             (unsigned long)((m.subjects | kNavtexFixedSubjects) & kNavtexAllLetters));
    return withSum(b);
}

std::string navtexNrmQuery() { return withSum("IICRQ,NRM"); }

std::string navtexAckSentence() { return withSum("IIACK,003"); }

}  // namespace plotter
