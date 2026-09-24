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
TRACKER VIEWS (GRID + PIANO ROLL)
-------------------------------------------------------------------------------
The Tracker tab shows one pattern (TrackerDocument) through two views stacked in
a QStackedWidget, switched with the View buttons or F9:

- TrackerGridWidget : the tracker grid. It stays the SOURCE OF TRUTH for the
  view state (current pattern, cursor channel/row, playback row, mutes) and
  announces it: document_switched, cursor_moved, playback_row_changed,
  channel_mute_changed.
- PianoRollWidget   : mirrors that state (the piano roll voice = the channel of
  the tracker cursor) and edits the same TrackerDocument.

Both views write the same cells, so the .ngps format, the export and the driver
are unchanged. Undo is TrackerDocument's snapshot stack: the piano roll pushes
ONE snapshot per gesture (drag, stroke, paste...).

NoteSpans (app/src/models) converts one channel between cells and bars:
- read  : a note lasts until the next note, note-off or pattern end;
- place : note-on at the start, nothing inside, note-off after unless a note
          starts there (monophonic voice: a bar placed over another cuts it);
- erase : the previous note gets a note-off so it does not ring on;
- replace : erase a group (last first) then place a group (first first), used
          for move / transpose / cut / paste.
Effects stay on their rows. erase + place of the same bar gives back the exact
same cells (checked on 26 real songs, 14 000+ bars).

-------------------------------------------------------------------------------
BUILD
-------------------------------------------------------------------------------
- Qt6 + CMake
- Entry point: app/src/main.cpp

