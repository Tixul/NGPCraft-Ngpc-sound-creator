#include "ngpc/psg.h"

#include <cstring>

namespace ngpc {

namespace {
constexpr uint32_t kNoiseFbWhite = 0x14002;  // bit16 = bit0 ^ bit2 ^ bit15
constexpr uint32_t kNoiseFbPeriodic = 0x08000;
constexpr uint32_t kNoisePreset = 0x0f35;
}

PsgChip::PsgChip()
    : last_reg_(0), rng_(kNoisePreset), noise_fb_(kNoiseFbWhite), update_step_(0) {
    std::memset(reg_, 0, sizeof(reg_));
    std::memset(volume_, 0, sizeof(volume_));
    std::memset(period_, 0, sizeof(period_));
    std::memset(count_, 0, sizeof(count_));
    std::memset(output_, 0, sizeof(output_));
    std::memset(vol_table_, 0, sizeof(vol_table_));
}

void PsgChip::reset(int sample_rate_hz) {
    last_reg_ = 0;
    rng_ = kNoisePreset;
    noise_fb_ = kNoiseFbWhite;

    // Update step: number of PSG ticks per output sample in fixed point.
    update_step_ = static_cast<uint32_t>(
        (static_cast<double>(kStep) * sample_rate_hz * 16.0) / kClockHz);

    std::memset(reg_, 0, sizeof(reg_));
    for (int i = 1; i < 8; i += 2) {
        reg_[i] = 0x0f;  // volume = mute
    }

    for (int i = 0; i < 4; ++i) {
        output_[i] = 0;
        period_[i] = static_cast<int>(update_step_);
        count_[i] = static_cast<int>(update_step_);
    }

    rebuild_vol_table();
    for (int i = 0; i < 4; ++i) {
        volume_[i] = vol_table_[0x0f];
    }
}

void PsgChip::rebuild_vol_table() {
    double out = static_cast<double>(kMaxOutput) / 3.0;
    for (int i = 0; i < 15; ++i) {
        vol_table_[i] = static_cast<int32_t>(out);
        out /= 1.258925412;  // 10^(2/20) = 2 dB step
    }
    vol_table_[15] = 0;
}

void PsgChip::update_tone_period(int reg_index) {
    const int ch = reg_index / 2;
    period_[ch] = static_cast<int>(update_step_ * reg_[reg_index]);
    if (period_[ch] == 0) {
        period_[ch] = static_cast<int>(update_step_);
    }

    if (reg_index == 4) {
        // If noise uses tone3 clock, update noise period.
        if ((reg_[6] & 0x03) == 0x03) {
            period_[3] = 2 * period_[2];
        }
    }
}

void PsgChip::update_noise_period() {
    const int n = reg_[6] & 0x03;
    period_[3] = (n == 3) ? 2 * period_[2] : (static_cast<int>(update_step_) << (5 + n));

    rng_ = kNoisePreset;
    output_[3] = rng_ & 1;
}

void PsgChip::write(uint8_t data) {
    if (data & 0x80) {
        const int r = (data & 0x70) >> 4;
        const int ch = r / 2;

        last_reg_ = r;
        reg_[r] = (reg_[r] & 0x3f0) | (data & 0x0f);

        switch (r) {
        case 0:
        case 2:
        case 4:
            update_tone_period(r);
            break;
        case 1:
        case 3:
        case 5:
        case 7:
            volume_[ch] = vol_table_[data & 0x0f];
            break;
        case 6: {
            const int mode = reg_[6];
            // Bit2 selects noise type (1=white, 0=periodic).
            noise_fb_ = (mode & 4) ? kNoiseFbWhite : kNoiseFbPeriodic;
            update_noise_period();
            break;
        }
        default:
            break;
        }
        return;
    }

    const int r = last_reg_;
    const int ch = r / 2;

    switch (r) {
    case 0:
    case 2:
    case 4:
        reg_[r] = (reg_[r] & 0x0f) | ((data & 0x3f) << 4);
        update_tone_period(r);
        break;
    case 1:
    case 3:
    case 5:
    case 7:
        volume_[ch] = vol_table_[data & 0x0f];
        break;
    default:
        break;
    }
}

int16_t PsgChip::sample() {
    int32_t vol[4] = {0, 0, 0, 0};

    if (mode_ == Mode::kTone) {
        // Tone channels (0..2)
        for (int i = 0; i < 3; ++i) {
            if (output_[i]) {
                vol[i] += count_[i];
            }
            count_[i] -= kStep;

            while (count_[i] <= 0) {
                count_[i] += period_[i];
                if (count_[i] > 0) {
                    output_[i] ^= 1;
                    if (output_[i]) {
                        vol[i] += period_[i];
                    }
                    break;
                }
                count_[i] += period_[i];
                vol[i] += period_[i];
            }

            if (output_[i]) {
                vol[i] -= count_[i];
            }
        }

        int64_t out = 0;
        out += static_cast<int64_t>(vol[0]) * volume_[0];
        out += static_cast<int64_t>(vol[1]) * volume_[1];
        out += static_cast<int64_t>(vol[2]) * volume_[2];

        if (out > static_cast<int64_t>(kMaxOutput) * kStep) {
            out = static_cast<int64_t>(kMaxOutput) * kStep;
        }

        return static_cast<int16_t>(out / kStep);
    }

    // Noise channel (3) only
    int left = kStep;
    do {
        const int next = (count_[3] < left) ? count_[3] : left;

        if (output_[3]) {
            vol[3] += count_[3];
        }
        count_[3] -= next;
        if (count_[3] <= 0) {
            if (rng_ & 1) {
                rng_ ^= noise_fb_;
            }
            rng_ >>= 1;
            output_[3] = rng_ & 1;
            count_[3] += period_[3];
            if (output_[3]) {
                vol[3] += period_[3];
            }
        }
        if (output_[3]) {
            vol[3] -= count_[3];
        }

        left -= next;
    } while (left > 0);

    int64_t out = static_cast<int64_t>(vol[3]) * volume_[3];
    if (out > static_cast<int64_t>(kMaxOutput) * kStep) {
        out = static_cast<int64_t>(kMaxOutput) * kStep;
    }
    return static_cast<int16_t>(out / kStep);
}

void PsgMixer::reset(int sample_rate_hz) {
    std::lock_guard<std::mutex> lock(mutex_);
    tone_.set_mode(PsgChip::Mode::kTone);
    noise_.set_mode(PsgChip::Mode::kNoise);
    tone_.reset(sample_rate_hz);
    noise_.reset(sample_rate_hz);
}

void PsgMixer::write_tone(uint8_t data) {
    std::lock_guard<std::mutex> lock(mutex_);
    tone_.write(data);
}

void PsgMixer::write_noise(uint8_t data) {
    std::lock_guard<std::mutex> lock(mutex_);
    noise_.write(data);
}

void PsgMixer::render(int16_t* out, int frames) {
    std::lock_guard<std::mutex> lock(mutex_);
    for (int i = 0; i < frames; ++i) {
        const int16_t tone_sample = tone_.sample();
        const int16_t noise_sample = noise_.sample();
        out[i] = static_cast<int16_t>((tone_sample + noise_sample) / 2);
    }
}

void PsgChip::set_mode(Mode mode) {
    mode_ = mode;
}

PsgChip::Mode PsgChip::mode() const {
    return mode_;
}

}  // namespace ngpc
