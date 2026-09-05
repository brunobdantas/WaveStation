# WaveStation

WaveStation is an experimental Windows HAM-radio digital-mode build that adds a native RTTY modem to a pinned MSHV source revision while keeping the upstream repositories untouched.

## Current RTTY scope

- Native C++11 / Qt implementation
- ITA2 (Baudot) LTRS/FIGS encoding and decoding
- 45.45 baud
- 170 Hz AFSK shift
- 1.5 stop bits
- Mark/space quadrature correlation receiver
- Continuous RX using the application's existing sound input
- TX using the existing sound output and CAT/PTT path
- Optional multi-frequency decoder bank
- Dedicated RTTY console with RX/TX text and basic macros

## Build model

The repository stores only the WaveStation integration layer. CI downloads the pinned upstream MSHV source revision, applies the local integration patch, then compiles on a GitHub-hosted Windows runner using Qt/MinGW.

Pinned upstream revision:

`LZ2HV/MSHV@8f93eb3e25056f0cb18699ef6c3bef3998c52cdf` (MSHV 2.76.7 rc017)

The GitHub Actions artifact is expected to contain a deployable `WaveStation.exe` plus required Qt runtime files.

## Status

Experimental / alpha. RF transmission must first be tested at safe power or into a dummy load.

## Licensing

MSHV is GPL-licensed software. WaveStation modifications and integration code are distributed compatibly under GPLv3. See `THIRD_PARTY_NOTICES.md` and the upstream MSHV license for details.
