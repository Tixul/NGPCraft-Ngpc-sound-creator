#pragma once

#include <cstdint>

namespace ngpc {

class PsgMixer;

// Minimal K1-style stream executor (host-side emulation).
// NOTE: This is a tool-side helper for MVP iteration. It is NOT a Z80 driver.
class K1StreamPlayer {
public:
    explicit K1StreamPlayer(PsgMixer* psg = nullptr);

    void set_psg(PsgMixer* psg);
    void reset();

    // Execute a single K1-like command.
    // cmd byte format: cc--oooo (channel in bits 5-6, opcode in low nibble).
    // param: high byte = B, low byte = C (matches Z80 register usage).
    void exec(uint8_t cmd, uint16_t param);

    // Convenience helpers (host-side only).
    void note_on(uint8_t channel, uint8_t note, uint8_t volume);
    void silence(uint8_t channel);
    void pitch_offset(uint8_t channel, int8_t semitone_offset);

private:
    struct ChannelState {
        uint8_t note = 0;
        uint8_t volume = 0x0f;
        int8_t pitch_offset = 0;
    };

    void write_tone(uint8_t channel, uint16_t divider, uint8_t volume);
    void write_noise(uint8_t rate, uint8_t type, uint8_t volume);

    static uint16_t note_to_divider(int note);

    PsgMixer* psg_ = nullptr;
    ChannelState ch_[4] = {};
};

}  // namespace ngpc
