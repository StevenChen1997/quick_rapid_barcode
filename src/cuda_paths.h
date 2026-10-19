#pragma once

#include <string>
#include <vector>

#include "qrb_types.h"

#ifdef USE_CUDA
bool cuda_available(std::string &err);
ModuleMatrix linear_to_matrix_cuda(const std::vector<uint8_t> &modules, int height, int quiet,
                                   std::string &err);
ModuleMatrix encode_qr_cuda(const std::string &input, std::string &err, int version = 0);
Bitmap render_bitmap_cuda(const ModuleMatrix &mat, int scale, int margin, std::string &err);
#endif
