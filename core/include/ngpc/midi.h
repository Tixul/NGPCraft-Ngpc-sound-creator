#pragma once

#include <string>

namespace ngpc {

struct MidiInfo {
    bool valid = false;
    int tracks = 0;
    int ticks_per_beat = 0;
    int tempo_events = 0;
    int tempo_events_outside_track0 = 0;
    int normalized_ticks_per_beat = 0;
    int downscale_divisor = 1;
    std::string error;
    std::string warning;
};

MidiInfo InspectMidi(const std::string& path);

}  // namespace ngpc
