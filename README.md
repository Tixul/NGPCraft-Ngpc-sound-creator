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

## Download

Ready-to-run builds are attached to each [release](https://github.com/Tixul/NGPCraft-Ngpc-sound-creator/releases/latest). No Qt installation is needed: everything is bundled.

| System | File | Notes |
| --- | --- | --- |
| Windows 10/11 (x64) | `ngpc_sound_creator_setup_<version>.exe` | Installer |
| | `ngpc_sound_creator-windows-<version>.zip` | Portable: unzip anywhere and run `ngpc_sound_creator.exe` |
| Linux (x86_64) | `ngpc_sound_creator-linux-<version>-x86_64.AppImage` | Single file, runs on most distributions (glibc 2.35+, e.g. Ubuntu 22.04+, Debian 12+, Fedora 36+) |
| macOS 12+ | `ngpc_sound_creator-macos-<version>.dmg` | Universal: Apple Silicon and Intel |

### Linux

```sh
chmod +x ngpc_sound_creator-linux-*.AppImage
./ngpc_sound_creator-linux-*.AppImage
```

If it does not start and mentions FUSE, install `libfuse2` (`libfuse2t64` on Ubuntu 24.04+), or run it with `--appimage-extract-and-run`.

### macOS

Open the DMG and drag **NGPC Sound Creator** onto the **Applications** shortcut.

The app is not signed with an Apple developer certificate, so macOS blocks the first launch:

- macOS 14 and earlier: right-click the app, choose **Open**, then **Open** again.
- macOS 15 and later: try to open it once, then go to **System Settings > Privacy & Security** and click **Open Anyway**.
- Or, in a terminal: `xattr -dr com.apple.quarantine "/Applications/NGPC Sound Creator.app"`

### Driver pack location

Every package ships the driver pack (`driver_custom_latest`), used by the tool's driver export:

- Windows: next to `ngpc_sound_creator.exe`
- Linux: inside the AppImage (`usr/share/ngpc_sound_creator/`)
- macOS: inside the app bundle (`Contents/Resources/`)

## Build From Source

Requirements: CMake 3.20+, a C++17 compiler and **Qt 6** with the **Qt Multimedia** module. Release builds use Qt 6.10.2.

### Windows (MinGW / Qt)

```bat
set PATH=C:\Qt\6.10.2\mingw_64\bin;C:\Qt\Tools\mingw1310_64\bin;C:\Qt\Tools\CMake_64\bin;C:\Qt\Tools\Ninja;%PATH%
cmake -S . -B build-mingw -G Ninja
cmake --build build-mingw
```

Run:

```bat
.\build-mingw\app\ngpc_sound_creator.exe
```

### Linux

Debian / Ubuntu:

```sh
sudo apt install build-essential cmake ninja-build qt6-base-dev qt6-multimedia-dev
```

Fedora: `sudo dnf install gcc-c++ cmake ninja-build qt6-qtbase-devel qt6-qtmultimedia-devel`
Arch: `sudo pacman -S base-devel cmake ninja qt6-base qt6-multimedia`

Then:

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/app/ngpc_sound_creator
```

### macOS

With [Homebrew](https://brew.sh):

```sh
brew install cmake ninja qt
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="$(brew --prefix qt)"
cmake --build build
./build/app/ngpc_sound_creator.app/Contents/MacOS/ngpc_sound_creator
```

Launch it from the repository root as above so the tool finds `driver_custom_latest`
(on Windows and Linux, the build folder is enough).

### Windows Packaging

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\package_windows.ps1 -BuildDir build-mingw -AutoVersion
```

With installer (Inno Setup 6):

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\package_windows.ps1 -BuildDir build-mingw -AutoVersion -CreateInstaller
```

### Release Builds (GitHub Actions)

`.github/workflows/release.yml` builds the Windows, Linux and macOS packages on GitHub.
Publishing a release with a `vX.Y.Z` version builds all three and attaches them to the release.
**Actions > Release builds > Run workflow** builds them as downloadable artifacts only, without a release.

## Documentation

- Full documentation: `DOC.md`
- Driver overview: `driver_custom_latest/README.md`
- Driver integration quickstart: `driver_custom_latest/INTEGRATION_QUICKSTART.md`
- Architecture notes: `ARCHITECTURE.md`

## License

This project is licensed under the **MIT License** — Copyright (c) 2026 Willy (Tixul).
See `LICENSE`.

Both emulation cores — the Z80 CPU and the T6W28 PSG — are first-party: clean-room
code, vendored from the NGPCraft emulator. There is no third-party emulation code in
this tool.

The distributed builds (Windows, Linux, macOS) bundle **Qt 6** (used under LGPL v3,
dynamically linked) and, through Qt Multimedia, the **FFmpeg** libraries. Those keep their own licenses;
see `THIRD_PARTY.md`. None of that constrains reuse of this project's own MIT source.

Music, sound effects and data created with this tool are yours — not a derivative of
the software, and free to use commercially.

