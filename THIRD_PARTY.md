# Third-Party Notes

## Emulation code: NONE. All of it is ours.

This tool contains no third-party emulation code. Both cores are first-party,
clean-room, vendored from the NGPCraft emulator:

- **Z80 CPU** — `core/third_party/ngpc_z80/z80_core.hpp`
  From `Ngpcraft_emulator/cpp/src/z80.cpp` + `z80.hpp`, written from the documented
  Zilog instruction set. Made bus-agnostic here (templated on a `Bus` supplying
  `read8/write8/in8/out8`).
- **T6W28 PSG** — `core/third_party/ngpc_apu/apu_core.hpp`
  From `Ngpcraft_emulator/cpp/src/apu.cpp` + `apu.hpp`, written from
  `specs/APU_T6W28.md`. The emulator holds that code to a Python model
  (`core/apu.py`) as its oracle with a differential test.

> ⚠️ The directory is still named `third_party/`. That name is now inaccurate — the
> code in it is first-party. Renaming it is cosmetic and was left alone rather than
> churn a verified change.

### Re-syncing

Both are snapshots, not live dependencies: the two repositories are independent. Each
file's header documents the mechanical rule for re-applying an upstream fix, and the
deliberate divergences (the Z80's bus seam; the PSG's sample rate being a runtime
variable here, because this tool's WAV exporter takes the rate from user settings
while the emulator only ever runs at 44 100 Hz).

## Bundled runtime libraries (NOT MIT)

The distributed Windows build carries, alongside the MIT-licensed application:

- **Qt 6** — used under **LGPL v3**. The Qt DLLs are dynamically linked and the end
  user may replace them, which is what LGPL v3 requires. <https://www.qt.io/licensing>
- **FFmpeg** (`avcodec`, `avformat`, `avutil`, `swresample`, `swscale`) — shipped by
  Qt Multimedia under **LGPL v2.1+**. <https://www.ffmpeg.org/legal.html>

Distributing the binary means complying with the LGPL for those DLLs. It does not
constrain reuse of this project's own MIT source. See `LICENSE`.

## ⚡ One chip, two ports — how the PSG is modelled

The T6W28 is ONE chip with a RIGHT port and a LEFT port, and they are **not symmetric**
(measured in the emulator across all 73 commercial ROMs, which touch 0x4000 and 0x4001
and nothing else in the whole window):

```
0x4001 = LEFT   -> the tone periods (channels 0..2)
0x4000 = RIGHT  -> the noise control, and tone-3's period reused as the noise period
volume writes   -> work on BOTH ports, one stereo side each — that IS the stereo
```

`PsgMixer::write_tone` / `write_noise` keep those names for source compatibility. Read
them as "port 0x4001" and "port 0x4000". A driver that mirrors every command byte to
both ports (as `driver_custom_latest` does — "dual-port PSG writes for mono") drives
the tone periods through LEFT and the noise control through RIGHT, and gets correct
mono out of one chip.

**Still mono at the output.** The chip is stereo internally; `render()` averages the two
sides because this tool's audio path is mono end to end. With a mirroring driver the
sides are equal, so nothing is lost — and going stereo is now a change to the audio
path alone, not to the chip.

**Levels are set for headroom, not matched to any past build.** Four channels at full
volume land on 16384 — half of int16 full scale, so 6 dB before clipping. Each channel
gets one quarter of a four-channel sum, which is what silicon does (the four channels
sum in the analogue stage). The absolute level is a headroom choice, not a fidelity
question. What hardware *can* settle — the attenuation curve, the summing law, noise vs
tone — has a ROM and an analysis script ready in `tools/psg_calibration/`. The script
reports 0.00 dB against a render of the model itself, so any disagreement a real capture
shows belongs to the chip.
