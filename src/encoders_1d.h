#pragma once

#include <cstdint>
#include <string>
#include <vector>

std::vector<uint8_t> encode_ean13(const std::string &input, std::string &err);
std::vector<uint8_t> encode_upc(const std::string &input, std::string &err);
std::vector<uint8_t> encode_code39(const std::string &input, std::string &err);
std::vector<uint8_t> encode_itf(const std::string &input, std::string &err);
std::vector<uint8_t> encode_code128(const std::string &input, std::string &err);
