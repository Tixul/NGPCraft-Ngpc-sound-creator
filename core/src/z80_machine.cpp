#include "ngpc/z80_machine.h"

#include <algorithm>
#include <cstring>

#include "ngpc/psg.h"

extern "C" {
#include "Z80.h"
}

namespace {
ngpc::Z80Machine* g_machine = nullptr;
Z80 g_z80;
}

// Z80 core hooks (global C symbols expected by Z80.c)
extern "C" void WrZ80(register word Addr, register byte Value) {
    if (!g_machine) {
        return;
    }

    // Z80 memory map (sound window)
    // 0x0000..0x0FFF: shared RAM
    // 0x4000: PSG noise write
    // 0x4001: PSG tone write
    // 0x8000: comm register
    if (Addr <= 0x0FFF) {
        g_machine->ram()[Addr] = Value;
        return;
    }

    if (Addr == 0x8000) {
        if (g_machine->comm_ptr()) {
            *g_machine->comm_ptr() = Value;
        } else {
            g_machine->set_comm_value(Value);
        }
        return;
    }

    if (Addr == 0x4000) {
        if (g_machine->psg()) {
            g_machine->psg()->write_noise(Value);
        }
        return;
    }

    if (Addr == 0x4001) {
        if (g_machine->psg()) {
            g_machine->psg()->write_tone(Value);
        }
        return;
    }
}

extern "C" byte RdZ80(register word Addr) {
    if (!g_machine) {
        return 0xFF;
    }

    if (Addr <= 0x0FFF) {
        return g_machine->ram()[Addr];
    }

    if (Addr == 0x8000) {
        if (g_machine->comm_ptr()) {
            return *g_machine->comm_ptr();
        }
        return g_machine->comm_value();
    }

    return 0xFF;
}

extern "C" void OutZ80(register word Port, register byte Value) {
    (void)Port;
    (void)Value;
}

extern "C" byte InZ80(register word Port) {
    (void)Port;
    return 0xFF;
}

extern "C" void PatchZ80(register Z80* R) {
    (void)R;
}

extern "C" word LoopZ80(register Z80* R) {
    (void)R;
    return INT_NONE;
}

namespace ngpc {

Z80Machine::Z80Machine()
    : comm_(0), psg_(nullptr) {
    std::memset(ram_, 0, sizeof(ram_));
    init_core();
}

void Z80Machine::init_core() {
    g_machine = this;
    ResetZ80(&g_z80);
    g_z80.IPeriod = 1000;
    g_z80.ICount = 0;
}

void Z80Machine::reset() {
    std::memset(ram_, 0, sizeof(ram_));
    comm_ = 0;
    ResetZ80(&g_z80);
    g_z80.IPeriod = 1000;
    g_z80.ICount = 0;
}

void Z80Machine::load_binary(const std::vector<uint8_t>& data, uint16_t address) {
    load_binary(data.data(), data.size(), address);
}

void Z80Machine::load_binary(const uint8_t* data, size_t size, uint16_t address) {
    if (!data || size == 0) {
        return;
    }
    if (address >= 0x1000) {
        return;
    }
    const size_t max_copy = std::min<size_t>(size, 0x1000 - address);
    std::memcpy(&ram_[address], data, max_copy);
}

void Z80Machine::step_cycles(int cycles) {
    g_z80.ICount = cycles;
    while (g_z80.ICount > 0) {
        ExecZ80(&g_z80);
    }
}

void Z80Machine::request_irq() {
    IntZ80(&g_z80, INT_IRQ);
}

void Z80Machine::request_nmi() {
    IntZ80(&g_z80, INT_NMI);
}

uint8_t* Z80Machine::ram() {
    return ram_;
}

const uint8_t* Z80Machine::ram() const {
    return ram_;
}

void Z80Machine::set_psg(PsgMixer* psg) {
    psg_ = psg;
}

void Z80Machine::set_comm_ptr(uint8_t* comm) {
    comm_ptr_ = comm;
}

PsgMixer* Z80Machine::psg() {
    return psg_;
}

uint8_t* Z80Machine::comm_ptr() {
    return comm_ptr_;
}

uint8_t Z80Machine::comm_value() const {
    return comm_;
}

void Z80Machine::set_comm_value(uint8_t value) {
    comm_ = value;
}

}  // namespace ngpc
