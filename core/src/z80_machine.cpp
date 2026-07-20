#include "ngpc/z80_machine.h"

#include <algorithm>
#include <cstring>

#include "ngpc/psg.h"
#include "z80_core.hpp"

namespace ngpc {

/* The bus the CPU sees. This is the ONLY console-specific part of running the
 * core -- the interpreter itself carries no memory map with it.
 *
 * ⚠️ THIS MAP IS THE TOOL'S, KEPT BYTE-FOR-BYTE AS IT WAS. It decodes four exact
 * addresses; the real console decodes four 16 KB REGIONS (0x0000..0x3FFF is RAM
 * mirrored, 0x4000..0x7FFF is all sound chip, and so on). Aligning the two is a
 * genuine improvement and a SEPARATE change: swapping the CPU and moving the
 * memory map in one go would make any regression impossible to attribute. */
struct Z80Bus {
    uint8_t* ram = nullptr;
    PsgMixer* psg = nullptr;
    uint8_t* comm_ptr = nullptr;
    uint8_t* comm_fallback = nullptr;
    z80::Z80* cpu = nullptr;

    uint8_t read8(uint16_t addr) {
        if (addr <= 0x0FFF) return ram[addr];
        if (addr == 0x8000) return comm_ptr ? *comm_ptr : *comm_fallback;
        return 0xFF;
    }

    void write8(uint16_t addr, uint8_t value) {
        if (addr <= 0x0FFF) { ram[addr] = value; return; }
        if (addr == 0x8000) {
            if (comm_ptr) *comm_ptr = value;
            else          *comm_fallback = value;
            return;
        }
        /* The two PSG ports. On silicon these are the chip's RIGHT (0x4000) and
         * LEFT (0x4001) sides and address bit A0 picks between them; this tool
         * instead runs two PsgChip objects and renders the tone channels from one
         * and the noise channel from the other. That works here only because the
         * built-in driver mirrors every command byte to BOTH ports, so the two
         * chips stay in lockstep -- see the note in THIRD_PARTY.md. Preserved
         * exactly as it was; the CPU swap does not touch it. */
        if (addr == 0x4000) { if (psg) psg->write_noise(value); return; }
        if (addr == 0x4001) { if (psg) psg->write_tone(value); return; }
    }

    uint8_t in8(uint8_t /*port*/) { return 0xFF; }   /* open bus */

    /* ⚡ AN I/O WRITE IS THE INTERRUPT ACKNOWLEDGE. The maskable line is a LEVEL,
     * not a pulse: on the console the main CPU's timer 3 holds it asserted and only
     * this write drops it (SNK, K1SoundSim § 5.2.4 -- "Releases INT request to the
     * Z80 with write access"). This is why 71 of the 73 commercial ROMs run
     * `OUT (0xFF),A` at the end of their interrupt handler. The byte is explicitly
     * invalid data, so it is NOT sound and must not reach the chip. */
    void out8(uint8_t /*port*/, uint8_t /*value*/) {
        if (cpu) cpu->int_pending = false;
    }
};

struct Z80Machine::Impl {
    uint8_t ram[0x1000] = {};
    uint8_t comm = 0;
    uint8_t* comm_ptr = nullptr;
    PsgMixer* psg = nullptr;
    z80::Z80 cpu;
    Z80Bus bus;

    Impl() {
        bus.ram = ram;
        bus.comm_fallback = &comm;
        bus.cpu = &cpu;
        rebind();
        boot();
    }

    /* The bus caches the pointers the host can move under it. */
    void rebind() {
        bus.psg = psg;
        bus.comm_ptr = comm_ptr;
    }

    void boot() {
        cpu.reset();
        /* There is no main CPU here to release it from reset, so it simply runs. */
        cpu.running = true;
    }
};

Z80Machine::Z80Machine() : impl_(new Impl()) {}
Z80Machine::~Z80Machine() = default;

void Z80Machine::reset() {
    std::memset(impl_->ram, 0, sizeof(impl_->ram));
    impl_->comm = 0;
    impl_->rebind();
    impl_->boot();
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
    std::memcpy(&impl_->ram[address], data, max_copy);
}

void Z80Machine::step_cycles(int cycles) {
    impl_->rebind();
    z80::z80_run(impl_->bus, impl_->cpu, cycles);
}

void Z80Machine::request_irq() { impl_->cpu.int_pending = true; }
void Z80Machine::request_nmi() { impl_->cpu.nmi_pending = true; }

uint8_t* Z80Machine::ram() { return impl_->ram; }
const uint8_t* Z80Machine::ram() const { return impl_->ram; }

void Z80Machine::set_psg(PsgMixer* psg) {
    impl_->psg = psg;
    impl_->rebind();
}

void Z80Machine::set_comm_ptr(uint8_t* comm) {
    impl_->comm_ptr = comm;
    impl_->rebind();
}

PsgMixer* Z80Machine::psg() { return impl_->psg; }
uint8_t* Z80Machine::comm_ptr() { return impl_->comm_ptr; }
uint8_t Z80Machine::comm_value() const { return impl_->comm; }
void Z80Machine::set_comm_value(uint8_t value) { impl_->comm = value; }

bool Z80Machine::trapped() const { return impl_->cpu.trapped; }
uint16_t Z80Machine::trap_pc() const { return impl_->cpu.trap_pc; }
uint8_t Z80Machine::trap_opcode() const { return impl_->cpu.trap_opcode; }
uint8_t Z80Machine::trap_prefix() const { return impl_->cpu.trap_prefix; }

uint16_t Z80Machine::pc() const { return impl_->cpu.pc; }
uint64_t Z80Machine::executed() const { return impl_->cpu.executed; }

}  // namespace ngpc
