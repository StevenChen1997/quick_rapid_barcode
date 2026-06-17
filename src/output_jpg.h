#pragma once

#include <string>

#include "qrb_types.h"

bool write_jpg(const std::string &path, const Bitmap &bmp, std::string &err);
