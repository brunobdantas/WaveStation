#!/usr/bin/env python3
from pathlib import Path
import argparse, shutil, re, sys

MODE_ID = 19


def read(p: Path) -> str:
    return p.read_text(encoding='utf-8').replace('\r\n','\n')

def write(p: Path, s: str):
    p.write_text(s.replace('\r\n','\n'), encoding='utf-8', newline='\n')

def one(p: Path, old: str, new: str):
    s=read(p)
    n=s.count(old)
    if n != 1:
        raise RuntimeError(f'{p}: expected one anchor, found {n}:\n{old[:240]}')
    write(p,s.replace(old,new,1))

def allrep(p: Path, old: str, new: str, min_count=1):
    s=read(p); n=s.count(old)
    if n < min_count: raise RuntimeError(f'{p}: anchor not found: {old[:200]}')
    write(p,s.replace(old,new))

def copy_overlay(root: Path, out: Path):
    src = root/'overlay'/'src'/'HvRtty'
    shutil.copytree(src, out/'src'/'HvRtty', dirs_exist_ok=True)
    src = root/'overlay'/'src'/'HvMsPlayer'/'libsound'/'HvGenRtty'
    shutil.copytree(src, out/'src'/'HvMsPlayer'/'libsound'/'HvGenRtty', dirs_exist_ok=True)

