# Custom NGPC Driver (latest copy)

This folder is the canonical location for the most up-to-date custom driver.
Always update these files when the driver changes.

**Files**
- `sounds.c`
- `sounds.h`
- `sounds_game_sfx_template.c` (optional template for game-specific SFX mapping)

**Capabilities (Exhaustive)**
- Embedded Z80 polling driver (`s_z80drv[]`) in Z80 RAM 0x0000.., shared RAM window 0x7000.. (CPU side).
- Multi-command PSG buffer (up to 5 PSG commands per frame).
- Non-blocking buffer policy with drop counter and fault flag.
- Dual-port PSG writes for mono (0x4000 + 0x4001).
- Tone channels (3) + noise channel (1) with per-channel base regs.
- SFX playback on 4 channels with per-channel timers.
- SFX tone sweep (linear up/down), optional ping-pong sweep.
- SFX envelope (step + speed) for tone and noise.
- SFX noise burst mode (duration override).
- BGM playback on up to 4 voices (3 tone + 1 noise).
- Frame-locked BGM scheduling (VBlank-based) to avoid tempo drift.
- BGM loop support via loop offsets per stream.
- BGM speed multiplier (global tempo scaling).
- BGM gate (percent) to shorten notes (`Bgm_SetGate`).
- BGM per-voice instrument preset (BGM_OP_SET_INST).
- BGM per-voice envelopes, vibrato, sweep (stream opcodes).
- BGM per-voice ADSR envelope with sustain-rate support (AR/DR/SL/SR/RR).
- BGM per-voice expression (additional attn offset, 0-15).
- BGM dual-LFO modulation (LFO1 + LFO2) with algorithm routing (0..7).
- BGM global fade out (progressive attn increase, auto-stops BGM at silence).
- Fade state reset on `Bgm_Start*` and `Bgm_Stop` (prevents residual attenuation between songs).
- BGM runtime tempo change (via stream opcode or API).
- BGM host commands (fade out, tempo change embedded in streams).
- BGM per-voice macro steps (optional instrument macro table).
- Optional envelope curves (per-instrument LUT).
- Optional pitch curves (per-instrument LUT).
- Instrument presets can select `macro_id` and `env_curve_id` (see `sounds.c`).
- Instrument presets can select `pitch_curve_id` (see `sounds.c`).
- Optional compile flags (default on): `SOUNDS_ENABLE_MACROS`, `SOUNDS_ENABLE_ENV_CURVES`,
  `SOUNDS_ENABLE_PITCH_CURVES`.
- Default pitch curves (IDs):
  - 0 = none
  - 1 = gentle down
  - 2 = gentle up
  - 3 = wobble
  - 4 = fast fall
- Example data is optional (default on): `SOUNDS_ENABLE_EXAMPLE_PRESETS`.
- Instrument presets use `BGM_INST(...)` for readability (see `sounds.c`).
- Default instrument IDs (from `sounds.c`): 0 clean, 1 noise, 2 lead, 3 pad, 4 pluck,
  5 bass, 6 bell, 7 zap.
- Automatic BGM channel restore after SFX ends (restore flags).
- PSG reset bases on init/reset (hardware stability).
- Latch-only silence writes to avoid partial data-byte effects.
- Debug counters and snapshot hooks (BGM_DEBUG).

**Public API (sounds.h)**
- `Sounds_Init`, `Sounds_ResetState`, `Sounds_Update`.
- `Sounds_DebugFault`, `Sounds_DebugDrops`, `Sounds_DebugLastSfx`.
- `Sfx_Update`, `Sfx_Play`, `Sfx_PlayToneCh`, `Sfx_PlayToneEx`, `Sfx_PlayNoise`, `Sfx_PlayNoiseEx`.
- `Sfx_SendBytes`, `Sfx_BufferBegin`, `Sfx_BufferPush`, `Sfx_BufferCommit`, `Sfx_Stop`.
- `Bgm_Start`, `Bgm_StartEx`, `Bgm_StartLoop`, `Bgm_StartLoop2`, `Bgm_StartLoop2Ex`, `Bgm_StartLoop3`, `Bgm_StartLoop3Ex`, `Bgm_StartLoop4`, `Bgm_StartLoop4Ex`, `Bgm_Stop`.
- `Bgm_SetNoteTable(const u8* note_table)` to switch active note table per song at runtime.
- `Bgm_Update`, `Bgm_SetSpeed`, `Bgm_SetGate`.
- `Bgm_FadeOut(u8 speed)` — start a progressive fade-out (0 = cancel, >0 = frames between fade steps).
- `Bgm_FadeOut(0)` also clears fade accumulators (`speed/counter/attn`) in current driver behavior.
- `Bgm_SetTempo(u8 speed)` — change BGM speed multiplier at runtime.
- `Bgm_DebugReset`, `Bgm_DebugSnapshot`.

