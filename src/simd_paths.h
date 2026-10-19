#pragma once

#include <string>
#include <vector>

#include "qrb_types.h"

#ifdef USE_SSE
bool sse_available(std::string &err);
ModuleMatrix linear_to_matrix_sse(const std::vector<uint8_t> &modules, int height, int quiet,
                                  std::string &err);
ModuleMatrix encode_qr_sse(const std::string &input, std::string &err, int version = 0);
Bitmap render_bitmap_sse(const ModuleMatrix &mat, int scale, int margin, std::string &err);
#endif
