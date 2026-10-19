#pragma once

#include "qrb_types.h"
#include <string>

ModuleMatrix encode_pdf417(const std::string &input, int columns, int ecc, std::string &err);