**BGM Stream Format (current)**
- Stream is a sequence of bytes; events are parsed at runtime.
- Note event: `note` (1..NOTE_MAX_INDEX+1) then `duration` (frames scaled by speed).
- End marker: `0x00` (stop, or loop if loop enabled).
- Rest: `0xFF` then `duration` (silence for duration).
- `BGM_OP_SET_ATTN (0xF0)`: next byte = attenuation (0..15, absolute base value for the voice).
- `BGM_OP_SET_ENV (0xF1)`: next bytes = env_step, env_speed.
- `BGM_OP_SET_VIB (0xF2)`: next bytes = depth, speed, delay.
- `BGM_OP_SET_SWEEP (0xF3)`: next bytes = end_lo, end_hi, step (s8), speed.
- `BGM_OP_SET_INST (0xF4)`: next byte = instrument id.
- `BGM_OP_SET_PAN (0xF5)`: next byte = pan payload (reserved for stereo phase; currently consumed as no-op).
- `BGM_OP_HOST_CMD (0xF6)`: next 2 bytes = type, data. Type 0 = fade out (data=speed), Type 1 = tempo change (data=speed).
- `BGM_OP_SET_EXPR (0xF7)`: next byte = expression (0-15, additional per-voice attn offset).
- `BGM_OP_SET_ADSR (0xF9)`: next 4 bytes = attack, decay, sustain level, release (legacy ADSR; SR=0).
- `BGM_OP_SET_LFO (0xFA)`: next 3 bytes = `wave, rate, depth` (legacy LFO1 shorthand).
- `BGM_OP_EXT (0xFE)`: extended modulation payload.
  - `0x01` = ADSR5: `A, D, SL, SR, RR`.
  - `0x02` = MOD2: `algo, lfo1_on,wave,hold,rate,depth, lfo2_on,wave,hold,rate,depth`.
- Example (single voice): `note, dur, note, dur, 0xF0, attn, 0x00`.
- NOTE_TABLE and stream arrays are provided by your build (e.g. generated by `midi_to_ngpc`).

**Tracker Export Integration (music + instruments)**
- The Tracker export now produces:
  - music data file (`*.c` or `*.inc`) containing `NOTE_TABLE`, `BGM_CH0..BGM_CHN`, and loop offsets.
  - companion instrument file (`*_instruments.c`) containing the `s_bgm_instruments[]` table.
- For correct runtime parity, keep these two files paired from the same export.

Recommended game-side workflow:
1. Copy the exported music file into your game source tree.
2. Copy the exported `*_instruments.c` file into your game source tree.
3. Update driver instrument table in `sounds.c`:
   - either replace the existing `s_bgm_instruments[]` block with the exported one,
   - or include the exported file at the instrument-table location.
4. Build and run with your normal BGM start call (`Bgm_StartLoop*` / `Bgm_StartLoop*Ex`).

Notes:
- If streams use `BGM_OP_SET_INST (0xF4)`, missing or stale instrument tables will change playback.
- Do not mix a music stream from export A with an instrument table from export B.
- `mode`/`noise_config` are preserved in presets; runtime BGM noise behavior remains channel-based.
- Tracker export may prepend an "Export audit warnings" comment block (in `.c`/`.inc`):
  - this is informational and does not change runtime parsing.
  - warnings indicate parity risks (invalid IDs, unsupported FX nibble, note-table pressure, etc.).
- In hybrid export, keep global commands on channel 0 when possible:
  - `Bxx` (speed) and `Exy` (host command) are global by nature.
  - placing them on CH0 avoids ambiguous authoring and keeps streams deterministic.
- If tone note diversity exceeds NOTE_TABLE capacity (51), export falls back to nearest divider match.

**Project Export-All Integration (new files)**
- `NGPC Sound Creator` Export All now also generates:
  - `exports/project_audio_manifest.txt` (song list, files, symbol prefixes)
  - `exports/project_audio_api.h` + `exports/project_audio_api.c` (C-side song registry)
