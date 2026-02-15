# NGPC Sound Creator

NGPC Sound Creator is a C++/Qt tool to compose, preview, and export BGM/SFX for Neo Geo Pocket / Color.
It includes a tracker workflow, instrument editor, SFX lab, project mode, and C/ASM export.

<img width="1290" height="1035" alt="ngpc_sound_creator_Tracker" src="https://github.com/user-attachments/assets/fa01fccc-4de3-40dc-858f-4f486cc82d1a" />
                               
<img width="437" height="523" alt="ngpc_sound_creator_start" src="https://github.com/user-attachments/assets/bf12f722-7bb8-46e7-9159-a380677ced2c" />

<img width="1289" height="1036" alt="ngpc_sound_creator_projet" src="https://github.com/user-attachments/assets/81c6fc2b-a8de-4ed8-b6c3-1d8051cbe6a3" />

<img width="1292" height="1035" alt="ngpc_sound_creator_Instruments" src="https://github.com/user-attachments/assets/1fc793ed-1537-43c0-a59b-96b448d1b479" />

## What You Get

- 4-channel tracker workflow (T0/T1/T2/Noise) with keyboard-first editing
- Instrument editor (envelope, ADSR5 AR/DR/SL/SR/RR, vibrato, sweep, pitch curve, LFO1/LFO2 + algo)
- SFX Lab with driver-like tone/noise preview (sweep/env/burst + tone ADSR5 + dual LFO/algo)
- Instrument slot management (tracker code visibility, rename, factory overwrite/reset per slot, full factory-bank reset)
- Project mode (multi-song, autosave, batch export, global instrument bank, project SFX bank)
- Driver-faithful preview for songs and SFX
- Export modes:
  - Pre-baked: maximum playback fidelity, larger data
  - Hybrid: compact streams + driver opcodes/effects (including extended modulation opcodes)
- Project export artifacts: songs, `project_instruments.c`, `project_sfx.c`, manifest, C API
  - `project_sfx.c` now exports base SFX arrays plus tone ADSR/LFO parameter arrays

## Driver Recap (Important)

This tool is designed around the provided NGPC driver pack in `driver_custom_latest/`.

- Goal: stay as close as possible to original SNK-style runtime behavior where relevant
- Scope: adds modern export/runtime features needed by the tool (hybrid opcodes, extended effects)
- Footprint: kept practical for NGPC constraints (no unnecessary heavy runtime systems)
- Result: best tool-to-hardware parity when you integrate this driver pack in the game

If you use another driver, advanced features may differ in sound or behavior.

## Intentionally Out of Scope (V1)

- **VGM import/playback**: not implemented in the tool workflow (focus is tracker + MIDI + project export).
- **DAC/PCM sample playback**: not part of this PSG driver path.
- **Stereo PAN runtime (`0xF5`)**: opcode is reserved but currently consumed as no-op (mono-safe behavior).
- **Generic cross-driver compatibility**: not a primary goal; parity is tuned for the provided driver pack.

## Quick Start

### Build (MinGW / Qt)

```bat
set PATH=C:\Qt\6.10.2\mingw_64\bin;C:\Qt\Tools\CMake_64\bin;C:\Qt\Tools\Ninja;%PATH%
cmake -S . -B build-mingw -G Ninja
cmake --build build-mingw
```

Run:

```bat
.\build-mingw\app\ngpc_sound_creator.exe
```

### Windows Packaging

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\package_windows.ps1 -BuildDir build-mingw -AutoVersion
```

With installer (Inno Setup 6):

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\package_windows.ps1 -BuildDir build-mingw -AutoVersion -CreateInstaller
```

## Documentation

- Full documentation: `DOC.md`
- Driver overview: `driver_custom_latest/README.md`
- Driver integration quickstart: `driver_custom_latest/INTEGRATION_QUICKSTART.md`
- Architecture notes: `ARCHITECTURE.md`

## License Notes

This project is licensed under GNU GPL v3.

Qt 6: GPL/LGPL (project currently uses GPL path)

⚠ Important:
The included NeoPop Z80 core is licensed for non-commercial use only.
If you intend to use this tool in a commercial context,
you must replace this component or obtain proper permission.

