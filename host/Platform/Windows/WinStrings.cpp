#include "WinStrings.h"

#include <windows.h>

namespace stm32_audio::host::win {

std::string toUtf8(const wchar_t* text)
{
    if (text == nullptr || *text == L'\0') {
        return {};
    }
    const int bytes = WideCharToMultiByte(CP_UTF8, 0, text, -1, nullptr, 0, nullptr, nullptr);
    if (bytes <= 1) {
        return {};
    }
    std::string out(static_cast<std::size_t>(bytes), '\0');
    WideCharToMultiByte(CP_UTF8, 0, text, -1, out.data(), bytes, nullptr, nullptr);
    out.resize(static_cast<std::size_t>(bytes - 1));  // drop the terminator
    return out;
}

std::wstring toWide(const std::string& utf8)
{
    if (utf8.empty()) {
        return {};
    }
    const int chars = MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()),
                                          nullptr, 0);
    if (chars <= 0) {
        return {};
    }
    std::wstring out(static_cast<std::size_t>(chars), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), out.data(), chars);
    return out;
}

}  // namespace stm32_audio::host::win
