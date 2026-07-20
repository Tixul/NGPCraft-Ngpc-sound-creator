#pragma once

#include <cstdint>
#include <memory>

namespace ngpc {

// The T6W28, driven from the Z80's two sound ports.
//
// ⚡ ONE CHIP, TWO PORTS -- NOT TWO CHIPS. This used to run a pair of SN76489-style
// objects named `tone_` and `noise_`, rendering the squares from one and the noise
// from the other and summing to mono. That worked only because the shipped driver
// mirrors every command byte to both ports, keeping the two in lockstep; it had no
// way to express what the real chip does. The T6W28 has a RIGHT port and a LEFT
// port, and they are NOT symmetric:
//
//     write_tone()  = Z80 0x4001 = LEFT  -> the tone periods (channels 0..2)
//     write_noise() = Z80 0x4000 = RIGHT -> the noise control, and tone-3's period
//                                           reused as the noise period
//     volume writes work on BOTH ports, one stereo side each -- that IS the stereo
//
// The method names are kept for source compatibility; read them as "port 0x4001"
// and "port 0x4000". A mirroring driver still comes out correct MONO, for the same
// reason it does on silicon.
class PsgMixer {
public:
    PsgMixer();
    ~PsgMixer();

    PsgMixer(const PsgMixer&) = delete;
    PsgMixer& operator=(const PsgMixer&) = delete;

    void reset(int sample_rate_hz);
    void write_tone(uint8_t data);    // Z80 0x4001 -> LEFT port
    void write_noise(uint8_t data);   // Z80 0x4000 -> RIGHT port

    // Renders `frames` MONO samples. The chip itself is stereo; the two sides are
    // averaged here because this tool's audio path is mono end to end. With a
    // mirroring driver the two sides are equal, so nothing is lost.
    void render(int16_t* out, int frames);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace ngpc
