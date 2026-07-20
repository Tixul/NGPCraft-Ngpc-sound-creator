/* apu_core.hpp — the T6W28, ours.
 *
 * PROVENANCE. Vendored from the NGPCraft emulator:
 *
 *     Ngpcraft_emulator/cpp/src/apu.hpp
 *     Ngpcraft_emulator/cpp/src/apu.cpp
 *
 * Clean-room, written from specs/APU_T6W28.md. The emulator keeps a Python model
 * (core/apu.py) as its ORACLE and holds this code to it command for command with a
 * differential test. Nothing here is ported from any third-party emulator.
 *
 * ⚠️ THE ONE DELIBERATE DIVERGENCE FROM THE EMULATOR: THE SAMPLE RATE IS A VARIABLE.
 * The emulator only ever runs at 44 100 Hz and hard-codes it as a constant. This tool
 * does not -- the WAV exporter takes the rate from user settings -- and a chip that
 * silently produces 44 100 Hz samples while the header says 48 000 is a chip that
 * detunes the export. So `sample_rate_hz` is a member here and the two places that
 * used the constant (the 16.16 sample step, and tick()'s chip-clock -> sample
 * conversion) read the member instead. Keep that in mind when re-syncing.
 *
 * WHO WRITES TO IT, AND WHERE — MEASURED, NOT ASSUMED (emulator DEVLOG pass 209)
 * ------------------------------------------------------------------------------
 * Every write the sound drivers of all 73 commercial ROMs aim at the chip was
 * recorded before any of this was wired:
 *
 *     71 of 73 ROMs drive the chip.
 *     Z80 MEMORY writes land on EXACTLY TWO addresses -- 0x4000 and 0x4001.
 *     Nothing else in the whole 0x4000..0x7FFF window is ever touched.
 *
 * The chip has a RIGHT port and a LEFT port and address bit A0 picks between them:
 *
 *     0x4000 -> RIGHT      0x4001 -> LEFT
 *
 * ⚡ AND THE TWO PORTS ARE NOT SYMMETRIC. This is the T6W28's real quirk and the
 * thing a two-chip model cannot express:
 *
 *     LEFT  -> the tone periods (channels 0..2)
 *     RIGHT -> the noise control, and tone-3's period reused as the noise period
 *     volumes work on BOTH, one stereo side each -- that IS the stereo
 *
 * Which is exactly why a driver that mirrors every command byte to both ports gets
 * correct MONO out of real silicon: the tone periods take effect through LEFT, the
 * noise control through RIGHT, and each volume write sets its own side to the same
 * value. That is what `driver_custom_latest` does by design.
 */
#ifndef NGPC_APU_CORE_HPP
#define NGPC_APU_CORE_HPP

#include <algorithm>
#include <cstdint>

namespace ngpc {
namespace apu {

/* The chip lives in the Z80's address space, so it lives in the Z80's clock
 * domain: half the main CPU's 6.144 MHz. */
constexpr uint32_t kApuClockHz = 3072000;          /* = main clock / 2 */
constexpr uint32_t kMainClockHz = kApuClockHz * 2;

/* Logarithmic attenuation, index 0 = full, 15 = silent. Mirrors core/apu.py. */
constexpr int kApuVolumes[16] = {64, 50, 39, 31, 24, 19, 15, 12, 9, 7, 5, 4, 3, 2, 1, 0};
constexpr int kNoisePeriods[3] = {0x100, 0x200, 0x400};
constexpr int kTapWhite    = 13;
constexpr int kTapDisabled = 16;

/* A square channel is muted below this period: the spec's anti-alias guard. */
constexpr int kMinAudiblePeriod = 128;

struct Square {
    int vol_left  = 0;
    int vol_right = 0;
    int period    = 0;   /* 14 bits, ALREADY multiplied by 16 (see the latch) */
    int phase     = 0;
    int counter   = 0;
};

struct Noise {
    int shifter       = 0x4000;
    int tap           = kTapWhite;
    int period_select = 0;      /* 0..2 -> kNoisePeriods, 3 -> period_extra */
    int period_extra  = 0;
    int vol_left      = 0;
    int vol_right     = 0;
    int counter       = 0;
};

struct Apu {
    Square square[3];
    Noise  noise;
    uint8_t latch_left  = 0;    /* (index << 1) | is_volume */
    uint8_t latch_right = 0;

