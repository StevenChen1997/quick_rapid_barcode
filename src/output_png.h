#pragma once

#include <string>

#include "qrb_types.h"

bool write_png(const std::string &path, const Bitmap &bmp, std::string &err);
