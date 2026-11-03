# MSHV Enhanced

MSHV Enhanced is an experimental Windows derivative of the official **MSHV** project, focused on advanced FT8/FT4/FT2 automation and online logbook integration.

## Upstream

Pinned upstream revision:

- MSHV 2.76.8 rc008
- commit `50ea1a7b5bf20c688ccc72107e389f06673deac6`
- official repository: https://github.com/LZ2HV/MSHV

## Enhanced scope

- Native in-process AutoQSO engine (HUNT / ANSWER / BOTH)
- Candidate ranking, history-aware selection, retry/cooldown and watchdog
- Automatic handoff to MSHV's own AutoSeq
- Skip Tx1 using MSHV's native implementation
- Direct-caller priority and busy-target detection
- Native MSHV Club Log and QRZ Logbook support retained
- Added HRDLog real-time upload
- Added World Radio League (WRL) real-time upload
- Retry queue for added online services

The legacy WaveStation/RTTY work is preserved in branch `archive/legacy-wavestation-rtty`.

> Experimental software. Validate CAT/PTT, frequency, audio and power at low power or into a dummy load before enabling automatic transmission.
