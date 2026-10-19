#include "decoder.h"
#include "input_image.h"

#include "ReadBarcode.h"

#include <exception>

bool decode_image(const std::string &path, std::string &payload, std::string &err) {
    try {
        Bitmap image;
        if (!read_image(path, image, err)) {
            return false;
        }
        ZXing::ReaderOptions options;
        options.setTryHarder(true).setTryRotate(true).setTryInvert(true).setMaxNumberOfSymbols(2);
        auto codes = ZXing::ReadBarcodes(
            ZXing::ImageView(image.pixels.data(), image.w, image.h, ZXing::ImageFormat::Lum), options);
        if (codes.empty()) {
            err = "No readable barcode found in image";
            return false;
        }
        if (codes.size() != 1) {
            err = "Multiple barcodes found; crop the image to the barcode you want to decode";
            return false;
        }
        if (!codes.front().isValid()) {
            err = "Barcode failed validation";
            return false;
        }
        const auto &bytes = codes.front().bytes();
        payload.assign(bytes.begin(), bytes.end());
        return true;
    } catch (const std::exception &ex) {
        err = std::string("Barcode decoding failed: ") + ex.what();
        return false;
    }
}
