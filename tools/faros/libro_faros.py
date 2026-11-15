#!/usr/bin/env python3
# Copyright (C) 2026 Manuel - EA7JRS
# SPDX-License-Identifier: GPL-3.0-or-later
"""
Convierte el PDF del Libro de Faros y Senales de Niebla del IHM (partes I y II) en la lista de luces de la microSD del
plotter (una luz por linea: nombre;latitud;longitud;caracteristica;altura m;alcance mn;numero nacional).

Uso personal: los datos son (c) Instituto Hidrografico de la Marina. La lista que sale es para tu tarjeta; no la publiques
ni la subas a ningun repositorio. Este programa solo lee el PDF que tengas tu.

    python3 libro_faros.py parte_I.pdf [parte_II.pdf ...] -o luces.txt [--zona LAT_S LON_W LAT_N LON_E] [--sin-boyas]

Necesita pdftotext (poppler-utils). Lee el texto con su disposicion (-layout): cada luz empieza con su numero nacional
(5 cifras), nombre, latitud, caracteristica, elevacion y alcance; la longitud va en la linea siguiente, y el hemisferio
(N/W, N/E...) en la cabecera de cada pagina. Los nombres se completan con los de su grupo ("-Isla de Tarifa" +
"--Faro" = "Isla de Tarifa. Faro"). Las caracteristicas se pasan a la forma que entiende el plotter: "Fl(3)WR 10s" ->
"Fl(3) W 10s" (el primer color, el del sector principal), "VQ(6)W+LFl 15s" -> "VQ(6)+LFl W 15s", "DirQG" -> "Q G".
Con sectores de distinto alcance (W26 / R18) se queda el mayor. Lo que no se entiende se deja tal cual y se cuenta.
"""
import argparse
import re
import subprocess
import sys

REC = re.compile(r"^(?P<num>\d{5}(?:\.\d+)?)\s+(?P<name>.+?)\s+(?P<latd>\d{1,2}) (?P<latm>\d{1,2},\d)\s+(?P<chr>\S.*?)"
                 r"(?:\s{2,}|\s(?=\d+\s+[WRGY]?\d|\.\.\.\s))(?P<alt>\d+|\.\.\.)\s+(?P<rng>[WRGY]?\d{1,2}|\.\.\.)(?:\s|$)")
LON = re.compile(r"(?P<lond>\d{1,3}) (?P<lonm>\d{1,2},\d)\b")
HEMI = re.compile(r"^\s*([NS])/([EW])\s*$")
HEADER_NUM = re.compile(r"^\s{4,}(-*)(\S.*?)\s*$")
CHR = re.compile(r"^(?:Dir|Aero)?\s*(?P<cls>LFl|VQ|UQ|IUQ|IQ|Iso|Oc|Mo|Al\.?|Fl|Q|F)(?P<grp>\([^)]*\))?"
                 r"(?P<col>(?:Bu|[WRGY])*)(?P<plus>\+LFl)?(?:\s*(?P<per>\d+(?:[,.]\d+)?s))?")
FOG = re.compile(r"^(?:Siren|Horn|Dia|Bell|Whis|Gong|Reed|Explos|Nauto|Tyfon)")
RANGE_TOKEN = re.compile(r"^[WRGY]?(\d{1,2})$")


def pdf_text(path):
    return subprocess.run(["pdftotext", "-layout", path, "-"], check=True, capture_output=True, text=True).stdout.splitlines()


SMALL = {"de", "del", "la", "las", "los", "el", "y", "e", "en", "a"}


def title_es(s):
    """"CABO DE PALOS" -> "Cabo de Palos"."""
    w = s.title().split(" ")
    return " ".join(x.lower() if i and x.lower() in SMALL else x for i, x in enumerate(w))


def tidy_name(s):
    s = re.sub(r"\s+", " ", s).strip().strip(".")
    return s


def normalize_char(raw):
    """Characteristic in the plotter's form, and whether it was understood."""
    t = raw.strip()
    t = re.sub(r"\s*\((?:vert|hor)\)", "", t)          # "3QR(vert)": varias luces en vertical u horizontal
    t = re.sub(r"^\d(?=[A-Z])", "", t)
    t = re.sub(r"^Dir(?=(?:Bu|[WRGY])+\b)", "F", t)     # "DirWRG": luz fija de direccion
    m = CHR.match(t)
    if not m:
        return t, False
    cls = m.group("cls").rstrip(".")
    grp = m.group("grp") or ""
    cols = re.findall(r"Bu|[WRGY]", m.group("col") or "")
    plus = m.group("plus") or ""
    per = (m.group("per") or "").replace(",", ".")
    parts = [cls + grp + plus]
    if cols:
        parts.append(cols[0])
    if per:
        parts.append(per)
    return " ".join(parts), True


