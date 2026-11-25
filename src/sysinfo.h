// ---------------------------------------------------------------------------
// CPU / GPU / 磁盘采集
// ---------------------------------------------------------------------------
#pragma once
#include "preamble.h"

// 单个 PDH 计数器（浮点值）
class PdhCounter {
   public:
    bool Init(const wchar_t* path) {
        if (m_query) {
            return true;
        }
        if (PdhOpenQueryW(nullptr, 0, &m_query) != ERROR_SUCCESS) {
            m_query = nullptr;
            return false;
        }
        if (PdhAddEnglishCounterW(m_query, path, 0, &m_counter) != ERROR_SUCCESS &&
            PdhAddCounterW(m_query, path, 0, &m_counter) != ERROR_SUCCESS) {
            PdhCloseQuery(m_query);
            m_query = nullptr;
            m_counter = nullptr;
            return false;
        }
        PdhCollectQueryData(m_query);
        return true;
    }

    bool Query(double* value) {
        if (!m_query) {
            return false;
        }
        if (PdhCollectQueryData(m_query) != ERROR_SUCCESS) {
            return false;
        }
        PDH_FMT_COUNTERVALUE counterValue;
        DWORD type = 0;
        if (PdhGetFormattedCounterValue(m_counter, PDH_FMT_DOUBLE, &type, &counterValue) !=
            ERROR_SUCCESS) {
            return false;
        }
        if (counterValue.CStatus != ERROR_SUCCESS) {
            return false;
        }
        *value = counterValue.doubleValue;
        return true;
    }

    // 多实例计数器：返回所有实例，name 形如 "pid_1234_engtype_3D"
    bool QueryArray(std::vector<std::pair<std::wstring, double>>* values) {
        values->clear();
        if (!m_query) {
            return false;
        }
        if (PdhCollectQueryData(m_query) != ERROR_SUCCESS) {
            return false;
        }

        DWORD size = 0;
        DWORD count = 0;
        PDH_STATUS status =
            PdhGetFormattedCounterArrayW(m_counter, PDH_FMT_DOUBLE, &size, &count, nullptr);
        if (status != PDH_MORE_DATA || size == 0) {
            return false;
        }
        std::vector<BYTE> buffer(size);
        auto* items = (PDH_FMT_COUNTERVALUE_ITEM_W*)buffer.data();
        status = PdhGetFormattedCounterArrayW(m_counter, PDH_FMT_DOUBLE, &size, &count, items);
        if (status != ERROR_SUCCESS) {
            return false;
        }
        for (DWORD i = 0; i < count; i++) {
            if (items[i].FmtValue.CStatus != ERROR_SUCCESS) {
                continue;
            }
            values->emplace_back(items[i].szName ? items[i].szName : L"", items[i].FmtValue.doubleValue);
        }
        return true;
    }

    void Uninit() {
        if (m_query) {
            PdhCloseQuery(m_query);
            m_query = nullptr;
            m_counter = nullptr;
        }
    }

   private:
    PDH_HQUERY m_query = nullptr;
    PDH_HCOUNTER m_counter = nullptr;
};

class CpuCollector {
   public:
    void Init() { m_pdh.Init(L"\\Processor Information(_Total)\\% Processor Utility"); }

    int Sample() {
        // PDH 优先，失败才退回 GetSystemTimes 差值法，避免每次 tick 都跑两套计算
        double value = 0;
        if (m_pdh.Query(&value)) {
            int usage = (int)value;
            if (usage < 0) {
                usage = 0;
            }
            if (usage > 100) {
                usage = 100;
            }
            return usage;
        }
        return SampleBySystemTimes();
    }

    void Uninit() { m_pdh.Uninit(); }

   private:
    static unsigned long long FileTimeValue(const FILETIME& ft) {
        ULARGE_INTEGER value;
        value.LowPart = ft.dwLowDateTime;
        value.HighPart = ft.dwHighDateTime;
        return value.QuadPart;
    }

    int SampleBySystemTimes() {
        FILETIME idleTime, kernelTime, userTime;
        if (!GetSystemTimes(&idleTime, &kernelTime, &userTime)) {
            return m_lastByTime;
        }
        unsigned long long idle = FileTimeValue(idleTime);
        unsigned long long kernel = FileTimeValue(kernelTime);
        unsigned long long user = FileTimeValue(userTime);

        int result = m_lastByTime;
        if (m_havePrev) {
            unsigned long long dIdle = idle - m_prevIdle;
            unsigned long long dKernel = kernel - m_prevKernel;
            unsigned long long dUser = user - m_prevUser;
            unsigned long long total = dKernel + dUser;
            if (total > 0) {
                result = (int)((total - dIdle) * 100 / total);
                result = std::max(0, std::min(100, result));
            }
        }
        m_prevIdle = idle;
        m_prevKernel = kernel;
        m_prevUser = user;
        m_havePrev = true;
        m_lastByTime = result;
        return result;
    }

    PdhCounter m_pdh;
    unsigned long long m_prevIdle = 0;
    unsigned long long m_prevKernel = 0;
    unsigned long long m_prevUser = 0;
    bool m_havePrev = false;
    int m_lastByTime = 0;
};

class GpuCollector {
   public:
    void Init() { m_pdh.Init(L"\\GPU Engine(*)\\Utilization Percentage"); }

    // 按引擎类型（3D / VideoDecode / ...）分别求和，取最大值，与任务管理器一致。
    // 原来用 find_if 做线性分组是 O(n²)，这里改成哈希聚合 O(n)。
    int Sample() {
        std::vector<std::pair<std::wstring, double>> values;
        if (!m_pdh.QueryArray(&values) || values.empty()) {
            return -1;
        }
        // 复用成员缓冲，避免每个 tick 都重建 map 和重分配
        m_groups.clear();
        m_groups.reserve(values.size());
        for (const auto& item : values) {
            std::wstring name = item.first;
            size_t pos = name.rfind(L'_');
            if (pos != std::wstring::npos) {
                name = name.substr(pos + 1);
            }
            auto it = m_index.find(name);
            if (it == m_index.end()) {
                m_groups.emplace_back(name, item.second);
                m_index.emplace(name, m_groups.size() - 1);
            } else {
                m_groups[it->second].second += item.second;
            }
        }
        double best = 0;
        for (const auto& group : m_groups) {
            best = std::max(best, group.second);
        }
        m_index.clear();
        int usage = (int)(best + 0.5);
        return std::max(0, std::min(100, usage));
    }

    void Uninit() { m_pdh.Uninit(); }

   private:
    PdhCounter m_pdh;
    std::vector<std::pair<std::wstring, double>> m_groups;
    std::unordered_map<std::wstring, size_t> m_index;
};

class DiskCollector {
   public:
    void Init() { m_pdh.Init(L"\\PhysicalDisk(_Total)\\% Idle Time"); }

    int Sample() {
        double idle = 0;
        if (!m_pdh.Query(&idle)) {
            return -1;
        }
        if (idle > 100) {
            idle = 100;
        }
        if (idle < 0) {
            idle = 0;
        }
        int usage = (int)(100.0 - idle + 0.5);
        return std::max(0, std::min(100, usage));
    }

    void Uninit() { m_pdh.Uninit(); }

   private:
    PdhCounter m_pdh;
};
