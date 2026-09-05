# Third-party notices

## MSHV

Base application: MSHV by Hrisimir Hristov, LZ2HV.  
Source: https://github.com/LZ2HV/MSHV  
License in upstream repository: GNU General Public License (GPL).

WaveStation does not modify or publish to the upstream MSHV repository. CI downloads a pinned source archive and creates a derived build tree inside the GitHub Actions runner.

## Diddle

Reference project: Diddle by WW2DX contributors.  
Source: https://github.com/WW2DX/diddle  
License: MIT.

WaveStation does not embed the Diddle Tauri/Rust application. RTTY DSP concepts such as ITA2/AFSK, mark-space correlation, asynchronous framing, and multi-decoder workflow were reimplemented in C++11 for integration with the existing Qt/audio/rig infrastructure.
