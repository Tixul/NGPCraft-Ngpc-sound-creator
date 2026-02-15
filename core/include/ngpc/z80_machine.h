#pragma once

#include <cstdint>
#include <cstddef>
#include <vector>

namespace ngpc {

class PsgMixer;

class Z80Machine {
public:
    Z80Machine();

    void reset();
    void load_binary(const std::vector<uint8_t>& data, uint16_t address = 0x0000);
    void load_binary(const uint8_t* data, size_t size, uint16_t address = 0x0000);

    void step_cycles(int cycles);
    void request_irq();
    void request_nmi();

    uint8_t* ram();
    const uint8_t* ram() const;

    void set_psg(PsgMixer* psg);
    void set_comm_ptr(uint8_t* comm);
    PsgMixer* psg();
    uint8_t* comm_ptr();
    uint8_t comm_value() const;
    void set_comm_value(uint8_t value);

private:
    void init_core();

    uint8_t ram_[0x1000];
    uint8_t comm_;
    uint8_t* comm_ptr_ = nullptr;
    PsgMixer* psg_;
};

}  // namespace ngpc
