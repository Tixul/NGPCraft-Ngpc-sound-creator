#include "ngpc/file.h"

#include <fstream>

namespace ngpc {

bool ReadBinaryFile(const std::string& path, std::vector<uint8_t>* out, std::string* error) {
    if (!out) {
        if (error) {
            *error = "Output buffer is null";
        }
        return false;
    }

    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) {
        if (error) {
            *error = "Unable to open file";
        }
        return false;
    }

    const std::streamsize size = file.tellg();
    if (size <= 0) {
        if (error) {
            *error = "Empty file";
        }
        return false;
    }

    out->resize(static_cast<size_t>(size));
    file.seekg(0, std::ios::beg);
    if (!file.read(reinterpret_cast<char*>(out->data()), size)) {
        if (error) {
            *error = "Read failed";
        }
        return false;
    }

    return true;
}

}  // namespace ngpc
