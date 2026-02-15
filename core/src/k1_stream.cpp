#include "ngpc/k1_stream.h"

#include <cmath>

#include "ngpc/psg.h"

namespace ngpc {

namespace {
constexpr double kPsgClockHz = 3072000.0;
constexpr double kPsgDividerBase = 32.0;

constexpr uint8_t kToneRegBase[3] = {0x80, 0xA0, 0xC0};
constexpr uint8_t kAttnRegBase[3] = {0x90, 0xB0, 0xD0};
}  // namespace

K1StreamPlayer::K1StreamPlayer(PsgMixer* psg)
    : psg_(psg) {}

void K1StreamPlayer::set_psg(PsgMixer* psg) {
    psg_ = psg;
}

void K1StreamPlayer::reset() {
    for (auto& ch : ch_) {
        ch.note = 0;
        ch.volume = 0x0f;
        ch.pitch_offset = 0;
    }
    if (!psg_) {
        return;
    }
    // Silence all tone channels and noise.
    for (int i = 0; i < 3; ++i) {
        psg_->write_tone(static_cast<uint8_t>(kAttnRegBase[i] | 0x0f));
    }
    psg_->write_noise(0xF0 | 0x0f);
}

void K1StreamPlayer::exec(uint8_t cmd, uint16_t param) {
    const uint8_t channel = static_cast<uint8_t>((cmd >> 5) & 0x03);
    const uint8_t opcode = static_cast<uint8_t>(cmd & 0x0f);
    const uint8_t b = static_cast<uint8_t>(param >> 8);
    const uint8_t c = static_cast<uint8_t>(param & 0xff);

    switch (opcode) {
    case 0x01:  // reset voice
        silence(channel);
        break;
    case 0x02:  // program change (stub)
        // TODO: load instrument table (K1 size 0x11 per program).
        (void)b;
        (void)c;
        break;
    case 0x03:  // note on (volume in C)
        note_on(channel, b, c & 0x0f);
        break;
    case 0x04:  // pitch offset (semitone)
        pitch_offset(channel, static_cast<int8_t>(b));
        break;
    case 0x05:  // set param (stub)
    case 0x06:  // set param from table (stub)
    default:
        break;
    }
}

void K1StreamPlayer::note_on(uint8_t channel, uint8_t note, uint8_t volume) {
    if (channel >= 4) {
        return;
    }
    ch_[channel].note = note;
    ch_[channel].volume = static_cast<uint8_t>(volume & 0x0f);
    const int note_adj = static_cast<int>(note) + ch_[channel].pitch_offset;
    const uint16_t divider = note_to_divider(note_adj);

    if (!psg_) {
        return;
    }
    if (channel < 3) {
        write_tone(channel, divider, ch_[channel].volume);
    } else {
        // Noise: use note as rate (0..3) and type bit (0/1).
        const uint8_t rate = note & 0x03;
        const uint8_t type = (note >> 2) & 0x01;
        write_noise(rate, type, ch_[channel].volume);
    }
}

void K1StreamPlayer::silence(uint8_t channel) {
    if (!psg_) {
        return;
    }
    if (channel < 3) {
        psg_->write_tone(static_cast<uint8_t>(kAttnRegBase[channel] | 0x0f));
    } else {
        psg_->write_noise(0xF0 | 0x0f);
    }
}

void K1StreamPlayer::pitch_offset(uint8_t channel, int8_t semitone_offset) {
    if (channel >= 4) {
        return;
    }
    ch_[channel].pitch_offset = semitone_offset;
    note_on(channel, ch_[channel].note, ch_[channel].volume);
}

void K1StreamPlayer::write_tone(uint8_t channel, uint16_t divider, uint8_t volume) {
    if (!psg_ || channel >= 3) {
        return;
    }
    if (divider == 0) {
        divider = 1;
    }
    const uint8_t latch = static_cast<uint8_t>(kToneRegBase[channel] | (divider & 0x0f));
    const uint8_t data = static_cast<uint8_t>((divider >> 4) & 0x3f);
    const uint8_t attn = static_cast<uint8_t>(kAttnRegBase[channel] | (volume & 0x0f));
    psg_->write_tone(latch);
    psg_->write_tone(data);
    psg_->write_tone(attn);
}

void K1StreamPlayer::write_noise(uint8_t rate, uint8_t type, uint8_t volume) {
    if (!psg_) {
        return;
    }
    const uint8_t noise = static_cast<uint8_t>(0xE0 | ((type & 0x01) << 2) | (rate & 0x03));
    const uint8_t attn = static_cast<uint8_t>(0xF0 | (volume & 0x0f));
    psg_->write_noise(noise);
    psg_->write_noise(attn);
}

uint16_t K1StreamPlayer::note_to_divider(int note) {
    if (note < 0) {
        note = 0;
    }
    if (note > 127) {
        note = 127;
    }
    // MIDI note to frequency (A4=440).
    const double freq = 440.0 * std::pow(2.0, (static_cast<double>(note) - 69.0) / 12.0);
    const double div = kPsgClockHz / (kPsgDividerBase * freq);
    int n = static_cast<int>(std::lround(div));
    if (n < 1) {
        n = 1;
    } else if (n > 1023) {
        n = 1023;
    }
    return static_cast<uint16_t>(n);
}

}  // namespace ngpc
