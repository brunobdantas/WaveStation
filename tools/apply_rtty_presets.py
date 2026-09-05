#!/usr/bin/env python3
"""Apply WaveStation RTTY presets without changing MSHV's persisted frequency schema.

The upstream MSHV frequency-settings format has seven mode slots. Expanding it
breaks compatibility with existing/bundled settings during startup, so RTTY
uses the existing FSK slot only as a safe internal fallback and applies its own
frequency preset explicitly when the operator activates RTTY or selects a band.
"""
from pathlib import Path
import argparse


def read(path: Path) -> str:
    return path.read_text(encoding="utf-8").replace("\r\n", "\n")


def write(path: Path, text: str) -> None:
    path.write_text(text.replace("\r\n", "\n"), encoding="utf-8", newline="\n")


def replace_one(text: str, old: str, new: str, label: str) -> str:
    count = text.count(old)
    if count != 1:
        raise RuntimeError(f"{label}: expected one anchor, found {count}: {old[:180]}")
    return text.replace(old, new, 1)


def patch_frequency_mode_fallback(root: Path) -> None:
    """Keep COUNT_FREQ_MODES=7 and map RTTY to FSK only as internal fallback."""
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
        raise RuntimeError(f"{path}: RTTY frequency fallback anchor not unique")
    body = body.replace(
        anchor,
        '    if (i==19) i=1;//WaveStation RTTY: safe fallback to existing FSK settings slot\n' + anchor,
        1,
    )
    text = text[:start] + body + text[end:]
    write(path, text)


def patch_hvtxw_public_rtty_frequency(root: Path) -> None:
    """Expose a narrow public wrapper around MSHV's existing private frequency path."""
    path = root / "src" / "HvTxW" / "hvtxw.h"
    text = read(path)
    anchor = "    void SetBand(QString,int id);//0<-from App 1<-from Rig"
    repl = anchor + "\n    void SetRttyPresetFreq(QString f) { SetDefFreqGlobal(3,f); } // WaveStation: force freq, preserve rig mode"
    text = replace_one(text, anchor, repl, str(path))
    write(path, text)


def patch_rtty_runtime_presets(root: Path) -> None:
    """Apply RTTY dial presets from Main_Ms without altering saved settings format."""
    h = root / "src" / "main_ms.h"
    text = read(h)
    text = replace_one(
        text,
        "    void RttyTxFinished();",
        "    void RttyTxFinished();\n    void RttyModeActive(bool);",
        str(h),
    )
    write(h, text)

    path = root / "src" / "main_ms.cpp"
    text = read(path)

    # Forward declaration is required because BandChanged() appears before the
    # RTTY slot implementations in upstream main_ms.cpp.
    text = replace_one(
        text,
        '#include "main_ms.h"',
        '#include "main_ms.h"\n\nstatic QString WaveStationRttyPresetHz(int bandIndex);',
        str(path),
    )

    # Do not read getMy_Call() during application construction. MSHV populates
    # its macro/settings data later in startup. Resolve it only when RTTY is
    # actually activated by the operator.
    text = replace_one(
        text,
        "    connect(rb_mode[19], SIGNAL(toggled(bool)), TRtty, SLOT(SetActive(bool))); // MSHV-RTTY",
        "    connect(rb_mode[19], SIGNAL(toggled(bool)), this, SLOT(RttyModeActive(bool))); // WaveStation RTTY lazy activation",
        str(path),
    )

    anchor = "void Main_Ms::RttySend(QString text)"
    helper = r'''static QString WaveStationRttyPresetHz(int bandIndex)
{
    // Dial-frequency starting points for common RTTY activity. These are
    // presets, not exclusive channels; the operator may retune at any time.
    switch (bandIndex)
    {
        case 3:  return "1840000";   // 160 m
        case 4:  return "3580000";   // 80 m
        case 6:  return "7080000";   // 40 m
        case 7:  return "10140000";  // 30 m
        case 8:  return "14080000";  // 20 m
        case 9:  return "18100000";  // 17 m
        case 10: return "21080000";  // 15 m
        case 11: return "24920000";  // 12 m
        case 13: return "28080000";  // 10 m
        case 15: return "50300000";  // 6 m
        default: return QString();
    }
}
void Main_Ms::RttyModeActive(bool on)
{
    if (!TRtty) return;
    if (on)
    {
        // Resolve the station call only after normal MSHV settings/macros have
        // finished loading. This avoids startup access to an unpopulated list.
        QString call = THvTxW->getMy_Call().trimmed();
        if (!call.isEmpty()) TRtty->SetStationCall(call);

        // If RTTY is selected after the band, immediately tune that band's
        // RTTY preset through a narrow wrapper over MSHV's normal frequency/CAT path.
        for (int i=0; i<COUNT_BANDS; ++i)
        {
            if (ListBands.at(i)->isChecked())
            {
                QString f = WaveStationRttyPresetHz(i);
                if (!f.isEmpty()) THvTxW->SetRttyPresetFreq(f);
                break;
            }
        }
    }
    TRtty->SetActive(on);
}
'''
    text = replace_one(text, anchor, helper + "\n" + anchor, str(path))

    band_anchor = "            THvTxW->SetBand(temp_band,s_id_set_to_rig);//0<-from App 1<-from Rig\n            RefreshWindowTitle();"
    band_repl = r'''            THvTxW->SetBand(temp_band,s_id_set_to_rig);//0<-from App 1<-from Rig
            // RTTY has its own compatibility-safe band presets. Do not add an
            // eighth entry to MSHV's persisted frequency table: older settings
            // contain seven entries and must remain readable at startup.
            if (s_mode==19 && s_id_set_to_rig==0)
            {
                QString rtty_f = WaveStationRttyPresetHz(i);
                if (!rtty_f.isEmpty()) THvTxW->SetRttyPresetFreq(rtty_f);
            }
            RefreshWindowTitle();'''
    text = replace_one(text, band_anchor, band_repl, str(path))
    write(path, text)


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--source", required=True, help="already patched WaveStation/MSHV source tree")
    args = parser.parse_args()
    root = Path(args.source).resolve()
    if not (root / "MSHV_WIN64.pro").exists():
        raise SystemExit(f"Not a WaveStation/MSHV source tree: {root}")

    # Deliberately DO NOT modify config_band_all.h / COUNT_FREQ_MODES.
    patch_frequency_mode_fallback(root)
    patch_hvtxw_public_rtty_frequency(root)
    patch_rtty_runtime_presets(root)
    print("RTTY compatibility-safe presets applied:", root)


if __name__ == "__main__":
    main()
