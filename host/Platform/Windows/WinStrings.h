// UTF-16 (Windows) <-> UTF-8 (host model) conversion.
#pragma once

#include <string>

namespace stm32_audio::host::win {

std::string toUtf8(const wchar_t* text);
std::wstring toWide(const std::string& utf8);

}  // namespace stm32_audio::host::win
