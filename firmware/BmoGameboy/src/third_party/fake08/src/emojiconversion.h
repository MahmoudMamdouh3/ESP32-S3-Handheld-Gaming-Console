#pragma once

#include <string>
#include <string_view>

struct charset
{
    static std::string utf8_to_pico8(std::string const &str);
    static std::string pico8_to_utf8(std::string const &str);

    static std::u32string_view to_utf32[256];
    static std::string_view to_utf8[256];

    static std::string upper_to_emoji(std::string str);

private:
    static void ensure_init();
};
