#include "encoders_1d.h"

#include <cctype>
#include <unordered_map>

static bool is_digits(const std::string &s) {
    for (char c : s) {
        if (!std::isdigit(static_cast<unsigned char>(c))) {
            return false;
        }
    }
    return true;
}

static int ean_checksum(const std::string &digits) {
    int sum = 0;
    bool odd = true;
    for (int i = static_cast<int>(digits.size()) - 1; i >= 0; --i) {
        int d = digits[static_cast<size_t>(i)] - '0';
        sum += odd ? d * 3 : d;
        odd = !odd;
    }
    int mod = sum % 10;
    return mod == 0 ? 0 : 10 - mod;
}

static void append_bits(std::vector<uint8_t> &out, const std::string &bits) {
    for (char c : bits) {
        out.push_back(c == '1' ? 1 : 0);
    }
}

static void append_widths(std::vector<uint8_t> &out, const std::string &widths) {
    bool bar = true;
    for (char c : widths) {
        int w = c - '0';
        for (int i = 0; i < w; ++i) {
            out.push_back(bar ? 1 : 0);
        }
        bar = !bar;
    }
}

std::vector<uint8_t> encode_ean13(const std::string &input, std::string &err) {
    static const char *L[10] = {
        "0001101", "0011001", "0010011", "0111101", "0100011",
        "0110001", "0101111", "0111011", "0110111", "0001011",
    };
    static const char *G[10] = {
        "0100111", "0110011", "0011011", "0100001", "0011101",
        "0111001", "0000101", "0010001", "0001001", "0010111",
    };
    static const char *R[10] = {
        "1110010", "1100110", "1101100", "1000010", "1011100",
        "1001110", "1010000", "1000100", "1001000", "1110100",
    };
    static const char *PARITY[10] = {
        "LLLLLL", "LLGLGG", "LLGGLG", "LLGGGL", "LGLLGG",
        "LGGLLG", "LGGGLL", "LGLGLG", "LGLGGL", "LGGLGL",
    };

    std::string digits = input;
    if (!is_digits(digits) || (digits.size() != 12 && digits.size() != 13)) {
        err = "EAN-13 requires 12 or 13 digits";
        return {};
    }
    if (digits.size() == 12) {
        digits.push_back(static_cast<char>('0' + ean_checksum(digits)));
    } else {
        int chk = ean_checksum(digits.substr(0, 12));
        if (chk != digits[12] - '0') {
            err = "EAN-13 checksum mismatch";
            return {};
        }
    }

    std::vector<uint8_t> out;
    append_bits(out, "101");
    int first = digits[0] - '0';
    const char *par = PARITY[first];
    for (int i = 1; i <= 6; ++i) {
        int d = digits[static_cast<size_t>(i)] - '0';
        append_bits(out, par[i - 1] == 'L' ? L[d] : G[d]);
    }
    append_bits(out, "01010");
    for (int i = 7; i <= 12; ++i) {
        int d = digits[static_cast<size_t>(i)] - '0';
        append_bits(out, R[d]);
    }
    append_bits(out, "101");
    return out;
}

std::vector<uint8_t> encode_upc(const std::string &input, std::string &err) {
    std::string digits = input;
    if (!is_digits(digits) || (digits.size() != 11 && digits.size() != 12)) {
        err = "UPC-A requires 11 or 12 digits";
        return {};
    }
    if (digits.size() == 11) {
        digits.push_back(static_cast<char>('0' + ean_checksum(digits)));
    } else {
        int chk = ean_checksum(digits.substr(0, 11));
        if (chk != digits[11] - '0') {
            err = "UPC checksum mismatch";
            return {};
        }
    }
    return encode_ean13("0" + digits, err);
}

std::vector<uint8_t> encode_code39(const std::string &input, std::string &err) {
    struct Entry {
        char ch;
        const char *pattern;
    };
    static const Entry table[] = {
        {'0', "nnnwwnwnn"}, {'1', "wnnwnnnnw"}, {'2', "nwnwnnnnw"}, {'3', "wwnwnnnnn"},
        {'4', "nnwwnnnnw"}, {'5', "wnwwnnnnn"}, {'6', "nwwwnnnnn"}, {'7', "nnnwnwnnw"},
        {'8', "wnnwnwnnn"}, {'9', "nwnwnwnnn"}, {'A', "wnnnnwnnw"}, {'B', "nwnnnwnnw"},
        {'C', "wwnnnwnnn"}, {'D', "nnnnwwnnw"}, {'E', "wnnnwwnnn"}, {'F', "nwnnwwnnn"},
        {'G', "nnnnnwwnw"}, {'H', "wnnnnwwnn"}, {'I', "nwnnnwwnn"}, {'J', "nnnnwwwnn"},
        {'K', "wnnnnnnww"}, {'L', "nwnnnnnww"}, {'M', "wwnnnnnwn"}, {'N', "nnnnwnnww"},
        {'O', "wnnnwnnwn"}, {'P', "nwnnwnnwn"}, {'Q', "nnnnnnwww"}, {'R', "wnnnnnwwn"},
        {'S', "nwnnnnwwn"}, {'T', "nnnnwnwwn"}, {'U', "wwnnnnnnw"}, {'V', "nwwnnnnnw"},
        {'W', "wwwnnnnnn"}, {'X', "nwnnwnnnw"}, {'Y', "wwnnwnnnn"}, {'Z', "nwwnwnnnn"},
        {'-', "nwnnnnwnw"}, {'.', "wwnnnnwnn"}, {' ', "nwwnnnwnn"}, {'*', "nwnnwnwnn"},
    };

    std::unordered_map<char, std::string> map;
    for (const auto &e : table) {
        map[e.ch] = e.pattern;
    }

    std::string data;
    data.reserve(input.size());
    for (char c : input) {
        char up = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
        data.push_back(up);
    }

    for (char c : data) {
        if (map.find(c) == map.end()) {
            err = "Code39 unsupported character (allowed: A-Z 0-9 space - .)";
            return {};
        }
    }

    std::string full = "*" + data + "*";
    std::vector<uint8_t> out;
    for (size_t i = 0; i < full.size(); ++i) {
        const std::string &pat = map[full[i]];
        bool bar = true;
        for (char p : pat) {
            int w = (p == 'w') ? 3 : 1;
            for (int j = 0; j < w; ++j) {
                out.push_back(bar ? 1 : 0);
            }
            bar = !bar;
        }
        if (i + 1 < full.size()) {
            out.push_back(0); // inter-character gap
        }
    }
    return out;
}

