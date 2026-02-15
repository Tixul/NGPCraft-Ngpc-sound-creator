#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace ngpc {

bool ReadBinaryFile(const std::string& path, std::vector<uint8_t>* out, std::string* error);

}  // namespace ngpc
