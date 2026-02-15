#include "ngpc/sound_engine.h"

#include <vector>

#include "ngpc/file.h"

namespace ngpc {

bool SoundEngine::init(int sample_rate_hz) {
    if (sample_rate_hz <= 0) {
        return false;
    }
    sample_rate_hz_ = sample_rate_hz;
    psg_.reset(sample_rate_hz_);
    z80_.reset();
    z80_.set_psg(&psg_);
    return true;
}

void SoundEngine::reset() {
    psg_.reset(sample_rate_hz_ > 0 ? sample_rate_hz_ : 44100);
    z80_.reset();
    z80_.set_psg(&psg_);
}

bool SoundEngine::load_z80_driver(const std::string& path, uint16_t address) {
    return load_z80_driver(path, nullptr, address);
}

bool SoundEngine::load_z80_driver(const std::string& path, std::string* error, uint16_t address) {
    std::vector<uint8_t> data;
    std::string local_error;
    if (!ReadBinaryFile(path, &data, &local_error)) {
        if (error) {
            *error = local_error.empty() ? "Read failed" : local_error;
        }
        return false;
    }
    z80_.load_binary(data, address);
    return true;
}

void SoundEngine::step_cycles(int cycles) {
    z80_.step_cycles(cycles);
}

void SoundEngine::request_irq() {
    z80_.request_irq();
}

void SoundEngine::request_nmi() {
    z80_.request_nmi();
}

void SoundEngine::render(int16_t* out, int frames) {
    psg_.render(out, frames);
}

int SoundEngine::sample_rate() const {
    return sample_rate_hz_;
}

PsgMixer& SoundEngine::psg() {
    return psg_;
}

Z80Machine& SoundEngine::z80() {
    return z80_;
}

}  // namespace ngpc
