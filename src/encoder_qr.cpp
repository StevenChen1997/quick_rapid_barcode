#include "encoder_qr.h"

#include "encoder_qr_common.h"

ModuleMatrix encode_qr(const std::string &input, std::string &err) {
    qr_internal::QrBase base;
    if (!qr_internal::build_qr_base(input, base, err)) {
        return {};
    }
    qr_internal::apply_mask_cpu(base.m, base.isFunc);
    qr_internal::apply_format_bits(base.m, base.isFunc, base.size);
    return base.m;
}
