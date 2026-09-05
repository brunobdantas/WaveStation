// MSHV RTTY integration - Copyright 2026
// GPL-compatible derived integration for MSHV.
#ifndef MSHV_GEN_RTTY_H
#define MSHV_GEN_RTTY_H

#include <QString>

class GenRtty
{
public:
    GenRtty();
    int genrtty(const QString &text, int *iwave, double sampleRate, double markHz, double shiftHz = 170.0);
};

#endif
