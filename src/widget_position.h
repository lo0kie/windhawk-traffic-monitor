// ---------------------------------------------------------------------------
// 挂到任务栏并定位
// ---------------------------------------------------------------------------
#pragma once
#include "widget_layout.h"

void TaskbarWidget::ApplyPosition(bool force) {
    if (!m_hwnd || !m_taskbar) {
        return;
    }

    // 跨进程（工具 mod）时窗口是独立 topmost 弹窗，不会随任务栏自动隐藏。
    // 先按任务栏可见性同步自身可见性，避免任务栏隐藏（自动隐藏 / 全屏）时
    // 网速窗口还悬浮在原地。子窗口（m_attached）由父窗口自动处理，无需干预。
    if (!m_attached) {
        BOOL taskbarVisible = IsWindowVisible(m_taskbar);
        BOOL selfVisible = IsWindowVisible(m_hwnd);
        if (taskbarVisible != selfVisible) {
            ShowWindow(m_hwnd, taskbarVisible ? SW_SHOWNOACTIVATE : SW_HIDE);
        }
        if (taskbarVisible) {
            SetWindowPos(m_hwnd, HWND_TOPMOST, 0, 0, 0, 0,
                         SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
        }
        if (!taskbarVisible) {
            return;  // 任务栏隐藏时无需（也无法可靠地）计算位置
        }
    }

    RECT rcClient = {};
    GetClientRect(m_taskbar, &rcClient);
    int clientWidth = rcClient.right - rcClient.left;
    int clientHeight = rcClient.bottom - rcClient.top;
    if (clientWidth <= 0 || clientHeight <= 0) {
        return;
    }

    if (!m_notify || !IsWindow(m_notify)) {
        m_notify = FindWindowExW(m_taskbar, nullptr, L"TrayNotifyWnd", nullptr);
    }

    int gap = Dpi(2);

    int notifyLeft = clientWidth - Dpi(200);
    if (m_notify) {
        RECT rc = {};
        GetWindowRect(m_notify, &rc);
        POINT points[2] = {{rc.left, rc.top}, {rc.right, rc.bottom}};
        MapWindowPoints(nullptr, m_taskbar, points, 2);
        notifyLeft = points[0].x;
    }

    int startLeft = -1;
    if (!m_start || !IsWindow(m_start)) {
        m_start = FindWindowExW(m_taskbar, nullptr, L"Start", nullptr);
    }
    if (m_start) {
        RECT rc = {};
        GetWindowRect(m_start, &rc);
        POINT points[2] = {{rc.left, rc.top}, {rc.right, rc.bottom}};
        MapWindowPoints(nullptr, m_taskbar, points, 2);
        startLeft = points[0].x;
    }

    // 右侧的可用宽度。图标居中时，任务栏 XAML 里的图标区域右边界可以用开始按钮
    // 按左右对称估出来（偏保守），以此判断右侧是否真的放得下。
    int rightSpace = notifyLeft - gap;
    if (startLeft > 0 && startLeft > clientWidth / 4) {
        int clusterRight = std::min(notifyLeft, 2 * (clientWidth / 2) - startLeft);
        rightSpace = notifyLeft - clusterRight - gap;
    }

    bool useRight = m_settings.position != 2 && m_settings.position != 3;
    if (m_settings.position == 0) {  // auto
        useRight = rightSpace >= m_width + gap ||
                   startLeft <= gap || startLeft - m_width - gap <= gap;
    }

    int x;
    if (m_settings.position == 3) {
        // 状态栏最左侧：紧贴任务栏客户区左缘
        x = gap + m_settings.offsetX;
    } else if (useRight) {
        x = notifyLeft - m_width - gap + m_settings.offsetX;
    } else {
        x = startLeft - m_width - gap + m_settings.offsetX;
        if (x < gap) {
            x = gap + m_settings.offsetX;
        }
    }

    if (x + m_width > clientWidth) {
        x = clientWidth - m_width;
    }
    if (x < 0) {
        x = 0;
    }

    int y = (clientHeight - m_height) / 2 + m_settings.offsetY;
    if (y < 0) {
        y = 0;
    }
    if (y + m_height > clientHeight) {
        y = clientHeight - m_height;
    }

    if (!m_attached) {
        POINT origin = {0, 0};
        ClientToScreen(m_taskbar, &origin);
        x += origin.x;
        y += origin.y;
    }

    bool changed = force || x != m_lastX || y != m_lastY || m_width != m_lastW ||
                   m_height != m_lastH;
    if (changed) {
        SetWindowPos(m_hwnd, HWND_TOP, x, y, m_width, m_height,
                     SWP_NOACTIVATE | SWP_NOOWNERZORDER | SWP_SHOWWINDOW);
        m_lastX = x;
        m_lastY = y;
        m_lastW = m_width;
        m_lastH = m_height;
    } else {
        EnsureTopmostChild();
    }
}

bool TaskbarWidget::EnsureTopmostChild() {
    if (!m_hwnd || !m_taskbar || !m_attached) {
        return false;
    }
    HWND first = GetWindow(m_taskbar, GW_CHILD);
    if (first != m_hwnd) {
        SetWindowPos(m_hwnd, HWND_TOP, 0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_NOOWNERZORDER);
    }
    return true;
}
