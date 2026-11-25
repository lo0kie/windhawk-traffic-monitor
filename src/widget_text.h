// ---------------------------------------------------------------------------
// 取值与格式串展开
// ---------------------------------------------------------------------------
#pragma once
#include "widget.h"
#include "format.h"

void TaskbarWidget::SampleData() {
    m_net.Sample();
    m_data.inSpeed = m_net.inSpeed();
    m_data.outSpeed = m_net.outSpeed();
    m_data.cpu = m_cpu.Sample();

    MEMORYSTATUSEX mem = {};
    mem.dwLength = sizeof(mem);
    if (GlobalMemoryStatusEx(&mem)) {
        m_data.memPercent = (int)mem.dwMemoryLoad;
        m_data.memTotalKB = mem.ullTotalPhys / 1024;
        m_data.memUsedKB = (mem.ullTotalPhys - mem.ullAvailPhys) / 1024;
    }
    m_data.gpu = m_gpu.Sample();
    m_data.disk = m_disk.Sample();
}

static std::wstring FormatSpeedText(unsigned long long bytesPerSec, const Settings& s) {
    std::wstring text = DataSizeToString(bytesPerSec, s);
    if (!s.hideUnit) {
        text += L"/s";
    }
    return text;
}

// 把 ${名字} 换成对应的数值。
// 占位符名全是 ASCII（up/down/total/cpu/mem/...），用免分配的大小写不敏感比较，
// 避免每次 tick 都 ToLower 出一个临时 wstring。
static bool NameEquals(const std::wstring& name, const wchar_t* expected) {
    size_t n = name.size();
    for (size_t i = 0; i < n; i++) {
        wchar_t c = name[i];
        if (c >= L'A' && c <= L'Z') {
            c += L'a' - L'A';
        }
        if (c != expected[i]) {
            return false;
        }
    }
    return expected[n] == L'\0';
}

static bool ResolvePlaceholder(const std::wstring& name, const Snapshot& data, const Settings& s,
                               std::wstring* out) {
    if (NameEquals(name, L"network_up")) {
        *out = FormatSpeedText(data.outSpeed, s);
    } else if (NameEquals(name, L"network_down")) {
        *out = FormatSpeedText(data.inSpeed, s);
    } else if (NameEquals(name, L"network_total")) {
        *out = FormatSpeedText(data.inSpeed + data.outSpeed, s);
    } else if (NameEquals(name, L"cpu_usage")) {
        *out = UsageToString(data.cpu, s);
    } else if (NameEquals(name, L"memory_usage")) {
        *out = UsageToString(data.memPercent, s);
    } else if (NameEquals(name, L"memory_used")) {
        *out = DataSizeWithUnit(data.memUsedKB * 1024, s);
    } else if (NameEquals(name, L"memory_available")) {
        *out = DataSizeWithUnit((data.memTotalKB - data.memUsedKB) * 1024, s);
    } else if (NameEquals(name, L"memory_total")) {
        *out = DataSizeWithUnit(data.memTotalKB * 1024, s);
    } else if (NameEquals(name, L"gpu_usage")) {
        *out = UsageToString(data.gpu, s);
    } else if (NameEquals(name, L"disk_usage")) {
        *out = UsageToString(data.disk, s);
    } else {
        return false;
    }
    return true;
}

// 展开格式串：${var} 这种写法。解析规则：
//   ${ 开头，遇到 } 结束，中间是变量名；变量名只取字母 / 数字 / 下划线。
//   不认识的名字原样输出（方便发现拼写错误）；单独的 $ 或残缺的 ${ 也原样输出。
static std::wstring ExpandFormat(const std::wstring& format, const Snapshot& data,
                                 const Settings& s) {
    std::wstring result;
    result.reserve(format.size() + 16);

    size_t i = 0;
    while (i < format.size()) {
        // 匹配 ${...}
        if (format[i] == L'$' && i + 1 < format.size() && format[i + 1] == L'{') {
            size_t nameEnd = i + 2;
            while (nameEnd < format.size() &&
                   (iswalnum(format[nameEnd]) || format[nameEnd] == L'_')) {
                nameEnd++;
            }
            if (nameEnd < format.size() && format[nameEnd] == L'}') {
                std::wstring name = format.substr(i + 2, nameEnd - i - 2);
                std::wstring value;
                if (ResolvePlaceholder(name, data, s, &value)) {
                    result += value;
                } else {
                    // 未知占位符原样输出，方便发现写错的地方
                    result += format.substr(i, nameEnd + 1 - i);
                }
                i = nameEnd + 1;
                continue;
            }
        }
        result += format[i++];
    }
    return result;
}
