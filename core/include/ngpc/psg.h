#pragma once

#include <cstdint>
#include <mutex>

namespace ngpc {

class PsgChip {
public:
    enum class Mode {
        kTone,
        kNoise,
    };

    PsgChip();
    void reset(int sample_rate_hz);
    void write(uint8_t data);
    int16_t sample();
    void set_mode(Mode mode);
    Mode mode() const;

private:
    static constexpr int kMaxOutput = 0x7fff;
    static constexpr int kStep = 0x10000;
    static constexpr int kClockHz = 3072000;

    int last_reg_;
    int reg_[8];
    int volume_[4];
    int period_[4];
    int count_[4];
    int output_[4];

    uint32_t rng_;
    int noise_fb_;
    uint32_t update_step_;
    int32_t vol_table_[16];
    Mode mode_ = Mode::kTone;

    void rebuild_vol_table();
    void update_tone_period(int reg_index);
    void update_noise_period();
};

class PsgMixer {
public:
    void reset(int sample_rate_hz);
    void write_tone(uint8_t data);
    void write_noise(uint8_t data);
    void render(int16_t* out, int frames);

private:
    PsgChip tone_;
    PsgChip noise_;
    std::mutex mutex_;
};

}  // namespace ngpc
