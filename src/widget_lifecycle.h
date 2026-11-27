// ---------------------------------------------------------------------------
// 窗口的创建 / 销毁 / 重载
// ---------------------------------------------------------------------------
#pragma once
#include "widget_message.h"
#include "widget_render.h"
#include "widget_position.h"

static bool RegisterWidgetClass(HINSTANCE instance) {
    static bool registered = false;
    if (registered) {
        return true;
    }
    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = TaskbarWidget::WndProc;
    wc.hInstance = instance;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.lpszClassName = TM_WINDOW_CLASS;
    if (!RegisterClassExW(&wc)) {
        return GetLastError() == ERROR_CLASS_ALREADY_EXISTS;
    }
    registered = true;
    return true;
}

bool TaskbarWidget::CreateFor(HWND taskbar) {
    m_taskbar = taskbar;
    m_settings = CopySettings();
    m_dpi = 96;

    HINSTANCE instance = nullptr;
    GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                           GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                       (LPCWSTR)&RegisterWidgetClass, &instance);
    if (!RegisterWidgetClass(instance)) {
        LOG(L"RegisterClassExW failed: %lu", GetLastError());
        return false;
    }

    UpdateLayout();

    m_hwnd = CreateWindowExW(
        WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE | WS_EX_LAYERED, TM_WINDOW_CLASS, L"",
        WS_POPUP | WS_CLIPSIBLINGS, 0, 0, m_width, m_height, taskbar, nullptr, instance, this);
    if (!m_hwnd) {
        LOG(L"CreateWindowExW failed: %lu", GetLastError());
        return false;
    }

    // WS_POPUP 窗口的 hWndParent 只是 owner，要显式 SetParent 才会变成任务栏的子窗口
    m_attached = SetParent(m_hwnd, taskbar) != nullptr;
    if (!m_attached) {
        LOG(L"SetParent failed: %lu, fallback to topmost popup", GetLastError());
        SetWindowLongPtrW(m_hwnd, GWL_EXSTYLE,
                          GetWindowLongPtrW(m_hwnd, GWL_EXSTYLE) | WS_EX_TOPMOST);
    }

    UpdateLayout();
    ApplyPosition(true);
    ShowWindow(m_hwnd, SW_SHOWNOACTIVATE);

    SetupTooltip();

    m_net.Reset();
    m_cpu.Init();
    m_gpu.Init();
    m_disk.Init();

    OnTick();
    SetTimer(m_hwnd, TM_TIMER_ID, (UINT)m_settings.updateInterval, nullptr);
    return true;
}

void TaskbarWidget::RequestClose() {
    if (m_hwnd) {
        PostMessageW(m_hwnd, WM_CLOSE, 0, 0);
    }
}

void TaskbarWidget::Reload() {
    if (m_hwnd) {
        PostMessageW(m_hwnd, WM_TM_RELOAD, 0, 0);
    }
}

void TaskbarWidget::Destroy() {
    KillTimer(m_hwnd, TM_TIMER_ID);
    m_cpu.Uninit();
    m_gpu.Uninit();
    m_disk.Uninit();
    m_net.Reset();
    if (m_tooltip) {
        if (IsWindow(m_tooltip)) {
            DestroyWindow(m_tooltip);
        }
        m_tooltip = nullptr;
    }
    if (m_hwnd) {
        DestroyWindow(m_hwnd);
        m_hwnd = nullptr;
    }
    if (m_font) {
        if (m_dc) {
            SelectObject(m_dc, m_oldFont ? m_oldFont : GetStockObject(DEFAULT_GUI_FONT));
        }
        DeleteObject(m_font);
        m_font = nullptr;
    }
    if (m_dc) {
        DeleteDC(m_dc);
        m_dc = nullptr;
    }
    m_text.Release();
    m_fontKey.clear();
    ReleaseBuffers();
    m_taskbar = nullptr;
    m_notify = nullptr;
    m_start = nullptr;
}
