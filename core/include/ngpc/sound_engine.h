#pragma once

#include <cstdint>
#include <string>

#include "ngpc/psg.h"
#include "ngpc/z80_machine.h"

namespace ngpc {

class SoundEngine {
public:
    bool init(int sample_rate_hz);
    void reset();

    bool load_z80_driver(const std::string& path, uint16_t address = 0x0000);
    bool load_z80_driver(const std::string& path, std::string* error, uint16_t address = 0x0000);

    void step_cycles(int cycles);
    void request_irq();
    void request_nmi();

    void render(int16_t* out, int frames);

    int sample_rate() const;
    PsgMixer& psg();
    Z80Machine& z80();

private:
    int sample_rate_hz_ = 0;
    PsgMixer psg_;
    Z80Machine z80_;
};

}  // namespace ngpc
