// ---------------------------------------------------------------------------
// 窗口消息处理
// ---------------------------------------------------------------------------
#pragma once
#include "widget_ui.h"

LRESULT TaskbarWidget::HandleMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_TIMER:
            if (wParam == TM_TIMER_ID) {
                OnTick();
            }
            return 0;
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC dc = BeginPaint(hwnd, &ps);
            if (m_colorDc) {
                BitBlt(dc, 0, 0, m_width, m_height, m_colorDc, 0, 0, SRCCOPY);
            }
            EndPaint(hwnd, &ps);
            return 0;
        }
        case WM_ERASEBKGND:
            return 1;
        case WM_MOUSEACTIVATE:
            return MA_NOACTIVATE;
        case WM_NCHITTEST:
            // 整个窗口都拦截鼠标（插件功能优先：tooltip / hover / 点击动作都要鼠标）。
            // 不再做逐像素穿透，避免透明留白处右键/点击行为不一致。
            return HTCLIENT;
        case WM_MOUSEMOVE:
        case WM_RBUTTONDOWN:
            // 手动把鼠标消息转发给 tooltip（代替 TTF_SUBCLASS），
            // 这样 tooltip 能跟踪悬停，又不会吞掉点击消息。
            if (m_tooltip && m_settings.showTooltip) {
                MSG m = {hwnd, msg, wParam, lParam, (DWORD)GetMessageTime(),
                         {(SHORT)LOWORD(lParam), (SHORT)HIWORD(lParam)}};
                SendMessageW(m_tooltip, TTM_RELAYEVENT, 0, (LPARAM)&m);
            }
            if (msg == WM_MOUSEMOVE) {
                // 首次进入时注册 TrackMouseEvent，收到 WM_MOUSELEAVE 后清掉 hover
                if (!m_hoverTracked) {
                    TRACKMOUSEEVENT tme = {sizeof(tme), TME_LEAVE, hwnd, 0};
                    TrackMouseEvent(&tme);
                    m_hoverTracked = true;
                }
                if (!m_hovered) {
                    m_hovered = true;
                    Render();
                }
            }
            return 0;
        case WM_LBUTTONDOWN:
            // 转发给 tooltip（tooltip 需要 LBUTTONDOWN 才显示）
            if (m_tooltip && m_settings.showTooltip) {
                MSG m = {hwnd, msg, wParam, lParam, (DWORD)GetMessageTime(),
                         {(SHORT)LOWORD(lParam), (SHORT)HIWORD(lParam)}};
                SendMessageW(m_tooltip, TTM_RELAYEVENT, 0, (LPARAM)&m);
            }
            return 0;
        case WM_LBUTTONUP:
            // 转发给 tooltip；单击动作抬起来才触发（和任务栏按钮一致）。
            if (m_tooltip && m_settings.showTooltip) {
                MSG m = {hwnd, msg, wParam, lParam, (DWORD)GetMessageTime(),
                         {(SHORT)LOWORD(lParam), (SHORT)HIWORD(lParam)}};
                SendMessageW(m_tooltip, TTM_RELAYEVENT, 0, (LPARAM)&m);
            }
            if (m_settings.singleClick != ACTION_NONE) {
                ExecuteAction(m_settings.singleClick);
            }
            return 0;
        case WM_MBUTTONDOWN:
            // 中键按下也吞掉，避免走到 DefWindowProcW 触发系统行为
            return 0;
        case WM_MBUTTONUP:
            // 中键动作：默认无，配置了才触发。
            if (m_settings.middleClick != ACTION_NONE) {
                ExecuteAction(m_settings.middleClick);
            }
            return 0;
        case WM_RBUTTONUP:
            // 右键：配置了动作就执行，否则直接吞掉（整个窗口统一拦截鼠标，
            // 不再转发给任务栏，避免和任务栏自身的右键菜单状态冲突）。
            if (m_settings.rightClick != ACTION_NONE) {
                ExecuteAction(m_settings.rightClick);
            }
            return 0;
        case WM_MOUSELEAVE:
            m_hoverTracked = false;
            if (m_hovered) {
                m_hovered = false;
                Render();
            }
            return 0;
        case WM_DESTROY:
            KillTimer(hwnd, TM_TIMER_ID);
            m_hwnd = nullptr;
            PostQuitMessage(0);
            return 0;
        case WM_CLOSE:
            DestroyWindow(hwnd);
            return 0;
        case WM_TM_RELOAD: {
#ifdef TM_PREVIEW
            LoadSettings();  // 预览模式下顺带重读 preview.ini，改完设置不用重启
#endif
            Settings s = CopySettings();
            m_settings = s;
            m_net.Reset();
            SetTimer(m_hwnd, TM_TIMER_ID, (UINT)m_settings.updateInterval, nullptr);
            UpdateLayout();
            ApplyPosition(true);
            Render();
            SetupTooltip();
            UpdateTooltip();
            return 0;
        }
        case WM_DISPLAYCHANGE:
        case WM_SETTINGCHANGE:
            m_lightThemeTick = 0;
            // 分辨率 / DPI / 任务栏设置变化时立即重算，不等下一次定时器
            UpdateLayout();
            ApplyPosition(true);
            Render();
            return 0;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

LRESULT CALLBACK TaskbarWidget::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    TaskbarWidget* self = nullptr;
    if (msg == WM_NCCREATE) {
        auto* cs = (CREATESTRUCTW*)lParam;
        self = (TaskbarWidget*)cs->lpCreateParams;
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)self);
    } else {
        self = (TaskbarWidget*)GetWindowLongPtrW(hwnd, GWLP_USERDATA);
    }
    if (!self) {
        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }
    return self->HandleMessage(hwnd, msg, wParam, lParam);
}
