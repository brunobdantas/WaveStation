#!/usr/bin/env python3
"""Apply WaveStation RTTY operating presets to an already patched MSHV tree.

This keeps the frequency behaviour inside MSHV's native per-mode/per-band
frequency table, so RTTY behaves like FT8/FT4 when the operator selects a band.
"""
from pathlib import Path
import argparse
import re


def read(path: Path) -> str:
    return path.read_text(encoding="utf-8").replace("\r\n", "\n")


def write(path: Path, text: str) -> None:
    path.write_text(text.replace("\r\n", "\n"), encoding="utf-8", newline="\n")


def replace_one(text: str, old: str, new: str, label: str) -> str:
    count = text.count(old)
    if count != 1:
        raise RuntimeError(f"{label}: expected one anchor, found {count}: {old[:160]}")
    return text.replace(old, new, 1)


def patch_frequency_table(root: Path) -> None:
    path = root / "src" / "config_band_all.h"
    text = read(path)

    text = replace_one(text, "#define COUNT_FREQ_MODES 7", "#define COUNT_FREQ_MODES 8", str(path))
    text = replace_one(
        text,
        "static const uint8_t pos_mod_rea_frq[COUNT_FREQ_MODES]={0,1,6,2,3,4,5};",
        "static const uint8_t pos_mod_rea_frq[COUNT_FREQ_MODES]={0,1,6,2,3,4,5,7};",
        str(path),
    )
    text = replace_one(
        text,
        "static const uint8_t pos_mod_sav_frq[COUNT_FREQ_MODES]={0,1,3,4,5,6,2};",
        "static const uint8_t pos_mod_sav_frq[COUNT_FREQ_MODES]={0,1,3,4,5,6,2,7};",
        str(path),
    )
    text = replace_one(
        text,
        'static const QString ModeStrForFerq[COUNT_FREQ_MODES]={"MSK","FSK","FT4","FT8","JT65","Q65","FT2"};',
        'static const QString ModeStrForFerq[COUNT_FREQ_MODES]={"MSK","FSK","FT4","FT8","JT65","Q65","FT2","RTTY"};',
        str(path),
    )

    # Defaults are dial-frequency starting points, not exclusive channels.
    # HF choices follow common RTTY activity in Region 2 and remain inside
    # IARU-R2 digital/all-mode segments. Bands without a broadly established
    # RTTY watering hole inherit MSHV's existing FSK-family default.
    rtty = {
        3: "1.840.000",   # 160 m - very low RTTY activity; Region-2 DM segment
        4: "3.580.000",   # 80 m
        6: "7.080.000",   # 40 m - common Region-2/Americas RTTY activity
        7: "10.140.000",  # 30 m
        8: "14.080.000",  # 20 m
        9: "18.100.000",  # 17 m
        10: "21.080.000", # 15 m
        11: "24.920.000", # 12 m
        13: "28.080.000", # 10 m
        15: "50.300.000", # 6 m - digital weak-signal area; operator may retune
    }

    marker = "#if defined _ALLBANDSMODSFRQ_H_"
    start = text.find(marker)
    if start < 0:
        raise RuntimeError(f"{path}: active frequency table marker not found")
    end = text.find("#endif", start)
    if end < 0:
        raise RuntimeError(f"{path}: frequency table end not found")

    section = text[start:end]
    rows = 0
    output = []
    for line in section.splitlines(True):
        values = re.findall(r'"([^"]+)"', line)
        if len(values) == 7 and "{" in line and "}" in line:
            default = rtty.get(rows, values[1])
            close = line.rfind("}")
            if close < 0:
                raise RuntimeError(f"{path}: malformed frequency row {rows}")
            line = line[:close] + ',       "' + default + '"' + line[close:]
            rows += 1
        output.append(line)

    if rows != 33:
        raise RuntimeError(f"{path}: expected 33 band rows, patched {rows}")

    text = text[:start] + "".join(output) + text[end:]
    write(path, text)


def patch_frequency_mode_mapping(root: Path) -> None:
    path = root / "src" / "HvTxW" / "HvRadioNetW" / "radionetw.cpp"
    text = read(path)
    fn = "void RadioAndNetW::SetModeForFreqFromMode(int i)"
    start = text.find(fn)
    if start < 0:
        raise RuntimeError(f"{path}: SetModeForFreqFromMode not found")
    end = text.find("\nvoid ", start + len(fn))
    if end < 0:
        end = len(text)
    body = text[start:end]
    anchor = '    QString mode = "UNKNOWN";'
    if body.count(anchor) != 1:
        raise RuntimeError(f"{path}: RTTY frequency-mode insertion anchor not unique")
    body = body.replace(anchor, '    if (i==19) i=7;//WaveStation RTTY frequency profile\n' + anchor, 1)
    text = text[:start] + body + text[end:]
    write(path, text)


def patch_station_call(root: Path) -> None:
    path = root / "src" / "main_ms.cpp"
    text = read(path)
    old = "    TRtty = new HvRttyWidget(this);"
    new = old + "\n    TRtty->SetStationCall(THvTxW->getMy_Call()); // use station identity configured in MSHV"
    text = replace_one(text, old, new, str(path))
    write(path, text)


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--source", required=True, help="already patched WaveStation/MSHV source tree")
    args = parser.parse_args()
    root = Path(args.source).resolve()
    if not (root / "MSHV_WIN64.pro").exists():
        raise SystemExit(f"Not a WaveStation/MSHV source tree: {root}")
    patch_frequency_table(root)
    patch_frequency_mode_mapping(root)
    patch_station_call(root)
    print("RTTY presets applied:", root)


if __name__ == "__main__":
    main()
