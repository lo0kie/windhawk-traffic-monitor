// ---------------------------------------------------------------------------
// 一次采样得到的数值
// ---------------------------------------------------------------------------
#pragma once
#include "preamble.h"

struct Snapshot {
    unsigned long long inSpeed = 0;   // B/s
    unsigned long long outSpeed = 0;  // B/s
    int cpu = 0;
    int memPercent = 0;
    unsigned long long memUsedKB = 0;
    unsigned long long memTotalKB = 0;
    int gpu = -1;
    int disk = -1;
};

struct NetRow {
    std::wstring key;
    unsigned long long in = 0;
    unsigned long long out = 0;
};
