#include "file_io.h"

#include <fstream>
#include <iostream>

bool read_file(const std::string &path, std::string &data, std::size_t limit, std::string &err) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        err = "Cannot open input file: " + path;
        return false;
    }
    data.clear();
    char buffer[8192];
    while (input.read(buffer, sizeof(buffer)) || input.gcount() > 0) {
        auto count = static_cast<std::size_t>(input.gcount());
        if (count > limit - data.size()) {
            err = "Input file exceeds size limit: " + path;
            return false;
        }
        data.append(buffer, count);
    }
    if (input.bad() || !input.eof()) {
        err = "Failed to read input file: " + path;
        return false;
    }
    return true;
}

bool write_payload(const std::string &path, const std::string &data, std::string &err) {
    std::ofstream file;
    if (!path.empty()) {
        file.open(path, std::ios::binary);
        if (!file) {
            err = "Cannot open decoded output file: " + path;
            return false;
        }
    }
    std::ostream &output = path.empty() ? std::cout : file;
    output.write(data.data(), static_cast<std::streamsize>(data.size()));
    output.flush();
    bool ok = static_cast<bool>(output);
    if (file.is_open()) {
        file.close();
        ok = ok && static_cast<bool>(file);
    }
    if (!ok) {
        err = "Failed to write decoded payload";
    }
    return ok;
}
