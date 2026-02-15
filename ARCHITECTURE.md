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
- psg/     : T6W28 emulator (NeoPop-based)
- z80/     : Z80 core for driver execution (Marat Fayzullin, non-commercial)
- midi/    : parser + K1Sound rules
- format/  : K1Sound encoder (group/list + BGM/SE)
- project/ : project model (JSON)
- polling_driver/ : built-in Z80 polling driver + host buffer (quick tests)

-------------------------------------------------------------------------------
BUILD
-------------------------------------------------------------------------------
- Qt6 + CMake
- Entry point: app/src/main.cpp

