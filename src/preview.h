// ---------------------------------------------------------------------------
// 独立预览入口（仅本地调试，不参与 mod 构建）
// ---------------------------------------------------------------------------
#pragma once
#include "thread.h"

#ifdef TM_PREVIEW

int PreviewMain() {
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    InitializeCriticalSection(&g_settingsLock);
    g_settingsLockInit = true;
    LoadSettings();
    StartWidgetThread();

    if (g_stopEvent) {
        WaitForSingleObject(g_stopEvent, 120000);
    }
    return 0;
}

#endif  // TM_PREVIEW
