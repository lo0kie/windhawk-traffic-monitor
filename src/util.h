// ---------------------------------------------------------------------------
// 字符串与颜色小工具
// ---------------------------------------------------------------------------
#pragma once
#include "preamble.h"

static std::wstring AnsiToWide(const char* text) {
    if (!text || !*text) {
        return L"";
    }
    int length = MultiByteToWideChar(CP_ACP, 0, text, -1, nullptr, 0);
    if (length <= 1) {
        return L"";
    }
    std::wstring result(length - 1, L'\0');
    MultiByteToWideChar(CP_ACP, 0, text, -1, &result[0], length);
    return result;
}

static std::wstring TrimString(const std::wstring& s) {
    size_t begin = s.find_first_not_of(L" \t\r\n");
    if (begin == std::wstring::npos) {
        return L"";
    }
    size_t end = s.find_last_not_of(L" \t\r\n");
    return s.substr(begin, end - begin + 1);
}

static std::wstring ToLower(std::wstring s) {
    std::transform(s.begin(), s.end(), s.begin(), [](wchar_t c) { return (wchar_t)towlower(c); });
    return s;
}

static COLORREF ParseColor(const std::wstring& text, COLORREF fallback) {
    std::wstring s = TrimString(text);
    if (s.empty()) {
        return fallback;
    }
    if (s[0] == L'#') {
        s.erase(0, 1);
    }
    if (s.size() != 6) {
        return fallback;
    }
    wchar_t* end = nullptr;
    unsigned long value = wcstoul(s.c_str(), &end, 16);
    if (end == nullptr || *end != L'\0') {
        return fallback;
    }
    return RGB((value >> 16) & 0xFF, (value >> 8) & 0xFF, value & 0xFF);
}

// 系统 UI 语言是否是简体中文 / 繁体中文。用于运行时文案（tooltip 等）的语言切换，
// 设置面板 / 元数据那套用 Windhawk 自己的 :zh-CN 后缀本地化，不经过这里。
static bool IsChineseUI() {
    LANGID lang = GetUserDefaultUILanguage();
    WORD primary = PRIMARYLANGID(lang);
    return primary == LANG_CHINESE;
}

static bool IsSystemLightTheme() {
    HKEY key;
    if (RegOpenKeyExW(HKEY_CURRENT_USER,
                      L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
                      0, KEY_READ, &key) != ERROR_SUCCESS) {
        return false;
    }
    DWORD value = 0;
    DWORD size = sizeof(value);
    DWORD type = 0;
    bool light = false;
    if (RegQueryValueExW(key, L"AppsUseLightTheme", nullptr, &type, (LPBYTE)&value,
                         &size) == ERROR_SUCCESS &&
        type == REG_DWORD) {
        light = value != 0;
    }
    RegCloseKey(key);
    return light;
}
