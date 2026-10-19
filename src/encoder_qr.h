#pragma once

#include <string>

#include "qrb_types.h"

ModuleMatrix encode_qr(const std::string &input, std::string &err, int version = 0);
