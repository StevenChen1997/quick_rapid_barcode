#pragma once

#include "qrb_types.h"
#include <string>

bool read_image(const std::string &path, Bitmap &image, std::string &err);
