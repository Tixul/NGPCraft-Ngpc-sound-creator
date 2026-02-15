#pragma once

#include <cstdint>

namespace ngpc {
class SoundEngine;
}

namespace psg_helpers {

void DirectTone(ngpc::SoundEngine& engine, uint16_t divider, uint8_t attn);
void DirectToneCh(ngpc::SoundEngine& engine, int ch, uint16_t divider, uint8_t attn);
void DirectNoiseMode(ngpc::SoundEngine& engine, uint8_t rate, uint8_t type);
void DirectNoiseAttn(ngpc::SoundEngine& engine, uint8_t attn);
void DirectNoise(ngpc::SoundEngine& engine, uint8_t rate, uint8_t type, uint8_t attn);
void DirectSilenceTone(ngpc::SoundEngine& engine, int ch);
void DirectSilenceNoise(ngpc::SoundEngine& engine);

}  // namespace psg_helpers
