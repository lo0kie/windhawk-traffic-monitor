// ---------------------------------------------------------------------------
// 设置结构体与读取
// ---------------------------------------------------------------------------
#pragma once
#include "util.h"

// 鼠标按键动作枚举：三个按键（单击 / 中键 / 右键）共用同一套动作。
// 数值必须与 mod_meta.inc 里 $options 的顺序一致。
enum MouseAction {
    ACTION_NONE = 0,
    ACTION_TASKMGR = 1,
    ACTION_SHOW_DESKTOP = 2,
};

struct Settings {
    int arrange = 0;  // 0 单排, 1 双排
    std::wstring singleRow = L"↑: ${network_up}    ↓: ${network_down}";
    std::wstring topRow = L"↑: ${network_up}";
    std::wstring bottomRow = L"↓: ${network_down}";
    int align = 0;  // 0 left, 1 center, 2 right
    std::wstring fontFace = L"Microsoft YaHei UI";
    int fontSize = 14;  // 像素（em 高度），不再随 DPI 缩放
    int fontWeight = 400;  // 100-900，和 LOGFONT.lfWeight / DWRITE_FONT_WEIGHT 同一套
    int theme = 0;  // 0 auto, 1 dark, 2 light
    std::wstring textColor;
    std::wstring backColor;
    int backOpacity = 200;
    int speedUnit = 0;  // 0 auto, 1 KB, 2 MB, 3 auto-bits, 4 Kb, 5 Mb
    bool useBits = false;      // 由 speedUnit 派生：>= 3 为 true
    bool hideUnit = false;   // 隐藏所有数值的单位（KB/s、GB、% 都算单位）
    bool separateUnit = true;
    int position = 0;  // 0 auto, 1 right, 2 left, 3 far-left
    int offsetX = 0;
    int offsetY = 0;
    int rowGap = 0;  // 双排上下两行之间的留白（像素，可为负 = 拉近）
    int updateInterval = 1000;
    bool showTooltip = true;
    int singleClick = 0;  // MouseAction
    int middleClick = 0;  // MouseAction
    int rightClick = 0;   // MouseAction
};

static Settings g_settings;
static CRITICAL_SECTION g_settingsLock;
static bool g_settingsLockInit = false;
static int GetEnumSetting(PCWSTR name, const wchar_t* const* values, int count,
                          int fallback) {
    PCWSTR raw = Wh_GetStringSetting(name);
    std::wstring text = raw ? raw : L"";
    Wh_FreeStringSetting(raw);
    text = ToLower(TrimString(text));
    const wchar_t* const* found =
        std::find_if(values, values + count,
                     [&text](const wchar_t* v) { return text == v; });
    return found == values + count ? fallback : (int)(found - values);
}

static int GetIntSetting(PCWSTR name, int fallback) {
#ifdef TM_PREVIEW
    return fallback;
#else
    return Wh_GetIntSetting(name, fallback);
#endif
}

// 与 GetStringSetting 的区别：只有整串为空才回退默认值，首尾空格保留
static std::wstring GetRawStringSetting(PCWSTR name, const std::wstring& fallback) {
    PCWSTR raw = Wh_GetStringSetting(name);
    std::wstring text = raw ? raw : L"";
    Wh_FreeStringSetting(raw);
    if (text.empty()) {
        return fallback;
    }
    return text;
}

static std::wstring GetStringSetting(PCWSTR name, const std::wstring& fallback) {
    PCWSTR raw = Wh_GetStringSetting(name);
    std::wstring text = raw ? raw : L"";
    Wh_FreeStringSetting(raw);
    if (TrimString(text).empty()) {
        return fallback;
    }
    return text;
}

#ifdef TM_PREVIEW
static std::wstring IniStr(const wchar_t* key, const wchar_t* fallback, const wchar_t* path) {
    wchar_t buffer[1024] = L"";
    GetPrivateProfileStringW(L"preview", key, fallback, buffer, 1024, path);
    return buffer;
}

static void ApplyPreviewIni(Settings* s, const wchar_t* path) {
    s->arrange = _wtoi(IniStr(L"arrange", L"0", path).c_str());
    s->singleRow = IniStr(L"singlerow", s->singleRow.c_str(), path);
    s->topRow = IniStr(L"toprow", s->topRow.c_str(), path);
    s->bottomRow = IniStr(L"bottomrow", s->bottomRow.c_str(), path);
    s->align = _wtoi(IniStr(L"align", L"0", path).c_str());
    s->fontFace = IniStr(L"fontface", s->fontFace.c_str(), path);
    s->fontSize = _wtoi(IniStr(L"fontsize", L"14", path).c_str());
    s->fontWeight = _wtoi(IniStr(L"weight", L"400", path).c_str());
    s->theme = _wtoi(IniStr(L"theme", L"0", path).c_str());
    s->textColor = IniStr(L"textcolor", L"", path);
    s->backColor = IniStr(L"backcolor", L"", path);
    s->backOpacity = _wtoi(IniStr(L"backopacity", L"200", path).c_str());
    s->speedUnit = _wtoi(IniStr(L"speedunit", L"0", path).c_str());
    s->useBits = s->speedUnit >= 3;
    s->hideUnit = _wtoi(IniStr(L"hideunit", L"0", path).c_str()) != 0;
    s->separateUnit = _wtoi(IniStr(L"separateunit", L"1", path).c_str()) != 0;
    s->position = _wtoi(IniStr(L"position", L"0", path).c_str());
    s->offsetX = _wtoi(IniStr(L"offsetx", L"0", path).c_str());
    s->offsetY = _wtoi(IniStr(L"offsety", L"0", path).c_str());
    s->rowGap = _wtoi(IniStr(L"rowgap", L"0", path).c_str());
}

