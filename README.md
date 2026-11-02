# MSHV Enhanced

MSHV Enhanced is a GPL-3.0 derivative build of MSHV focused on advanced assisted/automatic FT8-family operation and modern logbook integrations.

## Upstream

- Project: LZ2HV/MSHV
- Pinned revision: `50ea1a7b5bf20c688ccc72107e389f06673deac6`
- Upstream version: **MSHV 2.76.8 rc008**
- Upstream date: 2026-10-23

## Goals

- Advanced AutoQSO for FT8/FT4/FT2 with operator-controlled ARM/DISARM/HALT.
- Native MSHV AutoSeq remains responsible for the exchange itself.
- Target policies by callsign, DXCC, grid, CQ zone and ITU zone.
- Skip Tx1 support and configurable retry/cooldown/watchdog behavior.
- History-aware scoring by call, entity, band and mode.
- Directed-call priority and busy-target detection.
- Transparent candidate scoring and decision audit trail.
- Preserve/extend MSHV's Club Log support and add HRDLog.net and World Radio League.
- Reproducible Windows x64 CI build with Qt/MinGW.

## Safety model

Automation is always explicitly armed by the operator. Loss of required station state must fail closed and stop automatic TX.

## Legacy WaveStation

The previous RTTY-oriented project is preserved in branch:

`archive/wavestation-rtty-2026-11-02`

## License

Derived from MSHV and distributed under GPL-3.0. See `THIRD_PARTY_NOTICES.md`.
