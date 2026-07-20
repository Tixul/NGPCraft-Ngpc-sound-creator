# PSG gain calibration

> ## ⚠️ Read this before spending an evening on it: you probably do not need this.
>
> **It requires an external recorder — there is no way around that.** The console cannot
> measure its own audio: the TMP95C061's A/D converter has exactly ONE channel and it is
> wired to the battery voltage (SNK SDK, `SysPro.txt`: *"A/D converter — 1 channel
> (Power management.)"*). Nothing about the sound output reaches it, so no number this
> ROM could put on screen would be a measurement of anything.
>
> And the three things it measures already have answers:
>
> 1. **The absolute level is not a fidelity question at all** — see the arithmetic below.
> 2. **The curve, the summing and the noise are documented**: SNK gives the attenuator as
>    4-bit / 16 levels, and four channels summing in an analogue stage is linear by
>    physical default.
> 3. **Sound from this tool already runs on real consoles** in finished games. That is the
>    whole chain validated where it counts.
>
> So this measures to CONFIRM, not to discover. It is here for the day you want the
> confirmation — with a 3 € 3.5 mm male-male cable into a PC line-in and Audacity, it is
> ten minutes. A phone voice recorder usually will NOT do: they apply automatic gain
> control, which destroys the exact ratios being measured.

Everything needed to settle, on real hardware, the three things about the T6W28 model
that a spec cannot settle by itself.

```
analyse_capture.py    turns a recording into numbers  (this repo)
README.md             this file
```

The **calibration ROM itself lives outside this repo** — it is a full NGPC project
(official Toshiba toolchain) and has no place in the tool's MIT source tree:

```
NGPC_RAG/04_MY_PROJECTS/psg_calibration_rom/
  src/main.c          the sequence
  bin/main.ngc        the built ROM — flash this
  make                rebuild (needs the Toshiba toolchain at C:\t900)
```

## First: what actually needed calibrating, and what did not

The current model peaks at **4096** for a single full-volume tone. That is a headroom
choice, not drift, and it is worth understanding before measuring anything:

| | current model |
|---|---|
| per-channel full scale | 64 × 64 = 4096 |
| mixer | linear sum of 4 channels |
| 4 channels at full volume | 16384 (half of int16 → 6 dB headroom) |

Each channel gets one quarter of a four-channel sum, which is what silicon does — the
four channels add in the analogue stage.

The old model gave each channel **one third** of full scale and then halved the mix; the
new one gives each channel **one quarter** of a four-channel sum. Full-scale output is
the same either way — only the single-channel case moved, and the new split is the one
that matches silicon, where four channels sum in the analogue stage.

**So the absolute level is not a fidelity question.** It is a headroom choice: four
channels at maximum volume land on 16384, exactly half of int16 full scale, leaving
6 dB before clipping. Nothing measured that; nothing needs to.

What hardware *can* settle is the shape:

1. **the attenuation curve** — is it really 2 dB per step, all 15 audible ones?
2. **the summing law** — linear, or does the output stage compress?
3. **noise vs tone** — same volume table, or does the noise sit elsewhere?

## Procedure

### 1. Flash

`NGPC_RAG/04_MY_PROJECTS/psg_calibration_rom/bin/main.ngc`. It loops forever and names
the current segment on screen.

Rebuild with:
```
make            # in psg_calibration_rom/, needs the Toshiba toolchain at C:\t900
```

### 2. Record

Record the **headphone output** into a WAV:

- 44.1 kHz or better, 16-bit
- **no AGC, no normalisation, no noise reduction, no "voice enhancement"** — every one
  of those destroys the exact ratios being measured
- set the input gain so the loudest segment (three channels) stays clear of clipping,
  then **do not touch it again** for the rest of the pass
- capture at least one full pass — about 32 s — ideally two

### 3. Analyse

```
python analyse_capture.py capture.wav
```

It anchors on the double-length MARKER segment, fits the ROM's schedule to the
recording, and prints the measured curve beside the model's.

## The sequence

| segment | plays | measures |
|---|---|---|
| 0 | channel 0, attenuation 0, **2 s** | MARKER — anchors the pass |
| 1..15 | channel 0 alone, attenuation 0..14 | the attenuation curve |
| 16 | channels 0+1, attenuation 0 | the summing law |
| 17 | channels 0+1+2, attenuation 0 | the summing law |
| 18 | noise alone, attenuation 0 | noise vs tone |

Each is 1 s of sound then 0.5 s of silence.

**Attenuation 15 is deliberately not measured.** On the chip 0xF is defined silence, so
that segment would be acoustically identical to the gap after it: nothing to measure,
and it would break segment alignment by turning a 0.5 s hole into a 2.5 s one.

## How to read the result

The script prints a deviation column. Roughly:

- **within ±1.5 dB** — the model describes this chip. Nothing to change.
- **a consistent tilt across the curve** — the attenuation step is not 2 dB. Put the
  measured values into `kApuVolumes` in `core/third_party/ngpc_apu/apu_core.hpp`.
- **summing below the ideal** (+3.01 dB for two channels, +4.77 dB for three) — the
  output stage compresses and the model sums too honestly.
- **noise far from 0 dB** — the noise channel has its own gain stage.

## Sanity note

Feed the script a render of the model itself and it reports **0.00 dB everywhere**,
+3.01 dB for two channels and +4.77 dB for three. That is how it was checked before
anyone flashed anything: if a real capture disagrees, the disagreement is the chip's,
not the script's.

It cannot give an **absolute** level — that depends on your sound card's gain, which is
arbitrary. Every number above is a ratio, and ratios survive any gain.
