#pragma once

#include <string>
#include <vector>

namespace ngpc {

struct InstrumentPreset {
    std::string name;
    int program = 0;
};

struct Project {
    std::string name;
    std::string midi_path;
    std::vector<InstrumentPreset> instruments;
};

}  // namespace ngpc
