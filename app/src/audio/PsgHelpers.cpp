#include "audio/PsgHelpers.h"

#include "ngpc/sound_engine.h"

namespace psg_helpers {

void DirectTone(ngpc::SoundEngine& engine, uint16_t divider, uint8_t attn) {
    if (divider == 0) divider = 1;
    const uint8_t b1 = static_cast<uint8_t>(0x80 | (divider & 0x0F));
    const uint8_t b2 = static_cast<uint8_t>((divider >> 4) & 0x3F);
    const uint8_t b3 = static_cast<uint8_t>(0x90 | (attn & 0x0F));
    engine.psg().write_tone(b1);
    engine.psg().write_tone(b2);
    engine.psg().write_tone(b3);
}

void DirectToneCh(ngpc::SoundEngine& engine, int ch, uint16_t divider, uint8_t attn) {
    static const uint8_t kToneBase[3] = {0x80, 0xA0, 0xC0};
    static const uint8_t kAttnBase[3] = {0x90, 0xB0, 0xD0};
    if (ch < 0 || ch > 2) return;
    if (divider == 0) divider = 1;
    const uint8_t b1 = static_cast<uint8_t>(kToneBase[ch] | (divider & 0x0F));
    const uint8_t b2 = static_cast<uint8_t>((divider >> 4) & 0x3F);
    const uint8_t b3 = static_cast<uint8_t>(kAttnBase[ch] | (attn & 0x0F));
    engine.psg().write_tone(b1);
    engine.psg().write_tone(b2);
    engine.psg().write_tone(b3);
}

void DirectNoiseMode(ngpc::SoundEngine& engine, uint8_t rate, uint8_t type) {
    const uint8_t b1 = static_cast<uint8_t>(0xE0 | ((type & 0x01) << 2) | (rate & 0x03));
    engine.psg().write_noise(b1);
}

void DirectNoiseAttn(ngpc::SoundEngine& engine, uint8_t attn) {
    const uint8_t b3 = static_cast<uint8_t>(0xF0 | (attn & 0x0F));
    engine.psg().write_noise(b3);
}

void DirectNoise(ngpc::SoundEngine& engine, uint8_t rate, uint8_t type, uint8_t attn) {
    DirectNoiseMode(engine, rate, type);
    DirectNoiseAttn(engine, attn);
}

void DirectSilenceTone(ngpc::SoundEngine& engine, int ch) {
    static const uint8_t kAttnBase[3] = {0x90, 0xB0, 0xD0};
    if (ch < 0 || ch > 2) return;
    engine.psg().write_tone(static_cast<uint8_t>(kAttnBase[ch] | 0x0F));
}

void DirectSilenceNoise(ngpc::SoundEngine& engine) {
    engine.psg().write_noise(0xFF);
}

}  // namespace psg_helpers
