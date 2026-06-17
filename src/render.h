#pragma once

#include <cstdint>
#include <vector>

#include "qrb_types.h"

ModuleMatrix linear_to_matrix(const std::vector<uint8_t> &modules, int height, int quiet);
Bitmap render_bitmap(const ModuleMatrix &mat, int scale, int margin);
void render_ascii(const ModuleMatrix &mat, int hscale, bool half_height);
