// ---------------------------------------------------------------------------
// 任务栏窗口线程
// ---------------------------------------------------------------------------
#pragma once
#include "widget_lifecycle.h"

void TaskbarWidget::ThreadMain() {
    LOG(L"ThreadMain enter");
    TaskbarWidget* widget = TaskbarWidget::Instance();

    // explorer 重启会销毁任务栏，挂在上面的控件也会被销毁。这里不是直接退出，
    // 而是回到外层循环等新任务栏出现后重建，工具 mod 进程本身一直活着。
    for (;;) {
        if (WaitForSingleObject(g_stopEvent, 0) == WAIT_OBJECT_0) {
            return;
        }

        // 等待任务栏就绪（explorer 重启期间可能短暂没有）
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
            LOG(L"Taskbar window not found, retrying");
            continue;
        }

        RECT rc = {};
        GetWindowRect(taskbar, &rc);
        if ((rc.bottom - rc.top) > (rc.right - rc.left)) {
            LOG(L"Vertical taskbar is not supported");
            SetEvent(g_stopEvent);
            return;
        }

        LOG(L"Creating widget for taskbar=%p", taskbar);
        if (!widget->CreateFor(taskbar)) {
            LOG(L"CreateFor failed, retrying");
            Sleep(1000);
            continue;
        }

        // 清掉上一次残留的重建请求，避免重建后立刻又重建一次
        MSG pending;
        while (PeekMessageW(&pending, nullptr, WM_TM_RECREATE, WM_TM_RECREATE,
                            PM_REMOVE)) {
        }

        // 线程级看门狗：即使控件窗口被任务栏连带销毁、收不到 WM_DESTROY，
        // 这个定时器仍然留在线程队列里，能发现任务栏/窗口失效并触发重建。
        // 注意 hwnd=NULL 时系统忽略传入的 ID、自己生成 ID，必须用返回值判断。
        UINT_PTR watchdogId = SetTimer(nullptr, 0, 200, nullptr);

        bool recreate = false;
        MSG msg;
        while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
            if (msg.message == WM_QUIT) {
                KillTimer(nullptr, watchdogId);
                widget->Destroy();
                SetEvent(g_stopEvent);
                return;
            }
            if (msg.message == WM_TM_RECREATE) {
                LOG(L"WM_TM_RECREATE received");
                KillTimer(nullptr, watchdogId);
                recreate = true;
                break;
            }
            if (msg.message == WM_TIMER && msg.wParam == watchdogId) {
                if (!IsWindow(widget->hwnd()) || !IsWindow(widget->taskbar())) {
                    LOG(L"Watchdog: taskbar/widget gone");
                    KillTimer(nullptr, watchdogId);
                    recreate = true;
                    break;
                }
                continue;
            }
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }

        KillTimer(nullptr, watchdogId);
        LOG(L"Destroying widget (recreate=%d)", (int)recreate);
        widget->Destroy();
        if (!recreate) {
            break;
        }
        LOG(L"Taskbar restarted, recreating widget");
        Sleep(50);
    }
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