    /* Debug channel enable mask: bit0..2 squares, bit3 noise. Muting drops a channel
     * from the MIX only -- its oscillator keeps advancing, so un-muting does not jump
     * the phase. (The emulator's bit4 DAC is main-CPU-only and has no meaning here.) */
    uint8_t channel_mask = 0x0F;

    uint32_t sample_rate_hz = 44100;   /* see the header note: a variable, not a constant */

    /* chip clocks not yet converted into an output sample, and the 16.16 remainder
     * of the fractional sample step. Carrying both is what keeps the audio clock
     * from drifting over a long render. */
    uint64_t chip_residue = 0;
    uint32_t step_fp = 0;

    /* The output ring, stereo interleaved. */
    static constexpr uint32_t kRingFrames = 16384;
    int16_t  ring[kRingFrames * 2] = {};
    uint64_t produced = 0;
    uint64_t drained  = 0;

    void reset(uint32_t rate) {
        const uint32_t keep = (rate > 0) ? rate : 44100u;
        *this = Apu();
        sample_rate_hz = keep;
    }

    uint64_t available() const { return produced - drained; }

    void write_left(uint8_t data)  { write(data, true); }
    void write_right(uint8_t data) { write(data, false); }

    /* Advance by `chip_cycles` of the CHIP clock, emitting output samples. */
    void tick(uint32_t chip_cycles) {
        if (chip_cycles == 0) return;
        /* How many output samples do these chip-clocks buy? Exactly
         * `chip * rate / 3072000`, remainder carried -- NOT `chip / 69`, which in the
         * emulator produced 44 522 Hz worth of samples per second while claiming
         * 44 100. */
        chip_residue += uint64_t(chip_cycles) * sample_rate_hz;
        uint64_t samples = chip_residue / kApuClockHz;
        chip_residue %= kApuClockHz;
        samples = std::min<uint64_t>(samples, kRingFrames);
        for (uint64_t i = 0; i < samples; ++i) emit_sample();
    }

    /* Copy up to `n` stereo frames (interleaved L,R) out; returns how many. */
    uint32_t drain(int16_t* out, uint32_t n) {
        const uint32_t want = uint32_t(std::min<uint64_t>(available(), n));
        for (uint32_t i = 0; i < want; ++i) {
            const uint32_t slot = uint32_t((drained + i) % kRingFrames) * 2;
            out[i * 2]     = ring[slot];
            out[i * 2 + 1] = ring[slot + 1];
        }
        drained += want;
        return want;
    }

private:
    int active_noise_period() const {
        if (noise.period_select < 3) return kNoisePeriods[noise.period_select];
        return noise.period_extra;
    }

    void write(uint8_t data, bool left) {
        uint8_t& latch_reg = left ? latch_left : latch_right;
        const uint8_t latch = (data & 0x80) ? data : latch_reg;
        if (data & 0x80) latch_reg = data;

        const int index = (latch >> 5) & 3;

        if (latch & 0x10) {                                  /* volume */
            const int amp = kApuVolumes[data & 0x0F];
            if (index < 3) {
                if (left) square[index].vol_left = amp;
                else      square[index].vol_right = amp;
            } else {
                if (left) noise.vol_left = amp;
                else      noise.vol_right = amp;
            }
            return;
        }

        if (left) {
            if (index >= 3) return;                          /* no tone-3 on this chip */
            Square& sq = square[index];
            /* The 10-bit period arrives in two bytes and is stored ALREADY SHIFTED
             * LEFT BY FOUR -- i.e. multiplied by the chip's own /16 divider -- which
             * is why `period` is in chip clocks and why the mute threshold is 128 and
             * not 8. Do not "fix" the shift. */
            if (data & 0x80) sq.period = (sq.period & 0x3F00) | ((data << 4) & 0x00FF);
            else             sq.period = (sq.period & 0x00FF) | ((data << 8) & 0x3F00);
            return;
        }

        if (index == 2) {                                    /* the noise's extra period */
            if (data & 0x80)
                noise.period_extra = (noise.period_extra & 0x3F00) | ((data << 4) & 0x00FF);
            else
                noise.period_extra = (noise.period_extra & 0x00FF) | ((data << 8) & 0x3F00);
            return;
        }
        if (index == 3) {                                    /* the noise control */
            noise.period_select = data & 3;
            noise.tap     = (data & 0x04) ? kTapWhite : kTapDisabled;
            noise.shifter = 0x4000;                          /* every reconfig resets it */
        }
    }

