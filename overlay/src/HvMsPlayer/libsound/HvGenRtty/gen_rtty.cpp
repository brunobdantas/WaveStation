// MSHV RTTY integration - Copyright 2026
// GPL-compatible derived integration for MSHV.
#include "gen_rtty.h"
#include "../../../HvRtty/rtty_core.h"
#include <vector>

GenRtty::GenRtty() {}

int GenRtty::genrtty(const QString &text, int *iwave, double sampleRate, double markHz, double shiftHz)
{
    if (!iwave) return 0;
    mshv_rtty::Config c;
    c.sampleRate = sampleRate;
    c.baud = 45.45;
    c.markHz = markHz;
    c.spaceHz = markHz + shiftHz;
    c.stopBits = 1.5;
    mshv_rtty::Encoder e(c);
    std::vector<int> w = e.generate(text.toUpper().toStdString(),0.72,0.30,0.20);
    const int maxSamples = 2976000;
    int n = (int)w.size();
    if (n > maxSamples) n = maxSamples;
    for (int i=0;i<n;++i) iwave[i]=w[i];
    return n;
}
