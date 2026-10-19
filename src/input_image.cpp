#include "input_image.h"
#include "file_io.h"

#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#define STBI_ONLY_JPEG
#define STBI_NO_STDIO
#define STBI_MAX_DIMENSIONS 16384
#include "stb_image.h"

#include <memory>

bool read_image(const std::string &path, Bitmap &image, std::string &err) {
    std::string encoded;
    if (!read_file(path, encoded, 64 * 1024 * 1024, err)) {
        return false;
    }
    const auto *bytes = reinterpret_cast<const stbi_uc *>(encoded.data());
    const int length = static_cast<int>(encoded.size());
    int width = 0, height = 0, channels = 0;
    if (!stbi_info_from_memory(bytes, length, &width, &height, &channels)) {
        err = "Cannot read image header (expected PNG or JPEG)";
        return false;
    }
    if (width <= 0 || height <= 0 || static_cast<long long>(width) * height > 16 * 1024 * 1024) {
        err = "Image exceeds the 16-megapixel decoding limit";
        return false;
    }
    std::unique_ptr<stbi_uc, decltype(&stbi_image_free)> rgba(
        stbi_load_from_memory(bytes, length, &width, &height, &channels, 4), stbi_image_free);
    if (!rgba) {
        err = "Cannot decode image (invalid or truncated PNG/JPEG)";
        return false;
    }
    image.w = width;
    image.h = height;
    image.pixels.resize(static_cast<std::size_t>(width) * height);
    for (std::size_t i = 0; i < image.pixels.size(); ++i) {
        const stbi_uc *pixel = rgba.get() + i * 4;
        int luminance = (77 * pixel[0] + 150 * pixel[1] + 29 * pixel[2] + 128) >> 8;
        // Transparent backgrounds must be composited on white, not discarded as black.
        image.pixels[i] = static_cast<uint8_t>((luminance * pixel[3] + 255 * (255 - pixel[3]) + 127) / 255);
    }
    return true;
}
