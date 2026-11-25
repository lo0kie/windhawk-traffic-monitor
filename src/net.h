// ---------------------------------------------------------------------------
// 网速采集
// ---------------------------------------------------------------------------
#pragma once
#include "util.h"
#include "snapshot.h"

// 网卡“友好名称”缓存：从注册表读取网络连接里显示的名字（以太网 / WLAN ...）
class AdapterAliasCache {
   public:
    const std::wstring& Get(const std::wstring& guidName, const std::wstring& description) {
        if (guidName.empty()) {
            return description;
        }
        auto it = m_map.find(guidName);
        if (it != m_map.end()) {
            return it->second;
        }
        std::wstring alias = QueryAlias(guidName);
        if (alias.empty()) {
            alias = description;
        }
        return m_map.emplace(guidName, alias).first->second;
    }

   private:
    static std::wstring QueryAlias(const std::wstring& guidName) {
        std::wstring path =
            L"SYSTEM\\CurrentControlSet\\Control\\Network\\"
            L"{4D36E972-E325-11CE-BFC1-08002BE10318}\\";
        path += guidName;
        path += L"\\Connection";

        HKEY key;
        if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, path.c_str(), 0, KEY_READ, &key) != ERROR_SUCCESS) {
            return L"";
        }
        wchar_t buffer[256] = L"";
        DWORD size = sizeof(buffer) - sizeof(wchar_t);
        DWORD type = 0;
        std::wstring result;
        if (RegQueryValueExW(key, L"Name", nullptr, &type, (LPBYTE)buffer, &size) == ERROR_SUCCESS &&
            type == REG_SZ) {
            result = buffer;
        }
        RegCloseKey(key);
        return result;
    }

    std::map<std::wstring, std::wstring> m_map;
};

class NetCollector {
   public:
    // 只统计当时流量最大的那块网卡（虚拟网卡 / Hyper-V 不会重复计数）
    void Sample() {
        ULONGLONG now = GetTickCount64();
        std::vector<NetRow> rows;

        // GetIfTable 需要的缓冲区大小会变化，按需扩容（与 TrafficMonitor 的做法一致）
        for (int attempt = 0; attempt < 4; attempt++) {
            if (m_table.size() < sizeof(MIB_IFTABLE)) {
                m_table.resize(sizeof(MIB_IFTABLE) + sizeof(MIB_IFROW) * 64);
            }
            DWORD size = (DWORD)m_table.size();
            auto* table = (MIB_IFTABLE*)m_table.data();
            DWORD result = GetIfTable(table, &size, FALSE);
            if (result == ERROR_SUCCESS) {
                for (DWORD i = 0; i < table->dwNumEntries; i++) {
                    const MIB_IFROW& row = table->table[i];
                    if (row.dwType == IF_TYPE_SOFTWARE_LOOPBACK) {
                        continue;
                    }
                    NetRow r;
                    std::wstring description = AnsiToWide((const char*)row.bDescr);
                    r.key = m_aliases.Get(row.wszName, description);
                    r.key += L'|';
                    r.key += description;
                    r.in = row.dwInOctets;
                    r.out = row.dwOutOctets;
                    rows.push_back(r);
                }
                break;
            }
            if (result != ERROR_INSUFFICIENT_BUFFER) {
                break;
            }
            m_table.resize(size + sizeof(MIB_IFROW) * 8);
        }

        if (m_prev.empty()) {
            for (const auto& row : rows) {
                m_prev.emplace(row.key, NetRow{row.key, row.in, row.out});
            }
            m_inSpeed = 0;
            m_outSpeed = 0;
            m_lastTime = now;
            return;
        }

        unsigned long long inDelta = 0;
        unsigned long long outDelta = 0;
        unsigned long long bestTotal = 0;

        // 双缓冲：复用 m_next（上一帧留下的 map），原地清空并重建，避免每个 tick
        // 都 new 一个 unordered_map + 重新分配桶。查找用 m_prev 的哈希索引 O(1)。
        m_next.clear();
        m_next.reserve(rows.size());
        for (const auto& row : rows) {
            auto it = m_prev.find(row.key);
            if (it != m_prev.end()) {
                // 32 位计数器用模运算取增量，可正确处理回绕
                unsigned long long dIn = (DWORD)((DWORD)row.in - (DWORD)it->second.in);
                unsigned long long dOut = (DWORD)((DWORD)row.out - (DWORD)it->second.out);
                if (dIn + dOut > bestTotal) {
                    bestTotal = dIn + dOut;
                    inDelta = dIn;
                    outDelta = dOut;
                }
            }
            m_next.emplace(row.key, NetRow{row.key, row.in, row.out});
        }

        unsigned long long elapsed = now - m_lastTime;
        if (elapsed < 1) {
            elapsed = 1;
        }
        m_inSpeed = inDelta * 1000 / elapsed;
        m_outSpeed = outDelta * 1000 / elapsed;
        m_lastTime = now;
        // 交换而不是移动赋值：m_next 拿到旧 prev 的桶（可复用），prev 换成新结果
        m_prev.swap(m_next);
    }

    unsigned long long inSpeed() const { return m_inSpeed; }
    unsigned long long outSpeed() const { return m_outSpeed; }
    void Reset() {
        m_prev.clear();
        m_next.clear();
        m_inSpeed = m_outSpeed = 0;
    }

   private:
    std::unordered_map<std::wstring, NetRow> m_prev;
    std::unordered_map<std::wstring, NetRow> m_next;  // 双缓冲的复用桶
    std::vector<BYTE> m_table;
    AdapterAliasCache m_aliases;
    unsigned long long m_inSpeed = 0;
    unsigned long long m_outSpeed = 0;
    ULONGLONG m_lastTime = 0;
};
