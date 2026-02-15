#include "ngpc/midi.h"

#include <cstdint>
#include <cstring>
#include <vector>

#include "ngpc/file.h"

namespace ngpc {

namespace {
bool ReadU16BE(const std::vector<uint8_t>& data, size_t pos, uint16_t* out) {
    if (pos + 1 >= data.size()) {
        return false;
    }
    *out = static_cast<uint16_t>((data[pos] << 8) | data[pos + 1]);
    return true;
}

bool ReadU32BE(const std::vector<uint8_t>& data, size_t pos, uint32_t* out) {
    if (pos + 3 >= data.size()) {
        return false;
    }
    *out = (static_cast<uint32_t>(data[pos]) << 24) |
           (static_cast<uint32_t>(data[pos + 1]) << 16) |
           (static_cast<uint32_t>(data[pos + 2]) << 8) |
           (static_cast<uint32_t>(data[pos + 3]));
    return true;
}

bool ReadVLQ(const std::vector<uint8_t>& data, size_t* pos, uint32_t* out) {
    uint32_t value = 0;
    int count = 0;
    while (*pos < data.size()) {
        const uint8_t byte = data[*pos];
        (*pos)++;
        value = (value << 7) | (byte & 0x7F);
        ++count;
        if ((byte & 0x80) == 0) {
            *out = value;
            return true;
        }
        if (count >= 4) {
            return false;
        }
    }
    return false;
}

uint32_t Gcd32(uint32_t a, uint32_t b) {
    while (b != 0) {
        const uint32_t t = a % b;
        a = b;
        b = t;
    }
    return a;
}
}  // namespace

MidiInfo InspectMidi(const std::string& path) {
    MidiInfo info;
    if (path.empty()) {
        info.error = "Empty path";
        return info;
    }

    std::vector<uint8_t> data;
    std::string error;
    if (!ReadBinaryFile(path, &data, &error)) {
        info.error = error.empty() ? "Read failed" : error;
        return info;
    }

    if (data.size() < 14) {
        info.error = "File too small for MIDI header";
        return info;
    }
    if (std::memcmp(data.data(), "MThd", 4) != 0) {
        info.error = "Missing MThd header";
        return info;
    }

    uint32_t header_len = 0;
    if (!ReadU32BE(data, 4, &header_len) || header_len < 6) {
        info.error = "Invalid MIDI header length";
        return info;
    }
    const size_t header_end = 8 + header_len;
    if (header_end > data.size()) {
        info.error = "Header length exceeds file size";
        return info;
    }

    uint16_t format = 0;
    uint16_t tracks = 0;
    uint16_t division = 0;
    if (!ReadU16BE(data, 8, &format) ||
        !ReadU16BE(data, 10, &tracks) ||
        !ReadU16BE(data, 12, &division)) {
        info.error = "Unable to read MIDI header fields";
        return info;
    }

    if (format != 1) {
        info.error = "Unsupported MIDI type (only type 1 accepted)";
        return info;
    }
    if (tracks == 0) {
        info.error = "MIDI contains no tracks";
        return info;
    }

    if (division & 0x8000) {
        info.error = "SMPTE time division not supported";
        return info;
    }

    info.tracks = tracks;
    info.ticks_per_beat = division;

    size_t pos = header_end;
    uint32_t delta_gcd = 0;
    uint8_t running_status = 0;

    for (uint16_t track_index = 0; track_index < tracks; ++track_index) {
        if (pos + 8 > data.size()) {
            info.error = "Unexpected end of file while reading track header";
            return info;
        }
        if (std::memcmp(&data[pos], "MTrk", 4) != 0) {
            info.error = "Missing MTrk header";
            return info;
        }

        uint32_t track_len = 0;
        if (!ReadU32BE(data, pos + 4, &track_len)) {
            info.error = "Unable to read track length";
            return info;
        }
        pos += 8;
        const size_t track_end = pos + track_len;
        if (track_end > data.size()) {
            info.error = "Track length exceeds file size";
            return info;
        }

        running_status = 0;
        while (pos < track_end) {
            uint32_t delta = 0;
            if (!ReadVLQ(data, &pos, &delta)) {
                info.error = "Invalid MIDI delta time";
                return info;
            }
            if (delta > 0) {
                delta_gcd = (delta_gcd == 0) ? delta : Gcd32(delta_gcd, delta);
            }

            if (pos >= track_end) {
                info.error = "Unexpected end of track data";
                return info;
            }

            uint8_t status = data[pos];
            if (status < 0x80) {
                if (running_status == 0) {
                    info.error = "Running status without prior status byte";
                    return info;
                }
                status = running_status;
            } else {
                ++pos;
                if (status < 0xF0) {
                    running_status = status;
                }
            }

            if (status == 0xFF) {
                if (pos >= track_end) {
                    info.error = "Unexpected end of meta event";
                    return info;
                }
                const uint8_t meta_type = data[pos++];
                uint32_t len = 0;
                if (!ReadVLQ(data, &pos, &len)) {
                    info.error = "Invalid meta event length";
                    return info;
                }
                if (pos + len > track_end) {
                    info.error = "Meta event exceeds track length";
                    return info;
                }
                if (meta_type == 0x51) {
                    info.tempo_events++;
                    if (track_index != 0) {
                        info.tempo_events_outside_track0++;
                    }
                }
                pos += len;
                continue;
            }

            if (status == 0xF0 || status == 0xF7) {
                uint32_t len = 0;
                if (!ReadVLQ(data, &pos, &len)) {
                    info.error = "Invalid SysEx length";
                    return info;
                }
                if (pos + len > track_end) {
                    info.error = "SysEx exceeds track length";
                    return info;
                }
                pos += len;
                continue;
            }

            const uint8_t hi = status & 0xF0;
            int data_len = 0;
            switch (hi) {
            case 0x80:
            case 0x90:
            case 0xA0:
            case 0xB0:
            case 0xE0:
                data_len = 2;
                break;
            case 0xC0:
            case 0xD0:
                data_len = 1;
                break;
            default:
                info.error = "Unknown MIDI status byte";
                return info;
            }

            if (pos + data_len > track_end) {
                info.error = "MIDI event exceeds track length";
                return info;
            }
            pos += data_len;
        }

        if (pos != track_end) {
            info.error = "Track parse did not end on track boundary";
            return info;
        }
    }

    if (info.tempo_events_outside_track0 > 0) {
        info.error = "Tempo events must be in track 1 only";
        return info;
    }

    info.normalized_ticks_per_beat = info.ticks_per_beat;
    info.downscale_divisor = 1;
    if (info.ticks_per_beat != 48) {
        if (info.ticks_per_beat % 48 != 0) {
            info.warning =
                "Division not divisible by 48; converter will quantize to 48-grid";
            info.valid = true;
            return info;
        }

        const uint32_t needed_div = static_cast<uint32_t>(info.ticks_per_beat / 48);
        if ((needed_div & (needed_div - 1)) != 0) {
            info.warning =
                "Division not reducible to 48 by powers of two; converter will quantize";
            info.valid = true;
            return info;
        }

        if (delta_gcd != 0 && (delta_gcd % needed_div) != 0) {
            info.warning =
                "Delta times not divisible enough to downscale to 48; quantization may add jitter";
            info.valid = true;
            return info;
        }

        info.normalized_ticks_per_beat = 48;
        info.downscale_divisor = static_cast<int>(needed_div);
        info.warning = "Division will be downscaled to 48";
    }

    info.valid = true;
    return info;
}

}  // namespace ngpc
