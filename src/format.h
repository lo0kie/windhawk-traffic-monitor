// ---------------------------------------------------------------------------
// 数值格式化（规则同 TrafficMonitor）
// ---------------------------------------------------------------------------
#pragma once
#include "settings.h"

// 数值 + 单位：隐藏单位时只留数值，separateUnit 决定中间要不要空格
static std::wstring JoinUnitValue(const std::wstring& value, const wchar_t* unit,
                                  const Settings& s) {
    if (s.hideUnit || !unit || !*unit) {
        return value;
    }
    if (s.separateUnit) {
        return value + L' ' + unit;
    }
    return value + unit;
}

// 网速数值（不含 /s）。
// speedUnit：0 自动 / 1 固定 KB / 2 固定 MB / 3 自动比特 / 4 固定 Kb / 5 固定 Mb。
// 比特单位由 speedUnit >= 3 派生（useBits 已并入），简洁模式已移除。
static std::wstring DataSizeToString(unsigned long long size, const Settings& s) {
    wchar_t value[64] = L"";
    const wchar_t* unit = L"";

    const bool bits = s.useBits;
    if (bits) {
        size *= 8;
    }
    const int base = s.speedUnit >= 3 ? s.speedUnit - 3 : s.speedUnit;  // 归一化到 0/1/2

    switch (base) {
        case 1:  // 固定 KB / Kb
            if (size < 1024ull * 10) {
                swprintf(value, 64, L"%.2f", size / 1024.0);
            } else {
                swprintf(value, 64, L"%.1f", size / 1024.0);
            }
            unit = bits ? L"Kb" : L"KB";
            break;
        case 2:  // 固定 MB / Mb
            swprintf(value, 64, L"%.2f", size / 1024.0 / 1024.0);
            unit = bits ? L"Mb" : L"MB";
            break;
        default:  // 自动
            if (size < 1024ull * 10) {
                swprintf(value, 64, L"%.2f", size / 1024.0);
                unit = bits ? L"Kb" : L"KB";
            } else if (size < 1024ull * 1000) {
                swprintf(value, 64, L"%.1f", size / 1024.0);
                unit = bits ? L"Kb" : L"KB";
            } else if (size < 1024ull * 1024 * 1000) {
                swprintf(value, 64, L"%.2f", size / 1024.0 / 1024.0);
                unit = bits ? L"Mb" : L"MB";
            } else {
                swprintf(value, 64, L"%.2f", size / 1024.0 / 1024.0 / 1024.0);
                unit = bits ? L"Gb" : L"GB";
            }
            break;
    }

    return JoinUnitValue(value, unit, s);
}

// 容量数值（内存已用 / 总容量）
static std::wstring DataSizeWithUnit(unsigned long long size, const Settings& s) {
    wchar_t value[64] = L"";
    const wchar_t* unit = L"KB";
    if (size < 1024ull * 10) {
        swprintf(value, 64, L"%.2f", size / 1024.0);
    } else if (size < 1024ull * 1024) {
        swprintf(value, 64, L"%.1f", size / 1024.0);
    } else if (size < 1024ull * 1024 * 1024) {
        swprintf(value, 64, L"%.2f", size / 1024.0 / 1024.0);
        unit = L"MB";
    } else if (size < 1024ull * 1024 * 1024 * 1024) {
        swprintf(value, 64, L"%.2f", size / 1024.0 / 1024.0 / 1024.0);
        unit = L"GB";
    } else {
        swprintf(value, 64, L"%.2f", size / 1024.0 / 1024.0 / 1024.0 / 1024.0);
        unit = L"TB";
    }
    return JoinUnitValue(value, unit, s);
}

// 百分比数值（CPU / 内存 / GPU / 磁盘）
static std::wstring UsageToString(int usage, const Settings& s) {
    if (usage < 0) {
        return L"--";
    }
    wchar_t buf[32] = L"";
    swprintf(buf, 32, L"%d", usage);
    return JoinUnitValue(buf, L"%", s);
}
