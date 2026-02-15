#include "ngpc/polling_driver.h"

#include "ngpc/z80_machine.h"

namespace ngpc {

namespace {
constexpr uint16_t kCountOffset = 0x0003;
constexpr uint16_t kBufferOffset = 0x0004;
constexpr uint8_t kMaxCommands = 5;

// Built-in Z80 polling driver (from ngpc_sfx_tool).
// Note: data area at 0x0003..0x0012 must not overlap code, hence jp 0x0013.
constexpr uint8_t kPollingDriver[] = {
    0xC3, 0x13, 0x00,  // jp 0x0013
    0x00,              // count
    0x00, 0x00, 0x00,  // buf[0..2]
    0x00, 0x00, 0x00,  // buf[3..5]
    0x00, 0x00, 0x00,  // buf[6..8]
    0x00, 0x00, 0x00,  // buf[9..11]
    0x00, 0x00, 0x00,  // buf[12..14]
    // 0x0013:
    0xF3,              // di
    0x31, 0x00, 0x10,  // ld sp, 0x1000
    // loop (0x0017):
    0x3A, 0x03, 0x00,  // ld a, (0x0003)
    0xB7,              // or a
    0x28, 0xFA,        // jr z, loop (-6)
    0x47,              // ld b, a
    0x21, 0x04, 0x00,  // ld hl, 0x0004
    // cmd_loop (0x0021):
    0x7E,              // ld a, (hl)
    0x32, 0x01, 0x40,  // ld (0x4001), a
    0x32, 0x00, 0x40,  // ld (0x4000), a
    0x23,              // inc hl
    0x7E,              // ld a, (hl)
    0x32, 0x01, 0x40,  // ld (0x4001), a
    0x32, 0x00, 0x40,  // ld (0x4000), a
    0x23,              // inc hl
    0x7E,              // ld a, (hl)
    0x32, 0x01, 0x40,  // ld (0x4001), a
    0x32, 0x00, 0x40,  // ld (0x4000), a
    0x23,              // inc hl
    0x10, 0xE6,        // djnz cmd_loop (-26)
    0xAF,              // xor a
    0x32, 0x03, 0x00,  // ld (0x0003), a
    0x18, 0xD6         // jr loop (-42)
};
}  // namespace

PollingDriverImage BuiltinPollingDriverImage() {
    PollingDriverImage img;
    img.data = kPollingDriver;
    img.size = sizeof(kPollingDriver);
    return img;
}

PollingDriverHost::PollingDriverHost(Z80Machine* z80)
    : z80_(z80) {}

void PollingDriverHost::set_z80(Z80Machine* z80) {
    z80_ = z80;
}

Z80Machine* PollingDriverHost::z80() const {
    return z80_;
}

bool PollingDriverHost::load_builtin_driver() {
    if (!z80_) {
        return false;
    }
    const PollingDriverImage img = BuiltinPollingDriverImage();
    if (!img.data || img.size == 0) {
        return false;
    }
    z80_->load_binary(img.data, img.size, 0x0000);
    return true;
}

bool PollingDriverHost::buffer_begin() {
    if (!can_access()) {
        return false;
    }
    buf_count_ = 0;
    return true;
}

bool PollingDriverHost::buffer_push(uint8_t b1, uint8_t b2, uint8_t b3) {
    if (!can_access()) {
        return false;
    }
    if (buf_count_ >= kMaxCommands) {
        return false;
    }
    uint8_t* mem = ram();
    const uint16_t index = static_cast<uint16_t>(kBufferOffset + (buf_count_ * 3));
    mem[index + 0] = b1;
    mem[index + 1] = b2;
    mem[index + 2] = b3;
    buf_count_++;
    return true;
}

bool PollingDriverHost::buffer_commit(bool drop_if_busy, int spin_limit) {
    if (!can_access()) {
        return false;
    }
    if (buf_count_ == 0) {
        return true;
    }
    uint8_t* mem = ram();
    if (mem[kCountOffset] != 0) {
        if (drop_if_busy) {
            buf_count_ = 0;
            return false;
        }
        int spin = spin_limit;
        while (mem[kCountOffset] != 0 && spin-- > 0) {
            // spin
        }
        if (mem[kCountOffset] != 0) {
            buf_count_ = 0;
            return false;
        }
    }
    mem[kCountOffset] = buf_count_;
    buf_count_ = 0;
    return true;
}

bool PollingDriverHost::send_bytes(uint8_t b1, uint8_t b2, uint8_t b3, bool drop_if_busy) {
    if (!buffer_begin()) {
        return false;
    }
    if (!buffer_push(b1, b2, b3)) {
        return false;
    }
    return buffer_commit(drop_if_busy);
}

bool PollingDriverHost::play_tone(uint16_t divider, uint8_t attn, bool drop_if_busy) {
    if (divider == 0) {
        divider = 1;
    }
    const uint8_t b1 = static_cast<uint8_t>(0x80 | (divider & 0x0F));
    const uint8_t b2 = static_cast<uint8_t>((divider >> 4) & 0x3F);
    const uint8_t b3 = static_cast<uint8_t>(0x90 | (attn & 0x0F));
    return send_bytes(b1, b2, b3, drop_if_busy);
}

bool PollingDriverHost::play_noise(uint8_t rate, uint8_t type, uint8_t attn, bool drop_if_busy) {
    const uint8_t b1 = static_cast<uint8_t>(0xE0 | ((type & 0x01) << 2) | (rate & 0x03));
    const uint8_t b2 = 0x9F;  // keep tone1 silent
    const uint8_t b3 = static_cast<uint8_t>(0xF0 | (attn & 0x0F));
    return send_bytes(b1, b2, b3, drop_if_busy);
}

bool PollingDriverHost::silence_tone(bool drop_if_busy) {
    return send_bytes(0x9F, 0x9F, 0x9F, drop_if_busy);
}

bool PollingDriverHost::silence_noise(bool drop_if_busy) {
    return send_bytes(0xFF, 0xFF, 0xFF, drop_if_busy);
}

bool PollingDriverHost::silence_all(bool drop_if_busy) {
    if (!buffer_begin()) {
        return false;
    }
    if (!buffer_push(0x9F, 0x9F, 0x9F)) {
        return false;
    }
    if (!buffer_push(0xBF, 0xBF, 0xBF)) {
        return false;
    }
    if (!buffer_push(0xDF, 0xDF, 0xDF)) {
        return false;
    }
    if (!buffer_push(0xFF, 0xFF, 0xFF)) {
        return false;
    }
    return buffer_commit(drop_if_busy);
}

bool PollingDriverHost::can_access() const {
    return z80_ != nullptr;
}

uint8_t* PollingDriverHost::ram() const {
    return z80_ ? z80_->ram() : nullptr;
}

}  // namespace ngpc
