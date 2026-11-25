// ---------------------------------------------------------------------------
// 系统 / 标准库头与日志宏
// ---------------------------------------------------------------------------
#pragma once

#include <windows.h>
#include <commctrl.h>
#include <shellapi.h>
#include <shldisp.h>
#include <iphlpapi.h>
#include <pdh.h>
#include <pdhmsg.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <cwctype>
#include <map>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#ifdef TM_PREVIEW
#define LOG(...)                              \
    do {                                      \
        wprintf(__VA_ARGS__);                 \
        wprintf(L"\n");                       \
        fflush(stdout);                       \
    } while (0)
#else
#define LOG(...) Wh_Log(__VA_ARGS__)
#endif