    /* One output sample. The oscillators advance a whole sample-period at a time --
     * never clock by clock: at 3.072 MHz that would be millions of steps a second.
     *
     * This is a POINT sample, which the spec allows in as many words and which
     * aliases. Band-limited synthesis is a later pass, not a silent one. */
    void emit_sample() {
        /* ⚠️ THE SAMPLE STEP IS FRACTIONAL, AND TRUNCATING IT DETUNES THE WHOLE CHIP.
         * 3 072 000 / 44 100 = 69.66 chip clocks per sample. As an integer divide (69)
         * the oscillators advanced 0.95 % too slowly: every note came out sharp and the
         * audio clock ran at 100.9 % of real time. Fixed point 16.16, remainder carried
         * so the error cannot accumulate. */
        const uint32_t step_inc =
            uint32_t((uint64_t(kApuClockHz) << 16) / (sample_rate_hz ? sample_rate_hz : 44100u));
        step_fp += step_inc;
        const uint32_t step = step_fp >> 16;
        step_fp &= 0xFFFF;

        int left = 0, right = 0;

        for (int i = 0; i < 3; ++i) {
            Square& sq = square[i];
            if (sq.period <= kMinAudiblePeriod || (!sq.vol_left && !sq.vol_right)) continue;
            sq.counter += int(step);
            const int toggles = sq.counter / sq.period;
            sq.counter %= sq.period;
            sq.phase ^= (toggles & 1);
            if (!(channel_mask & (1u << i))) continue;   // muted: advanced, not mixed
            const int sign = sq.phase ? 1 : -1;
            left  += sign * sq.vol_left;
            right += sign * sq.vol_right;
        }

        if (noise.vol_left || noise.vol_right) {
            const int period = std::max(1, 2 * active_noise_period());
            noise.counter += int(step);
            int steps = noise.counter / period;
            noise.counter %= period;
            /* Bounded: a tiny extra-period could otherwise ask for thousands of LFSR
             * steps per sample. 64 is far more than any real period needs. */
            steps = std::min(steps, 64);
            for (int i = 0; i < steps; ++i) {
                noise.shifter = (((noise.shifter << 14) ^ (noise.shifter << noise.tap)) & 0x4000)
                              | (noise.shifter >> 1);
            }
            if (channel_mask & 0x08) {
                const int sign = (noise.shifter & 1) ? -1 : 1;
                left  += sign * noise.vol_left;
                right += sign * noise.vol_right;
            }
        }

        /* Four channels at 64 each = +-256 worst case; scale to a comfortable
         * headroom rather than clipping the one frame where they all align. */
        const int16_t l = int16_t(std::clamp(left  * 64, -32768, 32767));
        const int16_t r = int16_t(std::clamp(right * 64, -32768, 32767));

        if (available() >= kRingFrames) ++drained;   /* host fell behind: drop oldest */
        const uint32_t slot = uint32_t(produced % kRingFrames) * 2;
        ring[slot]     = l;
        ring[slot + 1] = r;
        ++produced;
    }
};

}  // namespace apu
}  // namespace ngpc

#endif
