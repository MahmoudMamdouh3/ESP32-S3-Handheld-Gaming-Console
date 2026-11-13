// ZEPTO-8 / Fake-08 PICO-8 UTF-8 <-> charset mapper (WTFPL2) without std::regex or <codecvt>
#include <cstdint>
#include <cstring>
#include <map>
#include <string>
#include <string_view>

#include "emojiconversion.h"

std::string_view charset::to_utf8[256];
std::u32string_view charset::to_utf32[256];

static uint8_t multibyte_start[256];
static std::map<std::string, uint8_t> to_pico8;
static char32_t utf32_storage[264];
static bool charset_initialized = false;

static char const utf8_chars[] =
    "\0¹²³⁴⁵⁶⁷⁸\t\nᵇᶜ\rᵉᶠ▮■□⁙⁘‖◀▶「」¥•、。゛゜"
    " !\"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNO"
    "PQRSTUVWXYZ[\\]^_`abcdefghijklmnopqrstuvwxyz{|}~○"
    "█▒🐱⬇️░✽●♥☉웃⌂⬅️😐♪🅾️◆…➡️★⧗⬆️ˇ∧❎▤▥あいうえおか"
    "きくけこさしすせそたちつてとなにぬねのはひふへほまみむめもやゆよ"
    "らりるれろわをんっゃゅょアイウエオカキクケコサシスセソタチツテト"
    "ナニヌネノハヒフヘホマミムメモヤユヨラリルレロワヲンッャュョ◜◝";

static size_t decode_utf8_cp(const char *s, size_t rem, char32_t *out_cp)
{
    if (rem == 0) return 0;
    uint8_t c0 = (uint8_t)s[0];
    if (c0 < 0x80u) {
        *out_cp = c0;
        return 1u;
    }
    if ((c0 & 0xE0u) == 0xC0u && rem >= 2) {
        *out_cp = ((char32_t)(c0 & 0x1Fu) << 6) | (char32_t)((uint8_t)s[1] & 0x3Fu);
        return 2u;
    }
    if ((c0 & 0xF0u) == 0xE0u && rem >= 3) {
        *out_cp = ((char32_t)(c0 & 0x0Fu) << 12) | ((char32_t)((uint8_t)s[1] & 0x3Fu) << 6) |
                  (char32_t)((uint8_t)s[2] & 0x3Fu);
        return 3u;
    }
    if ((c0 & 0xF8u) == 0xF0u && rem >= 4) {
        *out_cp = ((char32_t)(c0 & 0x07u) << 18) | ((char32_t)((uint8_t)s[1] & 0x3Fu) << 12) |
                  ((char32_t)((uint8_t)s[2] & 0x3Fu) << 6) | (char32_t)((uint8_t)s[3] & 0x3Fu);
        return 4u;
    }
    *out_cp = c0;
    return 1u;
}

void charset::ensure_init()
{
    if (charset_initialized) return;
    charset_initialized = true;

    const char *p8 = utf8_chars;
    const char *end8 = utf8_chars + sizeof(utf8_chars) - 1;
    size_t u32_idx = 0;

    for (int i = 0; i < 256; ++i) {
        if (i == 0) {
            utf32_storage[u32_idx] = 0;
            to_utf8[0] = std::string_view(p8, 1);
            to_utf32[0] = std::u32string_view(&utf32_storage[u32_idx], 1);
            u32_idx += 1;
            p8 += 1;
            continue;
        }
        char32_t cp1 = 0;
        size_t rem = (size_t)(end8 - p8);
        size_t len1 = decode_utf8_cp(p8, rem, &cp1);
        size_t total_len8 = len1;
        utf32_storage[u32_idx] = cp1;
        size_t count32 = 1;

        if (len1 < rem) {
            char32_t cp2 = 0;
            size_t len2 = decode_utf8_cp(p8 + len1, rem - len1, &cp2);
            if (cp2 == 0xFE0Fu) {
                utf32_storage[u32_idx + 1] = cp2;
                count32 = 2;
                total_len8 += len2;
            }
        }

        to_utf8[i] = std::string_view(p8, total_len8);
        to_utf32[i] = std::u32string_view(&utf32_storage[u32_idx], count32);
        to_pico8[std::string(p8, total_len8)] = (uint8_t)i;
        if (total_len8 > 1) {
            multibyte_start[(uint8_t)*p8] = 1;
        }
        u32_idx += count32;
        p8 += total_len8;
    }
}

std::string charset::utf8_to_pico8(std::string const &str)
{
    ensure_init();
    std::string ret;
    ret.reserve(str.size());

    size_t pos = 0;
    const size_t n = str.size();
    while (pos < n) {
        uint8_t lead = (uint8_t)str[pos];
        bool matched = false;
        if (multibyte_start[lead]) {
            size_t maxLen = (n - pos < 8u) ? (n - pos) : 8u;
            for (size_t l = maxLen; l >= 2u; --l) {
                auto it = to_pico8.find(str.substr(pos, l));
                if (it != to_pico8.end()) {
                    ret += (char)it->second;
                    pos += l;
                    matched = true;
                    break;
                }
            }
        }
        if (!matched) {
            ret += str[pos++];
        }
    }
    return ret;
}

std::string charset::pico8_to_utf8(std::string const &str)
{
    ensure_init();
    std::string ret;
    for (uint8_t ch : str)
        ret.append(to_utf8[ch]);
    return ret;
}

std::string charset::upper_to_emoji(std::string str)
{
    static const struct { char k; const char *v; } kMap[] = {
        {'A',"█"}, {'B',"▒"}, {'C',"🐱"}, {'D',"⬇️"}, {'E',"░"},
        {'F',"✽"}, {'G',"●"}, {'H',"♥"}, {'I',"☉"}, {'J',"웃"},
        {'K',"⌂"}, {'L',"⬅️"}, {'M',"😐"}, {'N',"♪"}, {'O',"🅾️"},
        {'P',"◆"}, {'Q',"…"}, {'R',"➡️"}, {'S',"★"}, {'T',"⧗"},
        {'U',"⬆️"}, {'V',"ˇ"}, {'W',"∧"}, {'X',"❎"}, {'Y',"▤"},
        {'Z',"▥"}
    };
    std::string newstring;
    for (unsigned char c : str) {
        const char *rep = nullptr;
        for (const auto &entry : kMap) {
            if (entry.k == (char)c) {
                rep = entry.v;
                break;
            }
        }
        if (rep) newstring += rep;
        else newstring += (char)c;
    }
    return charset::utf8_to_pico8(newstring);
}
