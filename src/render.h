#pragma once

#include <cstdint>
#include <vector>
#include <string>

#include "qrb_types.h"

ModuleMatrix linear_to_matrix(const std::vector<uint8_t> &modules, int height, int quiet);
Bitmap render_bitmap(const ModuleMatrix &mat, int scale, int margin);
bool render_ascii(const ModuleMatrix &mat, int hscale, bool half_height,
                  const std::string &terminal_mode, std::string &err, int terminal_scale = 1);