def patch(out: Path, root: Path):
    p=out/'src/config.h'
    one(p,'#define APP_NAME \"MSHV version \" VER_MS \" 64-bit\"// r002 For Test',
          '#define APP_NAME \"WaveStation alpha \" VER_MS \" 64-bit\"// WaveStation experimental build')

    p=out/'src/config_str_all.h'
    one(p,'#define COUNT_MODE 19','#define COUNT_MODE 20')
    one(p,'0,12,1,2,3,4,5,6,18,13,11,14,15,16,17,7,8,9,10','0,12,1,2,3,4,5,6,18,13,11,14,15,16,17,7,8,9,10,19')
    one(p,'"Q65A","Q65B","Q65C","Q65D","FT2"','"Q65A","Q65B","Q65C","Q65D","FT2","RTTY"')
    one(p,'            "QLabel{background-color:rgb(92,235,220);"};  //Color FT2',
          '            "QLabel{background-color:rgb(92,235,220);",  //Color FT2\n            "QLabel{background-color:rgb(255,205,120);"}; //Color RTTY')

    p=out/'src/HvMsCore/mscore.h'
    one(p,'signals:\n','signals:\n    void EmitRttyAudio(int*,int); // MSHV-RTTY native continuous audio\n')
    p=out/'src/HvMsCore/mscore.cpp'
    one(p,'    emit Set_Raw(dat,count,ffopen); //1.27 psk rep fopen bool true false no file open',
          '    emit Set_Raw(dat,count,ffopen); //1.27 psk rep fopen bool true false no file open\n    if (s_mod_iden==19) emit EmitRttyAudio(dat,count); // MSHV-RTTY')

    p=out/'src/HvDecoderMs/decoderms.cpp'
    needle='void DecoderMs::SetDecode(int *raw,int count_q,QString time,int t_istart,int mousebutton,bool f_rtd,bool end_rtd,bool ffopen)//1.27 psk rep   fopen bool true    false no file open\n{'
    one(p,needle,needle+'\n    if (s_mode==19) return; // native RTTY is continuous, not period-decoded')

    p=out/'src/HvMsPlayer/libsound/mpegsound.h'
    one(p,'#include "HvGenQ65/gen_q65.h"', '#include "HvGenQ65/gen_q65.h"\n#include "HvGenRtty/gen_rtty.h" // MSHV-RTTY')
    one(p,'    GenQ65 *TGenQ65;', '    GenQ65 *TGenQ65;\n    GenRtty *TGenRtty; // MSHV-RTTY')

    p=out/'src/HvMsPlayer/libsound/genmesage.cpp'
    one(p,'    TGenQ65=NULL;', '    TGenQ65=NULL;\n    TGenRtty=NULL; // MSHV-RTTY')
    one(p,'    else if ((s_mode == 14 || s_mode == 15 || s_mode == 16|| s_mode == 17) && f_gens) delete TGenQ65;//q65',
          '    else if ((s_mode == 14 || s_mode == 15 || s_mode == 16|| s_mode == 17) && f_gens) delete TGenQ65;//q65\n    else if (s_mode == 19 && f_gens) delete TGenRtty; // MSHV-RTTY')
    one(p,'        else if (s_mode == 14 || s_mode == 15 || s_mode == 16|| s_mode == 17) TGenQ65 = new GenQ65(false);//q65',
          '        else if (s_mode == 14 || s_mode == 15 || s_mode == 16|| s_mode == 17) TGenQ65 = new GenQ65(false);//q65\n        else if (s_mode == 19) TGenRtty = new GenRtty(); // MSHV-RTTY')
    allrep(p,'        if (iwave_count>=iwave_size) iwave_count = 0;',
           '        if (iwave_count>=iwave_size) { if (s_mode==19) return false; iwave_count = 0; } // MSHV-RTTY one-shot',2)

    p=out/'src/HvMsPlayer/libsound/genmesage_ms.cpp'
    anchor='    else if (mod_ident==14 || mod_ident==15  || mod_ident==16 || mod_ident==17)'
    rtty='''    else if (mod_ident==19)\n    {//RTTY 45.45 baud / 170 Hz AFSK\n        QString tstr = format_msg(msg,max_len);\n        nwave = TGenRtty->genrtty(tstr,iwave,GEN_SAMPLE_RATE,tx_freq,170.0);\n        goto end;\n    }\n'''
    one(p,anchor,rtty+anchor)

    p=out/'src/HvMsPlayer/msplayerhv.h'
    one(p,'    void SendTxMsgAllTxt(QString,double);','    void SendTxMsgAllTxt(QString,double);\n    void RttyTxFinished(); // MSHV-RTTY')
    p=out/'src/HvMsPlayer/msplayerhv.cpp'
    needle='''        if (!server->run())\n        {\n            music_done();\n            break;\n        }'''
    repl='''        if (!server->run())\n        {\n            bool rtty_done = (s_mode==19 && !music.RealStop);\n            music_done();\n            if (rtty_done) emit RttyTxFinished();\n            break;\n        }'''
    one(p,needle,repl)

    p=out/'src/HvTxW/hvspinbox.cpp'
    one(p,'    s_dftolerance[18] = 1500;//ft2 fictive','    s_dftolerance[18] = 1500;//ft2 fictive\n    s_dftolerance[19] = 400;//rtty')
    one(p,'    s_pt_all_modes[18] = 3;//ft4 3.75s','    s_pt_all_modes[18] = 3;//ft4 3.75s\n    s_pt_all_modes[19] = 30;//rtty continuous/manual')

    p=out/'src/main_ms.h'
    one(p,'#include "HvTxW/hvspinbox.h"','#include "HvTxW/hvspinbox.h"\n#include "HvRtty/hvrttywidget.h" // MSHV-RTTY\n#include <QByteArray>')
    one(p,'    void SetOffsetDt(int);//2.76.5','    void SetOffsetDt(int);//2.76.5\n    void RttySend(QString);\n    void RttyAbort();\n    void RttyTxFinished();')
    one(p,'    MsPlayerHV *TMsPlayerHV;','    MsPlayerHV *TMsPlayerHV;\n    HvRttyWidget *TRtty;\n    QByteArray rtty_tx_buffer_;')

    p=out/'src/main_ms.cpp'
    a='    connect(TMsPlayerHV, SIGNAL(SendTxMsgAllTxt(QString,double)), this, SLOT(SetTxMsgAllTxt(QString,double)));'
    b=a+'''\n\n    // MSHV-RTTY native window: uses the already selected MSHV sound device and rig/PTT.\n    TRtty = new HvRttyWidget(this);\n    connect(TMsCore,SIGNAL(EmitRttyAudio(int*,int)),TRtty,SLOT(FeedAudio(int*,int)),Qt::DirectConnection);\n    connect(TRtty,SIGNAL(SendRequested(QString)),this,SLOT(RttySend(QString)));\n    connect(TRtty,SIGNAL(AbortRequested()),this,SLOT(RttyAbort()));\n    connect(TMsPlayerHV,SIGNAL(RttyTxFinished()),this,SLOT(RttyTxFinished()));\n    connect(MainDisplay,SIGNAL(EmitVDTxFreq(double)),TRtty,SLOT(SetMarkFrequency(double)));'''
    one(p,a,b)
    a='    connect(W_mod_bt_sw,SIGNAL(clicked(int)),this,SLOT(ModBtSwClicked(int)));//2.74'
    one(p,a,'    connect(rb_mode[19], SIGNAL(toggled(bool)), TRtty, SLOT(SetActive(bool))); // MSHV-RTTY\n'+a)
    anchor='void Main_Ms::SetOffsetDt(int dt)//2.76.5'
    slots='''void Main_Ms::RttySend(QString text)\n{\n    if (s_mode!=19 || text.trimmed().isEmpty()) return;\n    TMsPlayerHV->Stop();\n    StopRx();\n    SetRigTxRx(true);\n    rtty_tx_buffer_ = text.toUpper().toUtf8();\n    TMsPlayerHV->setfile_play(rtty_tx_buffer_.data(),true,19,30);\n}\nvoid Main_Ms::RttyAbort()\n{\n    if (s_mode!=19) return;\n    TMsPlayerHV->Stop();\n    SetRigTxRx(false);\n    if (global_start_moni) StartRx();\n    if (TRtty) TRtty->SetTxFinished();\n}\nvoid Main_Ms::RttyTxFinished()\n{\n    if (s_mode!=19) return;\n    SetRigTxRx(false);\n    if (global_start_moni) StartRx();\n    if (TRtty) TRtty->SetTxFinished();\n}\n'''
    one(p,anchor,slots+'\n'+anchor)
    one(p,'if (s_mode==11 || s_mode==13 || s_mode==18 || allq65) txa = f_tx_busy;',
          'if (s_mode==11 || s_mode==13 || s_mode==18 || s_mode==19 || allq65) txa = f_tx_busy;')
    one(p,'if (s_mode==11 || s_mode==13 || s_mode==18 || allq65) ModeMenuStatRefresh(f_de_active);//ft8 and ft4',
          'if (s_mode==11 || s_mode==13 || s_mode==18 || s_mode==19 || allq65) ModeMenuStatRefresh(f_de_active);//ft8 ft4 rtty')

    # RTTY must actually set s_mode=19 in MSHV's explicit per-mode switch.
    anchor='    if (rb_mode[18]->isChecked())\n    {   // cps.'
    # Insert before FT2; independent of its branch.
    rtty_mode='''    if (rb_mode[19]->isChecked())\n    {\n        s_mode = 19;\n        MainDisplay->setArrayInPxel(50);\n        SecondDisplay->setArrayInPxel(50);\n        Direct_log_qso->setEnabled(true);\n        Prompt_log_qso->setEnabled(true);\n        ac_show_timec->setEnabled(true);\n        ac_show_freqc->setEnabled(true);\n        THvTxW->StopAuto(); // RTTY is asynchronous/manual in this build\n    }\n'''
    one(p,anchor,rtty_mode+anchor)

    # Do not invoke period decoding for asynchronous RTTY.
    anchor='    if (s_mode==10)//pi4 rx\n'
    one(p,anchor,'    if (s_mode==19)\n    {\n        // RTTY decoder consumes the live 12 kHz stream continuously.\n    }\n    else '+anchor.lstrip())

    # Rig-control frequency-offset behavior should match other soundcard digital modes.
    p=out/'src/HvRigControl/hvrigcontrol.cpp'
    one(p,'if (s_mode==11 || s_mode==13 || s_mode==18 || s_mode==14 || s_mode==15 || s_mode==16 || s_mode==17) all_static_tx_modes = true;',
          'if (s_mode==11 || s_mode==13 || s_mode==18 || s_mode==19 || s_mode==14 || s_mode==15 || s_mode==16 || s_mode==17) all_static_tx_modes = true;')

    # Treat RTTY like other 12 kHz/vertical waterfall modes.
    p=out/'src/HvMsCore/mscore.cpp'
    a='s_mod_iden == 14 || s_mod_iden == 15 || s_mod_iden == 16 || s_mod_iden == 17) f_disp_v_h = true;'
    one(p,a,'s_mod_iden == 14 || s_mod_iden == 15 || s_mod_iden == 16 || s_mod_iden == 17 || s_mod_iden == 19) f_disp_v_h = true;')

    # Decode list: a simple RTTY text row schema; no FT protocol parsing.
    p=out/'src/HvDecodeList/decodelist.cpp'
    anchor='    if (mode==0 || mode==12)//msk144\n'
    rtty='''    if (mode==19)//RTTY\n    {\n        list_A << tr("Time") << "Mark" << "Baud" << "Shift" << tr("Message");\n        model.setHorizontalHeaderLabels(list_A);\n        for (int c=5;c<13;c++) THvHeader->hideSection(c);\n        def_section_sizes[0]=70; def_section_sizes[1]=60; def_section_sizes[2]=60; def_section_sizes[3]=60; def_section_sizes[4]=250;\n        THvHeader->setSectionResizeMode(4,QHeaderView::Stretch);\n        msg_column=4;\n    }\n'''
    one(p,anchor,rtty+anchor)

    # qmake files: compile the new sources on every supported target.
    pros=list(out.glob('MSHV_*.pro'))
    if not pros: raise RuntimeError('No MSHV_*.pro files found')
    for q in pros:
        s=read(q)
        if 'src/HvRtty/rtty_core.h' not in s:
            a=' src/HvMsPlayer/libsound/HvGenQ65/q65_subs.h \\\n'
            b=a+' src/HvRtty/rtty_core.h \\\n src/HvRtty/hvrttywidget.h \\\n src/HvMsPlayer/libsound/HvGenRtty/gen_rtty.h \\\n'
            if a not in s: raise RuntimeError(f'{q}: qmake header anchor missing')
            s=s.replace(a,b,1)
        if 'src/HvRtty/rtty_core.cpp' not in s:
            a=' src/HvMsPlayer/libsound/HvGenQ65/q65_subs.cpp \\\n'
            b=a+' src/HvRtty/rtty_core.cpp \\\n src/HvRtty/hvrttywidget.cpp \\\n src/HvMsPlayer/libsound/HvGenRtty/gen_rtty.cpp \\\n'
            if a not in s: raise RuntimeError(f'{q}: qmake source anchor missing')
            s=s.replace(a,b,1)
        write(q,s)

    (out/'MSHV_RTTY_BUILD.txt').write_text(
        'WaveStation experimental native RTTY build\n'
        'Base: LZ2HV/MSHV pinned local build tree\nMode ID: 19\n'
        'RTTY: 45.45 baud, ITA2/Baudot, 170 Hz AFSK, 1.5 stop bits\n'
        'No remote repository is modified by this package.\n',encoding='utf-8')


def main():
    ap=argparse.ArgumentParser()
    ap.add_argument('--source',required=True)
    ap.add_argument('--output',required=True)
    ap.add_argument('--package-root',default=str(Path(__file__).resolve().parents[1]))
    args=ap.parse_args()
    source=Path(args.source).resolve(); out=Path(args.output).resolve(); root=Path(args.package_root).resolve()
    if not (source/'MSHV_WIN64.pro').exists(): raise SystemExit(f'Not an MSHV source tree: {source}')
    if out.exists(): shutil.rmtree(out)
    shutil.copytree(source,out)
    copy_overlay(root,out)
    patch(out,root)
    print(out)

if __name__=='__main__': main()
