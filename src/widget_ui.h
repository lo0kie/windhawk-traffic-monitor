// ---------------------------------------------------------------------------
// 鼠标提示与定时刷新
// ---------------------------------------------------------------------------
#pragma once
#include "widget_layout.h"
#include "widget_text.h"

// 执行一个鼠标按键动作。三个按键（单击 / 中键 / 右键）共用，动作值见 MouseAction。
static void ExecuteAction(int action) {
    switch (action) {
        case ACTION_TASKMGR:
            ShellExecuteW(nullptr, L"open", L"taskmgr.exe", nullptr, nullptr, SW_SHOWNORMAL);
            break;
        case ACTION_SHOW_DESKTOP:
            // 显示桌面：用 Shell 的 ToggleDesktop（等价于 Win+D，再执行一次则恢复）。
            // GUID 手动内联定义，避免依赖 uuid.lib 链接这些 Shell COM 符号。
            {
                static const GUID kCLSID_Shell = {
                    0x13709620, 0xc279, 0x11ce, {0xa4, 0x9e, 0x44, 0x45, 0x53, 0x54, 0x00, 0x00}};
                static const GUID kIID_IShellDispatch4 = {
                    0xefd84b2d, 0x4bcf, 0x4298,
                    {0xbe, 0x25, 0xeb, 0x54, 0x2a, 0x59, 0xfb, 0xda}};
                IShellDispatch4* shell = nullptr;
                if (SUCCEEDED(CoCreateInstance(kCLSID_Shell, nullptr, CLSCTX_INPROC_SERVER,
                                               kIID_IShellDispatch4, (void**)&shell)) &&
                    shell) {
                    shell->ToggleDesktop();
                    shell->Release();
                }
            }
            break;
        default:
            break;
    }
}

void TaskbarWidget::SetupTooltip() {
    if (!m_settings.showTooltip) {
        if (m_tooltip) {
            DestroyWindow(m_tooltip);
            m_tooltip = nullptr;
        }
        return;
    }
    if (m_tooltip || !m_hwnd) {
        return;
    }

    HINSTANCE instance = nullptr;
    GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                           GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                       (LPCWSTR)&RegisterWidgetClass, &instance);

    m_tooltip = CreateWindowExW(WS_EX_TOPMOST, TOOLTIPS_CLASSW, nullptr,
                                WS_POPUP | TTS_ALWAYSTIP | TTS_NOPREFIX, CW_USEDEFAULT,
                                CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, m_hwnd, nullptr,
                                instance, nullptr);
    if (!m_tooltip) {
        LOG(L"CreateWindowExW(tooltip) failed: %lu", GetLastError());
        return;
    }
    SetWindowPos(m_tooltip, HWND_TOPMOST, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    TOOLINFOW ti = {};
    ti.cbSize = sizeof(ti);
    // 不用 TTF_SUBCLASS：那会让 tooltip 子类化本窗口、吞掉鼠标消息，
    // 导致左键 / 中键 / 右键点击收不到。改成在窗口过程里手动 relay。
    ti.uFlags = TTF_IDISHWND;
    ti.hwnd = m_hwnd;
    ti.uId = (UINT_PTR)m_hwnd;
    ti.lpszText = const_cast<LPWSTR>(L"");
    SendMessageW(m_tooltip, TTM_ADDTOOLW, 0, (LPARAM)&ti);
    SendMessageW(m_tooltip, TTM_SETMAXTIPWIDTH, 0, 600);
}

void TaskbarWidget::UpdateTooltip() {
    if (!m_tooltip) {
        return;
    }
    // 提示框是详细数据，固定带单位，不受“隐藏单位”影响
    Settings tip = m_settings;
    tip.hideUnit = false;
    tip.separateUnit = true;

    // 运行时文案按系统 UI 语言切换（设置面板用 Windhawk 自己的 :zh-CN 后缀，
    // 这里 tooltip 是 mod 在独立进程里自己拼的，得手动判断语言）
    const bool zh = IsChineseUI();

    wchar_t text[1024] = L"";
    if (zh) {
        swprintf(text, 1024,
                 L"上传: %s/s\r\n下载: %s/s\r\nCPU: %d%%\r\n内存: %d%%（%s / %s）",
                 DataSizeToString(m_data.outSpeed, tip).c_str(),
                 DataSizeToString(m_data.inSpeed, tip).c_str(), m_data.cpu,
                 m_data.memPercent,
                 DataSizeWithUnit(m_data.memUsedKB * 1024, tip).c_str(),
                 DataSizeWithUnit(m_data.memTotalKB * 1024, tip).c_str());
    } else {
        swprintf(text, 1024,
                 L"Upload: %s/s\r\nDownload: %s/s\r\nCPU: %d%%\r\nMemory: %d%% (%s / %s)",
                 DataSizeToString(m_data.outSpeed, tip).c_str(),
                 DataSizeToString(m_data.inSpeed, tip).c_str(), m_data.cpu,
                 m_data.memPercent,
                 DataSizeWithUnit(m_data.memUsedKB * 1024, tip).c_str(),
                 DataSizeWithUnit(m_data.memTotalKB * 1024, tip).c_str());
    }

    std::wstring full = text;
    if (m_data.gpu >= 0) {
        wchar_t line[64] = L"";
        swprintf(line, 64, zh ? L"\r\nGPU: %d%%" : L"\r\nGPU: %d%%", m_data.gpu);
        full += line;
    }
    if (m_data.disk >= 0) {
        wchar_t line[64] = L"";
        swprintf(line, 64, zh ? L"\r\n磁盘活动: %d%%" : L"\r\nDisk activity: %d%%", m_data.disk);
        full += line;
    }

    TOOLINFOW ti = {};
    ti.cbSize = sizeof(ti);
    ti.hwnd = m_hwnd;
    ti.uId = (UINT_PTR)m_hwnd;
    ti.lpszText = const_cast<LPWSTR>(full.c_str());
    SendMessageW(m_tooltip, TTM_UPDATETIPTEXTW, 0, (LPARAM)&ti);
}

void TaskbarWidget::OnTick() {
    if (!m_hwnd || !m_taskbar) {
        return;
    }

    // 非子窗口模式下（SetParent 失败时的 topmost 兜底），任务栏被销毁不会
    // 自动带走本窗口；发现旧任务栏句柄失效就主动走重建流程。
    if (!IsWindow(m_taskbar)) {
        PostThreadMessageW(GetCurrentThreadId(), WM_TM_RECREATE, 0, 0);
        return;
    }

    SampleData();

    // 内容没变就跳过重绘：空闲时网速、CPU、内存、GPU、磁盘常常连续多帧相同，
    // 跳过昂贵的 DirectWrite layout 重建和分层窗口提交，能省下大部分 CPU。
    if (m_data.inSpeed == m_lastData.inSpeed && m_data.outSpeed == m_lastData.outSpeed &&
        m_data.cpu == m_lastData.cpu && m_data.memPercent == m_lastData.memPercent &&
        m_data.memUsedKB == m_lastData.memUsedKB && m_data.gpu == m_lastData.gpu &&
        m_data.disk == m_lastData.disk) {
        return;
    }
    m_lastData = m_data;

    UpdateLayout();
    ApplyPosition(false);
    Render();
    UpdateTooltip();
}