- Song symbols are namespaced as `PROJECT_<SONG_ID>_*` to avoid collisions.
- This API is useful as a central index for multi-song integration.
- Runtime helper available in generated API:
  - `NgpcProject_BgmStartLoop4ByIndex(i)` automatically calls `Bgm_SetNoteTable(song->note_table)`
    then `Bgm_StartLoop4Ex(...)` for the selected song.
  - Example: `NgpcProject_BgmStartLoop4ByIndex(0);`
- `project_audio_api.c` also defines a weak `NOTE_TABLE` fallback symbol for link compatibility
  in fully-namespaced multi-song exports.
- Manual path still works:
  1) call `Bgm_SetNoteTable(song_note_table)`
  2) call `Bgm_StartLoop4Ex(song streams/loops...)`

**Attenuation Model (important)**
- PSG attenuation is inverse volume: `0 = loudest`, `15 = silent`.
- `BGM_OP_SET_ATTN` is absolute (replace), not additive with instrument base attenuation.
- Final voice attenuation is built as:
  1) base attenuation (`SET_ATTN` or instrument default)
  2) instrument-time modulation (ADSR/env/macro)
  3) expression offset (`SET_EXPR`)
  4) global fade attenuation
- Final value is clamped to `0..15`.

**Fade Behavior (current/fixed)**
- Starting a new BGM (`Bgm_Start*`) resets fade state.
- Stopping BGM (`Bgm_Stop`) resets fade state.
- Calling `Bgm_FadeOut(0)` cancels fade and resets fade attenuation.
- This prevents quiet carry-over on the next song after a previous fade-out.

**SFX Behavior (current)**
- SFX uses frame timers per channel (0..3).
- Tone SFX supports sweep, envelope, vibrato.
- Noise SFX supports envelope and burst.
- Burst mode alternates on/off pulses for a gated noise effect.
- When SFX ends, channel is silenced and BGM restore is triggered.
- `Sfx_Play` is a no-op in the clean driver; use `Sfx_PlayPresetTable` or define `SFX_PLAY_EXTERNAL` and provide your own mapping.
- Data-driven presets are supported via `SfxPreset`/`Sfx_PlayPresetTable` (see comment in `sounds.h`).
- Template: `sounds_game_sfx_template.c` shows how to keep game-specific SFX mappings out of the driver.

**Project SFX Export Integration (`project_sfx.c`)**
- `NGPC Sound Creator` project export generates `exports/project_sfx.c`.
- This file is a **single global bank** for all project SFX (not one file per SFX).
- Arrays provided:
  - `PROJECT_SFX_COUNT`
  - `PROJECT_SFX_TONE_ON[]`, `PROJECT_SFX_TONE_CH[]`
  - `PROJECT_SFX_TONE_DIV[]`
  - `PROJECT_SFX_TONE_ATTN[]`
  - `PROJECT_SFX_TONE_FRAMES[]`
  - `PROJECT_SFX_TONE_SW_ON[]`, `PROJECT_SFX_TONE_SW_END[]`, `PROJECT_SFX_TONE_SW_STEP[]`,
    `PROJECT_SFX_TONE_SW_SPEED[]`, `PROJECT_SFX_TONE_SW_PING[]`
  - `PROJECT_SFX_TONE_ENV_ON[]`, `PROJECT_SFX_TONE_ENV_STEP[]`, `PROJECT_SFX_TONE_ENV_SPD[]`
  - `PROJECT_SFX_NOISE_ON[]`, `PROJECT_SFX_NOISE_RATE[]`, `PROJECT_SFX_NOISE_TYPE[]`, `PROJECT_SFX_NOISE_ATTN[]`
  - `PROJECT_SFX_NOISE_FRAMES[]`, `PROJECT_SFX_NOISE_BURST[]`, `PROJECT_SFX_NOISE_BURST_DUR[]`
  - `PROJECT_SFX_NOISE_ENV_ON[]`, `PROJECT_SFX_NOISE_ENV_STEP[]`, `PROJECT_SFX_NOISE_ENV_SPD[]`

Recommended integration:
1. Add `project_sfx.c` to your game build as a normal source file.
2. Define `SFX_PLAY_EXTERNAL 1`.
3. Implement `Sfx_Play(u8 id)` in game code (or in `sounds_game_sfx_template.c`) using exported arrays.

