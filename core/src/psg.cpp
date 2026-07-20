#include "ngpc/psg.h"

#include <algorithm>
#include <mutex>
#include <vector>

#include "apu_core.hpp"

namespace ngpc {

struct PsgMixer::Impl {
    apu::Apu chip;
    std::mutex mutex;
    // Scratch for the stereo drain before the mono downmix. Held as a member and
    // grown on demand: render() runs in the AUDIO CALLBACK, which must not allocate
    // once it is up to size.
    std::vector<int16_t> scratch;

    Impl() { chip.reset(44100); }
};

PsgMixer::PsgMixer() : impl_(new Impl()) {}
PsgMixer::~PsgMixer() = default;

void PsgMixer::reset(int sample_rate_hz) {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    impl_->chip.reset(sample_rate_hz > 0 ? uint32_t(sample_rate_hz) : 44100u);
}

void PsgMixer::write_tone(uint8_t data) {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    impl_->chip.write_left(data);      // 0x4001 = LEFT
}

void PsgMixer::write_noise(uint8_t data) {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    impl_->chip.write_right(data);     // 0x4000 = RIGHT
}

void PsgMixer::render(int16_t* out, int frames) {
    if (!out || frames <= 0) {
        return;
    }
    std::lock_guard<std::mutex> lock(impl_->mutex);
    apu::Apu& chip = impl_->chip;

    // Advance the oscillators by exactly the chip-clocks this many samples are
    // worth, then take them. The loop tops up the odd sample lost to the
    // integer division; the guard is there so a pathological rate can never spin.
    for (int guard = 0; chip.available() < uint64_t(frames) && guard < 64; ++guard) {
        const uint64_t missing = uint64_t(frames) - chip.available();
        const uint32_t rate = chip.sample_rate_hz ? chip.sample_rate_hz : 44100u;
        uint64_t chip_clocks = (missing * apu::kApuClockHz + rate - 1) / rate;
        chip_clocks = std::min<uint64_t>(chip_clocks, apu::kApuClockHz);   // <= 1 s
        chip.tick(uint32_t(chip_clocks));
    }

    const size_t need = size_t(frames) * 2;
    if (impl_->scratch.size() < need) {
        impl_->scratch.resize(need);
    }
    const uint32_t got = chip.drain(impl_->scratch.data(), uint32_t(frames));

    for (uint32_t i = 0; i < got; ++i) {
        const int l = impl_->scratch[i * 2];
        const int r = impl_->scratch[i * 2 + 1];
        out[i] = int16_t((l + r) / 2);
    }
    for (int i = int(got); i < frames; ++i) {
        out[i] = 0;
    }
}

}  // namespace ngpc
