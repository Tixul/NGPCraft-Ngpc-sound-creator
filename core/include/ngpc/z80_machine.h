#pragma once

#include <cstdint>
#include <cstddef>
#include <memory>
#include <vector>

namespace ngpc {

class PsgMixer;

// Drives the NGPCraft Z80 core (core/third_party/ngpc_z80/z80_core.hpp) over this
// tool's sound-window memory map:
//
//     0x0000..0x0FFF  shared RAM (4 KB, where a driver image is loaded)
//     0x4000          PSG port  -> PsgMixer::write_noise
//     0x4001          PSG port  -> PsgMixer::write_tone
//     0x8000          comm register
//
// The CPU state lives in the pimpl, so several Z80Machine instances are genuinely
// independent. They used not to be: the previous core kept its registers in a
// file-scope global, and a second instance would have silently shared the first
// one's CPU.
class Z80Machine {
public:
    Z80Machine();
    ~Z80Machine();

    Z80Machine(const Z80Machine&) = delete;
    Z80Machine& operator=(const Z80Machine&) = delete;

    void reset();
    void load_binary(const std::vector<uint8_t>& data, uint16_t address = 0x0000);
    void load_binary(const uint8_t* data, size_t size, uint16_t address = 0x0000);

    // Advance by `cycles` Z80 T-states.
    void step_cycles(int cycles);

    // Assert the maskable interrupt line. It is a LEVEL: it stays asserted until
    // the guest acknowledges with an `OUT`, exactly as the console wires it. A
    // driver running under `di` (the built-in polling driver does) never sees it.
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

    // An un-ported opcode traps the CPU LOUDLY rather than being skipped: the run
    // stops and stays stopped until reset(). Ask here rather than wondering why the
    // sound died.
    bool trapped() const;
    uint16_t trap_pc() const;
    uint8_t trap_opcode() const;
    uint8_t trap_prefix() const;   // 0, or 0xCB / 0xDD / 0xED / 0xFD

    uint16_t pc() const;
    uint64_t executed() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace ngpc
