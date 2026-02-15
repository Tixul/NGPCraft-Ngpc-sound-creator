#pragma once

#include <cstddef>
#include <cstdint>

namespace ngpc {

class Z80Machine;

struct PollingDriverImage {
    const uint8_t* data = nullptr;
    size_t size = 0;
};

// Built-in Z80 polling driver (multi-command buffer, 5 commands max).
PollingDriverImage BuiltinPollingDriverImage();

class PollingDriverHost {
public:
    explicit PollingDriverHost(Z80Machine* z80 = nullptr);

    void set_z80(Z80Machine* z80);
    Z80Machine* z80() const;

    bool load_builtin_driver();

    bool buffer_begin();
    bool buffer_push(uint8_t b1, uint8_t b2, uint8_t b3);
    bool buffer_commit(bool drop_if_busy = true, int spin_limit = 4000);

    bool send_bytes(uint8_t b1, uint8_t b2, uint8_t b3, bool drop_if_busy = true);
    bool play_tone(uint16_t divider, uint8_t attn, bool drop_if_busy = true);
    bool play_noise(uint8_t rate, uint8_t type, uint8_t attn, bool drop_if_busy = true);

    bool silence_tone(bool drop_if_busy = true);
    bool silence_noise(bool drop_if_busy = true);
    bool silence_all(bool drop_if_busy = true);

private:
    bool can_access() const;
    uint8_t* ram() const;

    Z80Machine* z80_ = nullptr;
    uint8_t buf_count_ = 0;
};

}  // namespace ngpc