#endif

static void LoadSettings() {
    Settings s;

    static const wchar_t* const arranges[] = {L"horizontal", L"vertical"};
    s.arrange = GetEnumSetting(L"Arrange", arranges, 2, 0);

    // 格式串保留原样（首尾空格是格式的一部分），只有整串为空时才用默认值
    s.singleRow = GetRawStringSetting(L"SingleRow", s.singleRow);
    s.topRow = GetRawStringSetting(L"TopRow", s.topRow);
    s.bottomRow = GetRawStringSetting(L"BottomRow", s.bottomRow);

    static const wchar_t* const aligns[] = {L"left", L"center", L"right"};
    s.align = GetEnumSetting(L"Align", aligns, 3, 0);

    s.fontFace = GetStringSetting(L"FontFace", L"Microsoft YaHei UI");
    s.fontSize = std::max(6, std::min(48, (int)GetIntSetting(L"FontSize", 14)));
    static const wchar_t* const weights[] = {L"thin",   L"extralight", L"light",
                                             L"normal", L"medium",     L"semibold",
                                             L"bold",   L"extrabold",  L"black"};
    static const int weightValues[] = {100, 200, 300, 400, 500, 600, 700, 800, 900};
    s.fontWeight = weightValues[GetEnumSetting(L"FontWeight", weights, 9, 3)];

    static const wchar_t* const themes[] = {L"auto", L"dark", L"light"};
    s.theme = GetEnumSetting(L"Theme", themes, 3, 0);

    s.textColor = GetStringSetting(L"TextColor", L"");
    s.backColor = GetStringSetting(L"BackColor", L"");
    s.backOpacity = std::max(0, std::min(255, (int)GetIntSetting(L"BackOpacity", 200)));

    static const wchar_t* const units[] = {L"auto", L"kb", L"mb", L"auto-bits", L"kb-bits",
                                           L"mb-bits"};
    s.speedUnit = GetEnumSetting(L"SpeedUnit", units, 6, 0);
    s.useBits = s.speedUnit >= 3;
    s.hideUnit = GetIntSetting(L"HideUnit", 0) != 0;
    s.separateUnit = GetIntSetting(L"SeparateUnit", 1) != 0;

    static const wchar_t* const positions[] = {L"auto", L"right", L"left", L"far-left"};
    s.position = GetEnumSetting(L"Position", positions, 4, 0);

    s.offsetX = (int)GetIntSetting(L"OffsetX", 0);
    s.offsetY = (int)GetIntSetting(L"OffsetY", 0);
    s.rowGap = std::max(-200, std::min(200, (int)GetIntSetting(L"RowGap", 0)));
    s.updateInterval = std::max(200, std::min(10000, (int)GetIntSetting(L"UpdateInterval", 1000)));
    s.showTooltip = GetIntSetting(L"ShowTooltip", 1) != 0;

    static const wchar_t* const actions[] = {L"none", L"taskmgr", L"show-desktop"};
    s.singleClick = GetEnumSetting(L"SingleClick", actions, 3, 0);
    s.middleClick = GetEnumSetting(L"MiddleClick", actions, 3, 0);
    s.rightClick = GetEnumSetting(L"RightClick", actions, 3, 0);

#ifdef TM_PREVIEW
    {
        wchar_t iniPath[MAX_PATH] = L"";
        GetModuleFileNameW(nullptr, iniPath, MAX_PATH);
        wchar_t* slash = wcsrchr(iniPath, L'\\');
        if (slash) {
            *(slash + 1) = L'\0';
            wcscat(iniPath, L"preview.ini");
            if (GetFileAttributesW(iniPath) != INVALID_FILE_ATTRIBUTES) {
                ApplyPreviewIni(&s, iniPath);
            }
        }
    }
#endif

    EnterCriticalSection(&g_settingsLock);
    g_settings = s;
    LeaveCriticalSection(&g_settingsLock);
}

static Settings CopySettings() {
    EnterCriticalSection(&g_settingsLock);
    Settings s = g_settings;
    LeaveCriticalSection(&g_settingsLock);
    return s;
}