def parse(lines, stats):
    hemi = ("N", "W")
    parents = {}          # nivel de guiones -> nombre
    port = ""
    out = []
    i = 0
    n = len(lines)
    while i < n:
        line = lines[i]
        h = HEMI.match(line)
        if h:
            hemi = (h.group(1), h.group(2))
            i += 1
            continue
        m = REC.match(line)
        if not m:
            # cabeceras de grupo: "          -Isla de Tarifa" o un puerto "          TARIFA"
            g = HEADER_NUM.match(line)
            if g and not re.search(r"\d{1,2} \d{1,2},\d", line) and len(g.group(2)) < 45 and not re.match(r"^[\d\[]", g.group(2)) \
                    and ".-" not in g.group(2):                     # la cabecera de pagina ("ESPAÑA.-COSTA SW") no es un grupo
                level = len(g.group(1))
                text = tidy_name(g.group(2))
                if level == 0:
                    if text.isupper() and not re.search(r"[a-z]", text):
                        port = title_es(text)
                        parents = {}
                else:
                    parents[level] = text
                    for k in list(parents):
                        if k > level:
                            del parents[k]
            i += 1
            continue
        stats["records"] += 1
        num = m.group("num")
        raw_name = m.group("name")
        dashes = len(raw_name) - len(raw_name.lstrip("-"))
        name = tidy_name(raw_name.lstrip("-"))
        lat = int(m.group("latd")) + float(m.group("latm").replace(",", ".")) / 60
        chr_text = m.group("chr").strip()
        chr_col = m.start("chr")
        rng_col = m.start("rng")
        alt = m.group("alt")
        ranges = []
        r0 = RANGE_TOKEN.match(m.group("rng"))
        if r0:
            ranges.append(int(r0.group(1)))
        lon = None
        # lineas siguientes del registro: longitud, resto del nombre, periodo, alcances de otros sectores
        j = i + 1
        while j < n and j <= i + 8 and lines[j].strip() and not REC.match(lines[j]) and not HEMI.match(lines[j]):
            ln = lines[j]
            if lon is None:
                lm = LON.search(ln)
                if lm and lm.start() > 10:
                    lon = int(lm.group("lond")) + float(lm.group("lonm").replace(",", ".")) / 60
                    # nombre que sigue en la segunda linea (antes de la longitud, detras del numero internacional)
                    before = ln[:lm.start()]
                    extra = re.sub(r"^\S*\d\S*\s+", "", before.strip()) if re.match(r"^\s*[A-Z]-?\d", before) else before.strip()
                    if extra and not re.match(r"^[A-Z]-?\d", extra):
                        name = tidy_name(name + " " + extra)
                    # periodo partido: "VQ(6)W+LFl" / "15s"
                    after = ln[lm.end():]
                    pm = re.match(r"^\s+(\d+(?:,\d+)?s)\b", after)
                    if pm and not re.search(r"\d+(?:,\d+)?s\b", chr_text):
                        chr_text += " " + pm.group(1)
            for tm in re.finditer(r"\S+", ln):
                if abs(tm.start() - rng_col) <= 3:
                    rm = RANGE_TOKEN.match(tm.group(0))
                    if rm and tm.group(0)[0] in "WRGY":
                        ranges.append(int(rm.group(1)))
            j += 1
        i = j
        if lon is None:
            stats["no_lon"] += 1
            continue
        if hemi[0] == "S":
            lat = -lat
        if hemi[1] == "W":
            lon = -lon
        # el nombre con los de su grupo, del mas cercano al mas lejano, mientras quepa (39 bytes en el plotter)
        chain = [parents[k] for k in sorted(parents) if k < dashes] if dashes else []
        if port and dashes:
            chain = [port] + chain
        full = name
        for parent in reversed(chain):
            longer = parent + ". " + full
            if len(longer.encode("utf-8")) > 39:
                break
            full = longer
        while len(full.encode("utf-8")) > 39:
            full = full.rsplit(" ", 1)[0] if " " in full else full.encode("utf-8")[:39].decode("utf-8", "ignore")
        if chr_text.startswith("...") or FOG.match(chr_text):
            stats["no_light"] += 1                            # sin luz (marca ciega) o solo senal de niebla
            continue
        char_out, ok = normalize_char(chr_text)
        if not ok:
            stats["raw_char"] += 1
        out.append((num, full, lat, lon, char_out, "" if alt == "..." else alt, str(max(ranges)) if ranges else "",
                    "Boya" in name or "boya" in name.lower()))
    return out


def main():
    ap = argparse.ArgumentParser(description="Libro de Faros del IHM (PDF) -> lista de luces del plotter")
    ap.add_argument("pdf", nargs="+")
    ap.add_argument("-o", "--out", required=True)
    ap.add_argument("--zona", nargs=4, type=float, metavar=("LAT_S", "LON_W", "LAT_N", "LON_E"))
    ap.add_argument("--sin-boyas", action="store_true", help="sin boyas ni balizas sin altura")
    a = ap.parse_args()
    stats = {"records": 0, "no_lon": 0, "raw_char": 0, "no_light": 0}
    rows = []
    for p in a.pdf:
        rows += parse(pdf_text(p), stats)
    seen = set()
    kept = []
    for r in rows:
        num, name, lat, lon, ch, alt, rng, buoy = r
        if num in seen:
            continue
        seen.add(num)
        if a.zona and not (a.zona[0] <= lat <= a.zona[2] and a.zona[1] <= lon <= a.zona[3]):
            continue
        if a.sin_boyas and (buoy or not alt):
            continue
        kept.append(r)
    with open(a.out, "w", encoding="utf-8") as f:
        f.write("# Lista de luces generada desde el Libro de Faros del IHM. Uso personal: (c) Instituto Hidrografico de la Marina.\n")
        f.write("# nombre;latitud;longitud;caracteristica;altura m;alcance mn;numero nacional\n")
        for num, name, lat, lon, ch, alt, rng, _ in kept:
            f.write(f"{name.replace(';', ',')};{lat:.5f};{lon:.5f};{ch};{alt};{rng};{num}\n")
    print(f"{stats['records']} registros leidos, {len(kept)} luces escritas en {a.out}; "
          f"{stats['no_light']} sin luz o solo senal de niebla, {stats['no_lon']} sin longitud, "
          f"{stats['raw_char']} caracteristicas sin entender (copiadas tal cual)", file=sys.stderr)


if __name__ == "__main__":
    main()
