# NGPC Sound Creator - Architecture

This document describes the initial C++/Qt layout for the MVP tool.

-------------------------------------------------------------------------------
GOALS
-------------------------------------------------------------------------------
- C++ codebase that can evolve without UI rewrites.
- Separate core logic (audio + format) from UI.
- Multiple tabs: player/convert, editor, palette, debug.

-------------------------------------------------------------------------------
STRUCTURE
-------------------------------------------------------------------------------
NGPC_SOUND_CREATOR/
- CMakeLists.txt
- core/
  - include/ngpc/
  - src/
- app/
  - CMakeLists.txt
  - src/
    - MainWindow.*
    - tabs/

-------------------------------------------------------------------------------
UI TABS (MVP PLACEHOLDERS)
-------------------------------------------------------------------------------
- Player / Convert
- Track Editor
- Sound Palette
- Debug

Audio preview
- QtMultimedia output (mono), driven by PSG + Z80 in the audio callback.

-------------------------------------------------------------------------------
NEXT CORE MODULES
-------------------------------------------------------------------------------
- psg/     : T6W28 emulator (NGPCraft clean-room, first-party; ONE chip, two
             asymmetric ports -- 0x4001 = LEFT/tone periods, 0x4000 = RIGHT/noise
             control -- see core/third_party/ngpc_apu/apu_core.hpp)
- z80/     : Z80 core for driver execution (NGPCraft clean-room, first-party;
             vendored from the NGPCraft emulator as a header-only core templated
             on a Bus -- see core/third_party/ngpc_z80/z80_core.hpp)
- midi/    : parser + K1Sound rules
- format/  : K1Sound encoder (group/list + BGM/SE)
- project/ : project model (JSON)
- polling_driver/ : built-in Z80 polling driver + host buffer (quick tests)

-------------------------------------------------------------------------------
BUILD
-------------------------------------------------------------------------------
- Qt6 + CMake
- Entry point: app/src/main.cpp

