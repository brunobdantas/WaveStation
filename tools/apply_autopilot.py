#!/usr/bin/env python3
from __future__ import annotations
import argparse
from pathlib import Path
import shutil
import subprocess
import sys

UPSTREAM = "50ea1a7b5bf20c688ccc72107e389f06673deac6"

def replace_once(path: Path, old: str, new: str, label: str) -> None:
    text = path.read_text(encoding="utf-8")
    count = text.count(old)
    if count != 1:
        raise RuntimeError(f"{label}: expected exactly one match in {path}, found {count}")
    path.write_text(text.replace(old, new, 1), encoding="utf-8")
    print("patched", label)

def verify(root: Path) -> None:
    marker = root / ".autopilot-upstream-revision"
    if marker.exists():
        actual = marker.read_text(encoding="utf-8").strip()
    elif (root / ".git").exists():
        actual = subprocess.check_output(["git","-C",str(root),"rev-parse","HEAD"],text=True).strip()
    else:
        raise RuntimeError("cannot verify upstream revision")
    if actual != UPSTREAM:
        raise RuntimeError(f"unexpected upstream {actual}; expected {UPSTREAM}")

def apply(root: Path) -> None:
    verify(root)
    repo = Path(__file__).resolve().parents[1]
    src = repo / "src" / "autopilot"
    dst = root / "src" / "autopilot"
    dst.mkdir(parents=True, exist_ok=True)
    for name in ("Ft8CopilotAutoPilot.h","Ft8CopilotAutoPilot.cpp"):
        shutil.copy2(src / name, dst / name)

    pro = root / "MSHV_WIN64.pro"
    main_h = root / "src/main_ms.h"
    main_cpp = root / "src/main_ms.cpp"
    tx_h = root / "src/HvTxW/hvtxw.h"
    tx_cpp = root / "src/HvTxW/hvtxw.cpp"
    log_h = root / "src/HvTxW/HvLogW/hvlogw.h"
    log_cpp = root / "src/HvTxW/HvLogW/hvlogw.cpp"

    replace_once(pro, "CONFIG += release warn_on console exceptions_off\n",
                 "CONFIG += release warn_on console exceptions_off\nTARGET = MSHV-FT8CoPilot\n", "target")
    replace_once(pro, "HEADERS = src/main_ms.h \\\n",
                 "HEADERS = src/autopilot/Ft8CopilotAutoPilot.h \\\n src/main_ms.h \\\n", "header")
    replace_once(pro, "SOURCES = src/main.cpp \\\n",
                 "SOURCES = src/autopilot/Ft8CopilotAutoPilot.cpp \\\n src/main.cpp \\\n", "source")

    replace_once(main_h, '#include "HvAggressiveW/aggressiv_d.h"\n',
                 '#include "HvAggressiveW/aggressiv_d.h"\n#include "autopilot/Ft8CopilotAutoPilot.h"\n',
                 "main include")
    replace_once(main_h, "    void StopTxGlobal();\n",
                 "    void StopTxGlobal();\n    void CopilotHaltTx();\n    void CopilotEnsureAuto();\n",
                 "main slots")
    replace_once(main_h, "    QString App_Path;\n",
                 "    QString App_Path;\n    Ft8CopilotAutoPilot *copilotAutoPilot = nullptr;\n",
                 "main member")

    original = "    connect(TDecodeList1, SIGNAL(EmitRxTextForAutoSeq(QStringList)), THvTxW, SLOT(SetTextForAutoSeq(QStringList)));\n"
    enhanced = """    copilotAutoPilot = new Ft8CopilotAutoPilot(App_Path, this);
    copilotAutoPilot->setStationIdentity(THvTxW->CopilotStationCall(), THvTxW->CopilotStationGrid());
    copilotAutoPilot->setBand(THvTxW->CopilotBand());
    copilotAutoPilot->setModeId(THvTxW->CopilotModeId());

    connect(TDecodeList1, SIGNAL(EmitRxTextForAutoSeq(QStringList)),
            copilotAutoPilot, SLOT(onDecode(QStringList)));
    connect(copilotAutoPilot, SIGNAL(forwardDecode(QStringList)),
            THvTxW, SLOT(SetTextForAutoSeq(QStringList)));
    connect(copilotAutoPilot, SIGNAL(selectTarget(QString,QString,QString,QString,QString)),
            THvTxW, SLOT(DecListTextAll(QString,QString,QString,QString,QString)));
    connect(copilotAutoPilot, SIGNAL(ensureAutoRequested()), this, SLOT(CopilotEnsureAuto()));
    connect(copilotAutoPilot, SIGNAL(haltTxRequested()), this, SLOT(CopilotHaltTx()));
    connect(THvTxW, SIGNAL(EmitCopilotIdentity(QString,QString)),
            copilotAutoPilot, SLOT(setStationIdentity(QString,QString)));
    connect(THvTxW, SIGNAL(EmitCopilotBand(QString)), copilotAutoPilot, SLOT(setBand(QString)));
    connect(THvTxW, SIGNAL(EmitCopilotMode(int)), copilotAutoPilot, SLOT(setModeId(int)));
    connect(THvTxW, SIGNAL(EmitCopilotAdif(QString)), copilotAutoPilot, SLOT(onAdifRecord(QString)));
    connect(THvTxW, SIGNAL(EmitQSOProgress(int)), copilotAutoPilot, SLOT(onQsoProgress(int)));
    connect(TDecodeList1, SIGNAL(ListSelectedTextAll(QString,QString,QString,QString,QString)),
            copilotAutoPilot, SLOT(onManualTarget(QString,QString,QString,QString,QString)));
"""
    replace_once(main_cpp, original, enhanced, "AutoSeq bridge")
    replace_once(main_cpp, "    V_l->addWidget(THvTxW);\n",
                 "    V_l->addWidget(THvTxW);\n    V_l->insertWidget(0, copilotAutoPilot);\n",
                 "autopilot bar")
    replace_once(main_cpp, "void Main_Ms::StopTxGlobal()\n",
                 """void Main_Ms::CopilotHaltTx()
{
    StopTxGlobal();
}
void Main_Ms::CopilotEnsureAuto()
{
    if (!THvTxW->GetAutoIsOn())
        THvTxW->auto_on();
}
void Main_Ms::StopTxGlobal()
""", "control slots")
    replace_once(main_cpp, "    setWindowTitle(str_t);\n",
                 '    setWindowTitle(QString("MSHV + FT8CoPilot AutoPilot 0.1.0 - ")+str_t);\n',
                 "window title")

    replace_once(tx_h, "    void RefreshOtpKeyMsg();//2.76 \n",
                 "    void RefreshOtpKeyMsg();//2.76 \n"
                 "    QString CopilotStationCall() const { return list_macros.value(0); }\n"
                 "    QString CopilotStationGrid() const { return list_macros.value(1); }\n"
                 "    QString CopilotBand() const { return s_band; }\n"
                 "    int CopilotModeId() const { return s_mode; }\n",
                 "state getters")
    replace_once(tx_h, "    void EmitUdpConfigure(int);//2.76.7\n",
                 "    void EmitUdpConfigure(int);//2.76.7\n"
                 "    void EmitCopilotIdentity(QString,QString);\n"
                 "    void EmitCopilotBand(QString);\n"
                 "    void EmitCopilotMode(int);\n"
                 "    void EmitCopilotAdif(QString);\n",
                 "signals")

    replace_once(tx_cpp,
                 "    connect(THvLogW, SIGNAL(EmitAdifRecord(QString)), TRadioAndNetW, SLOT(SendAdifRecord(QString)));\n",
                 "    connect(THvLogW, SIGNAL(EmitAdifRecord(QString)), TRadioAndNetW, SLOT(SendAdifRecord(QString)));\n"
                 "    connect(THvLogW, SIGNAL(EmitCopilotAdif(QString)), this, SIGNAL(EmitCopilotAdif(QString)));\n",
                 "ADIF forwarding")
    replace_once(tx_cpp,
                 "    list_macros = list;\n    s_my_base_call = MultiAnswerMod->FindBaseFullCallRemAllSlash(list_macros.at(0));\n",
                 "    list_macros = list;\n"
                 "    emit EmitCopilotIdentity(list_macros.value(0), list_macros.value(1));\n"
                 "    s_my_base_call = MultiAnswerMod->FindBaseFullCallRemAllSlash(list_macros.at(0));\n",
                 "identity updates")
    replace_once(tx_cpp,
                 "    s_band = s;\n    if (s_mode==11 || s_mode==13 || s_mode==18 || allq65) fpsk_restrict = true;",
                 "    s_band = s;\n    emit EmitCopilotBand(s_band);\n"
                 "    if (s_mode==11 || s_mode==13 || s_mode==18 || allq65) fpsk_restrict = true;",
                 "band updates")
    replace_once(tx_cpp,
                 "void HvTxW::ModeChanget(int mode,bool f)\n{\n",
                 "void HvTxW::ModeChanget(int mode,bool f)\n{\n    emit EmitCopilotMode(mode);\n",
                 "mode updates")

    replace_once(log_h, "   void EmitAdifRecord(QString);\n",
                 "   void EmitAdifRecord(QString);\n   void EmitCopilotAdif(QString);\n",
                 "logger signal")
    normal = "        if (s_udportcp_broad_logged_adif) emit EmitAdifRecord(MakeAdifString(rowt));\n"
    replace_once(log_cpp, normal, normal + "        emit EmitCopilotAdif(MakeAdifString(rowt));\n",
                 "completed QSO ADIF")
    manual = (
        "    if (s_udportcp_broad_logged_adif) emit EmitAdifRecord(MakeAdifString(rowt));\n"
        "\n"
        "    if (s_allowed_modes && (f_isCheck[0] || f_isCheck[3])) GetMarkTextAllFromLog_p(s_band,s_mode);\n"
    )
    replace_once(log_cpp, manual,
                 "    if (s_udportcp_broad_logged_adif) emit EmitAdifRecord(MakeAdifString(rowt));\n"
                 "    emit EmitCopilotAdif(MakeAdifString(rowt));\n\n"
                 "    if (s_allowed_modes && (f_isCheck[0] || f_isCheck[3])) GetMarkTextAllFromLog_p(s_band,s_mode);\n",
                 "manual QSO ADIF")
    print("MSHV + FT8CoPilot AutoPilot 0.1.0 integration applied")

def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("upstream", type=Path)
    args = ap.parse_args()
    try:
        apply(args.upstream.resolve())
    except Exception as exc:
        print("PATCH FAILED:", exc, file=sys.stderr)
        return 2
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
