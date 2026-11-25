// ---------------------------------------------------------------------------
// 任务栏窗口线程
// ---------------------------------------------------------------------------
#pragma once
#include "widget_lifecycle.h"

void TaskbarWidget::ThreadMain() {
    // 等待任务栏就绪（explorer 启动初期可能还没创建）
    HWND taskbar = nullptr;
    for (int i = 0; i < 600; i++) {
        if (WaitForSingleObject(g_stopEvent, 0) == WAIT_OBJECT_0) {
            return;
        }
        taskbar = FindWindowW(L"Shell_TrayWnd", nullptr);
        if (taskbar) {
            break;
        }
        Sleep(100);
    }
    if (!taskbar) {
        LOG(L"Taskbar window not found, giving up");
        SetEvent(g_stopEvent);
        return;
    }

    RECT rc = {};
    GetWindowRect(taskbar, &rc);
    if ((rc.bottom - rc.top) > (rc.right - rc.left)) {
        LOG(L"Vertical taskbar is not supported");
        SetEvent(g_stopEvent);
        return;
    }

    TaskbarWidget* widget = TaskbarWidget::Instance();
    if (!widget->CreateFor(taskbar)) {
        SetEvent(g_stopEvent);
        return;
    }

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        if (msg.message == WM_QUIT) {
            break;
        }
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    widget->Destroy();
    SetEvent(g_stopEvent);
}

// CreateThread 需要裸函数指针，无捕获 lambda 转函数指针在个别编译器组合下不成立，
// 这里用显式的静态函数，避免转换问题。
static DWORD WINAPI WidgetThreadProc(LPVOID) {
    TaskbarWidget::ThreadMain();
    return 0;
}

[[maybe_unused]] static void StartWidgetThread() {
    if (g_thread) {
        return;
    }
    if (!g_stopEvent) {
        g_stopEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    }
    ResetEvent(g_stopEvent);
    g_thread = CreateThread(nullptr, 0, WidgetThreadProc, nullptr, 0, nullptr);
}

[[maybe_unused]] static void StopWidgetThread() {
    if (!g_thread) {
        return;
    }
    if (g_stopEvent) {
        SetEvent(g_stopEvent);
    }
    // 让窗口线程退出消息循环
    TaskbarWidget::Instance()->RequestClose();
    WaitForSingleObject(g_thread, 3000);
    CloseHandle(g_thread);
    g_thread = nullptr;
}
