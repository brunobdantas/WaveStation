# MSHV + FT8CoPilot AutoPilot 0.1.0

A deliberately small derivative of the official **MSHV** project with one added feature:
an in-process AutoPilot based on the public FT8CoPilot selection/automation behavior.

## Baselines

- MSHV: 2.76.8 rc008, commit `50ea1a7b5bf20c688ccc72107e389f06673deac6`
- FT8CoPilot behavioral reference: commit `a1adf77ac6a691914d250f607e3cb8f3223c9e6c`

## 0.1.0 scope

The original MSHV decoder, CAT/PTT, message generator, AutoSeq and logger remain in control.
The AutoPilot only listens to decoded candidates, filters/ranks them and invokes the native
MSHV QSO path.

Supported selector chain: Any, CallSign, Grid, Continent, Country, CQZone, ITUZone,
DXCC100 and Extra.

AutoPilot always starts PAUSED. This release intentionally excludes QRZ enrichment,
online log integrations, POTA/SOTA APIs and Skip Tx1.
