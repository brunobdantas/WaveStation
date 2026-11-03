# MSHV Enhanced

**MSHV Enhanced** is a GPL-3.0 derivative of MSHV focused on advanced assisted/automatic FT8-family operation and modern online logbook integrations.

> This is an experimental derivative, not an official MSHV release. RF automation must always be tested at low power or into a dummy load before normal operation.

## Upstream

The build is pinned to the latest MSHV revision selected for this project:

- Project: [LZ2HV/MSHV](https://github.com/LZ2HV/MSHV)
- Version: **MSHV 2.76.8 rc008**
- Commit: `50ea1a7b5bf20c688ccc72107e389f06673deac6`
- Commit date: 2026-10-23

The CI fails closed if that revision cannot be verified.

## Enhanced AutoQSO

The native MSHV decoder and AutoSeq remain responsible for FT8/FT4/FT2 protocol operation. MSHV Enhanced adds a higher-level target-selection and automation controller.

### Strategies

- **HUNT** — automatically choose and call eligible stations.
- **ANSWER** — only handle stations that call your station.
- **BOTH** — hunt when idle, but give directed callers priority.

### Target policies

- Anyone
- Nearby
- Far
- Strongest signal
- Call CQ
- POTA hunter
- SOTA hunter
- POTA + SOTA
- New DXCC
- New grid
- New ITU zone
- New CQ zone
- **I/C/D/G** priority: ITU zone → CQ zone → DXCC → Grid

### Repeat/history policies

- Once
- Always
- Daily
- Weekly
- Monthly
- Per callsign
- Different bands
- Until target is confirmed
- Per band + until target is confirmed

History is read from MSHV's local ADIF log.

### Automation controls

- Explicit **ARM / DISARM / HALT TX**
- Skip Tx1 / send signal report first without GRID
- Retry limit, retry window and cooldown
- QSO progress watchdog
- Target-busy detection
- Automatic abandonment of a target that starts working another station
- Manual target override, then return to automation after completion/timeout
- Optional calls to stations that did not send CQ
- Optional calls to busy stations
- Directed-CQ filtering
- Prefix exclusion list
- POTA/SOTA activity spot enrichment
- CQ fallback when no eligible target exists

The program always starts **disarmed**.

## Online logging

### Club Log

The existing MSHV Club Log real-time integration is retained. Configure it through the normal MSHV Radio/Network settings.

### HRDLog.net

MSHV Enhanced adds real-time HRDLog uploads using:

- station callsign
- HRDLog upload code
- one ADIF record per completed QSO

The account password is not used.

### World Radio League

MSHV Enhanced implements the official WRL v1 API:

- Bearer Developer API key
- one contact per `POST /v1/contacts`
- optional logbook ID; otherwise the WRL default logbook is used
- FT8/FT4/FT2 mode preserved
- signal reports, frequency, band, grid, station callsign and notes
- POTA/SOTA activity references when available
- transient-error retry and `Retry-After` handling

Generate the WRL key in **Integrations → Developer API**.

## Build

Windows builds are reproducible through GitHub Actions using Qt 5.15.2 + MinGW x64.

The CI:

1. checks out the exact MSHV upstream revision;
2. applies `tools/apply_mshv_enhanced.py`;
3. compiles the derived application;
4. packages the required runtime;
5. strips upstream operator/demo state;
6. runs a Windows startup smoke test;
7. emits a portable ZIP, SHA-256 hashes and corresponding GPL source/patch.

## Source layout

```text
src/enhanced/
  MshvEnhancedController.h
  MshvEnhancedController.cpp

tools/
  apply_mshv_enhanced.py

.github/workflows/
  build-windows.yml
```

The upstream MSHV source is not copied into this repository. CI fetches the pinned revision and applies the maintained integration patch.

## Legacy WaveStation

The previous RTTY-oriented WaveStation project is preserved in:

`archive/wavestation-rtty-2026-11-02`

It is not part of the current MSHV Enhanced build.

## License

MSHV Enhanced is distributed under GPL-3.0 as a derivative of MSHV. See `THIRD_PARTY_NOTICES.md` and the corresponding source archive produced by CI.