Reference implementation:
```c
#include "sounds.h"

extern const unsigned char PROJECT_SFX_COUNT;
extern const unsigned char PROJECT_SFX_TONE_ON[];
extern const unsigned char PROJECT_SFX_TONE_CH[];
extern const unsigned short PROJECT_SFX_TONE_DIV[];
extern const unsigned char PROJECT_SFX_TONE_ATTN[];
extern const unsigned char PROJECT_SFX_TONE_FRAMES[];
extern const unsigned char PROJECT_SFX_TONE_SW_ON[];
extern const unsigned short PROJECT_SFX_TONE_SW_END[];
extern const signed short PROJECT_SFX_TONE_SW_STEP[];
extern const unsigned char PROJECT_SFX_TONE_SW_SPEED[];
extern const unsigned char PROJECT_SFX_TONE_SW_PING[];
extern const unsigned char PROJECT_SFX_TONE_ENV_ON[];
extern const unsigned char PROJECT_SFX_TONE_ENV_STEP[];
extern const unsigned char PROJECT_SFX_TONE_ENV_SPD[];
extern const unsigned char PROJECT_SFX_NOISE_ON[];
extern const unsigned char PROJECT_SFX_NOISE_RATE[];
extern const unsigned char PROJECT_SFX_NOISE_TYPE[];
extern const unsigned char PROJECT_SFX_NOISE_ATTN[];
extern const unsigned char PROJECT_SFX_NOISE_FRAMES[];
extern const unsigned char PROJECT_SFX_NOISE_BURST[];
extern const unsigned char PROJECT_SFX_NOISE_BURST_DUR[];
extern const unsigned char PROJECT_SFX_NOISE_ENV_ON[];
extern const unsigned char PROJECT_SFX_NOISE_ENV_STEP[];
extern const unsigned char PROJECT_SFX_NOISE_ENV_SPD[];

void Sfx_Play(u8 id)
{
    if (id >= PROJECT_SFX_COUNT) return;

    if (PROJECT_SFX_TONE_ON[id]) {
        Sfx_PlayToneEx(PROJECT_SFX_TONE_CH[id], PROJECT_SFX_TONE_DIV[id], PROJECT_SFX_TONE_ATTN[id],
                       PROJECT_SFX_TONE_FRAMES[id], PROJECT_SFX_TONE_SW_END[id], PROJECT_SFX_TONE_SW_STEP[id],
                       PROJECT_SFX_TONE_SW_SPEED[id], PROJECT_SFX_TONE_SW_PING[id], PROJECT_SFX_TONE_SW_ON[id],
                       PROJECT_SFX_TONE_ENV_ON[id], PROJECT_SFX_TONE_ENV_STEP[id], PROJECT_SFX_TONE_ENV_SPD[id]);
    }

    if (PROJECT_SFX_NOISE_ON[id]) {
        Sfx_PlayNoiseEx(PROJECT_SFX_NOISE_RATE[id], PROJECT_SFX_NOISE_TYPE[id], PROJECT_SFX_NOISE_ATTN[id],
                        PROJECT_SFX_NOISE_FRAMES[id], PROJECT_SFX_NOISE_BURST[id], PROJECT_SFX_NOISE_BURST_DUR[id],
                        PROJECT_SFX_NOISE_ENV_ON[id], PROJECT_SFX_NOISE_ENV_STEP[id], PROJECT_SFX_NOISE_ENV_SPD[id]);
    }
}
```

Gameplay call example:
```c
Sfx_Play(0); /* e.g. menu confirm */
Sfx_Play(1); /* e.g. menu cancel */
```

Notes:
- The above values (`frames`, sweep/env flags) are intentionally conservative defaults.
- Tune `Sfx_PlayToneEx` / `Sfx_PlayNoiseEx` timing per game if you want longer/shorter SFX tails.
- If you prefer static mappings, map gameplay events to fixed IDs in one central enum.

**Known Limits**
- Buffer max is 5 PSG commands per frame.
- Noise channel is monophonic and shares the noise register.
- Stream uses NOTE_TABLE for divider lookup (NOTE_MAX_INDEX = 50).
- This is a polling Z80 driver (no NMI/COMM protocol).
- Tone voices cannot switch into noise mode (only noise channel uses noise mode).
- Optional: `SOUNDS_MAX_CATCHUP` can clamp long frame catch-up loops.
- `BGM_OP_SET_PAN (0xF5)` is currently parsed as no-op (reserved for a future stereo phase).
- No native DAC/PCM sample playback path in this PSG-focused driver.
- No built-in VGM parser/player in runtime API (tool export pipeline is tracker/MIDI-oriented).

**Provenance**
- Runtime code in this folder is self-contained (`sounds.c/.h`) and does not include/link `vgmlib-ngpc` sources.
- No VGM runtime parser/player from `vgmlib-ngpc` is embedded in this driver.