std::vector<uint8_t> encode_itf(const std::string &input, std::string &err) {
    static const char *patterns[10] = {
        "nnwwn", "wnnnw", "nwnnw", "wwnnn", "nnwnw",
        "wnwnn", "nwwnn", "nnnww", "wnnwn", "nwnwn",
    };

    std::string digits = input;
    if (!is_digits(digits) || digits.empty()) {
        err = "ITF requires digits only";
        return {};
    }
    if (digits.size() % 2 == 1) {
        digits = "0" + digits;
    }

    std::vector<uint8_t> out;
    out.push_back(1);
    out.push_back(0);
    out.push_back(1);
    out.push_back(0);

    for (size_t i = 0; i < digits.size(); i += 2) {
        int d1 = digits[i] - '0';
        int d2 = digits[i + 1] - '0';
        const char *p1 = patterns[d1];
        const char *p2 = patterns[d2];
        for (int j = 0; j < 5; ++j) {
            int w1 = (p1[j] == 'w') ? 3 : 1;
            int w2 = (p2[j] == 'w') ? 3 : 1;
            for (int k = 0; k < w1; ++k) {
                out.push_back(1);
            }
            for (int k = 0; k < w2; ++k) {
                out.push_back(0);
            }
        }
    }

    for (int k = 0; k < 3; ++k) {
        out.push_back(1);
    }
    out.push_back(0);
    out.push_back(1);

    return out;
}

std::vector<uint8_t> encode_code128(const std::string &input, std::string &err) {
    static const char *patterns[107] = {
        "212222", "222122", "222221", "121223", "121322", "131222", "122213", "122312",
        "132212", "221213", "221312", "231212", "112232", "122132", "122231", "113222",
        "123122", "123221", "223211", "221132", "221231", "213212", "223112", "312131",
        "311222", "321122", "321221", "312212", "322112", "322211", "212123", "212321",
        "232121", "111323", "131123", "131321", "112313", "132113", "132311", "211313",
        "231113", "231311", "112133", "112331", "132131", "113123", "113321", "133121",
        "313121", "211331", "231131", "213113", "213311", "213131", "311123", "311321",
        "331121", "312113", "312311", "332111", "314111", "221411", "431111", "111224",
        "111422", "121124", "121421", "141122", "141221", "112214", "112412", "122114",
        "122411", "142112", "142211", "241211", "221114", "413111", "241112", "134111",
        "111242", "121142", "121241", "114212", "124112", "124211", "411212", "421112",
        "421211", "212141", "214121", "412121", "111143", "111341", "131141", "114113",
        "114311", "411113", "411311", "113141", "114131", "311141", "411131", "211412",
        "211214", "211232", "2331112",
    };

    for (char c : input) {
        unsigned char uc = static_cast<unsigned char>(c);
        if (uc < 32 || uc > 126) {
            err = "Code128 supports ASCII 32-126 only";
            return {};
        }
    }

    auto digit_run = [&](size_t i) {
        size_t n = 0;
        while (i + n < input.size() && std::isdigit(static_cast<unsigned char>(input[i + n]))) {
            ++n;
        }
        return n;
    };

    std::vector<int> codes;
    size_t i = 0;
    bool useC = false;
    size_t run = digit_run(i);
    if (run >= 4 && run % 2 == 0) {
        useC = true;
        codes.push_back(105); // Start C
    } else {
        codes.push_back(104); // Start B
    }

    while (i < input.size()) {
        if (useC) {
            run = digit_run(i);
            if (run >= 2) {
                if (run % 2 == 1) {
                    codes.push_back(100); // Code B
                    useC = false;
                    continue;
                }
                while (run >= 2) {
                    int pair = (input[i] - '0') * 10 + (input[i + 1] - '0');
                    codes.push_back(pair);
                    i += 2;
                    run -= 2;
                }
            } else {
                codes.push_back(100); // Code B
                useC = false;
            }
        } else {
            run = digit_run(i);
            if (run >= 4) {
                if (run % 2 == 1) {
                    codes.push_back(static_cast<int>(input[i]) - 32);
                    ++i;
                } else {
                    codes.push_back(99); // Code C
                    useC = true;
                }
            } else {
                codes.push_back(static_cast<int>(input[i]) - 32);
                ++i;
            }
        }
    }

    int checksum = codes[0];
    for (size_t idx = 1; idx < codes.size(); ++idx) {
        checksum += codes[idx] * static_cast<int>(idx);
    }
    checksum %= 103;
    codes.push_back(checksum);
    codes.push_back(106);

    std::vector<uint8_t> out;
    for (int code : codes) {
        append_widths(out, patterns[code]);
    }
    return out;
}
