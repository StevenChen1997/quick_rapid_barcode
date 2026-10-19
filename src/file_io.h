#pragma once

#include <cstddef>
#include <string>

bool read_file(const std::string &path, std::string &data, std::size_t limit, std::string &err);
bool write_payload(const std::string &path, const std::string &data, std::string &err);
