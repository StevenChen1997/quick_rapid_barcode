#include "encoder_pdf417.h"

#include "BitMatrix.h"
#include "CharacterSet.h"
#include "Utf.h"
#include "pdf417/PDFWriter.h"

#include <exception>

ModuleMatrix encode_pdf417(const std::string &input, int columns, int ecc, std::string &err) {
    if (input.empty() || input.size() > 10000) {
        err = "PDF417 input must contain 1-10000 UTF-8 bytes; actual capacity depends on compaction/ECC";
        return {};
    }
    if (columns < 0 || columns > 30 || ecc < 0 || ecc > 8) {
        err = "PDF417 columns must be 0 (auto) or 1-30; ECC must be 0-8";
        return {};
    }
    try {
        const auto text = ZXing::FromUtf8(input);
        if (ZXing::ToUtf8(text) != input) {
            err = "PDF417 text must be valid UTF-8; invalid bytes would change the payload";
            return {};
        }
        ZXing::Pdf417::Writer writer;
        writer.setMargin(0).setErrorCorrectionLevel(ecc);
        // Plain ASCII needs no ECI marker. UTF-8 payloads explicitly identify their encoding.
        for (unsigned char c : input) {
            if (c >= 128) {
                writer.setEncoding(ZXing::CharacterSet::UTF8);
                break;
            }
        }
        if (columns) {
            writer.setDimensions(columns, columns, 3, 90);
        }
        const auto bits = writer.encode(text, 0, 0);
        ModuleMatrix mat(bits.width(), bits.height());
        for (int y = 0; y < mat.h; ++y) {
            for (int x = 0; x < mat.w; ++x) {
                mat.set(x, y, bits.get(x, y));
            }
        }
        return mat;
    } catch (const std::exception &ex) {
        err = std::string("PDF417 encoding failed: ") + ex.what();
        return {};
    }
}
