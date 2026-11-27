// ---------------------------------------------------------------------------
// 任务栏窗口对象声明
// ---------------------------------------------------------------------------
#pragma once
#include "settings.h"
#include "snapshot.h"
#include "net.h"
#include "sysinfo.h"
#include "textrender.h"

#define WM_TM_RELOAD (WM_APP + 1)
// 任务栏被销毁（explorer 重启）时由窗口过程投递，通知窗口线程重建控件
#define WM_TM_RECREATE (WM_APP + 2)

#define TM_WINDOW_CLASS L"WindhawkTrafficMonitor"
#define TM_TIMER_ID 1

struct LineLayout {
    RECT rect;
    std::wstring text;
};

static bool RegisterWidgetClass(HINSTANCE instance);

class TaskbarWidget {
   public:
    static TaskbarWidget* Instance();

    bool CreateFor(HWND taskbar);
    void Destroy();
    void Reload();

    static void ThreadMain();

    void RequestClose();
    HWND hwnd() const { return m_hwnd; }
    HWND taskbar() const { return m_taskbar; }

    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

   private:
    LRESULT HandleMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

    void OnTick();
    void SampleData();
    void UpdateLayout();
    void ApplyPosition(bool force);
    void Render();
    void UpdateTooltip();
    void SetupTooltip();
    void CreateFonts(int dpi);
    void EnsureBuffers(int width, int height);
    void ReleaseBuffers();
    bool EnsureTopmostChild();

    int Dpi(int value) const { return MulDiv(value, m_dpi, 96); }
    COLORREF ResolveTextColor();

    HWND m_hwnd = nullptr;
    HWND m_taskbar = nullptr;
    HWND m_notify = nullptr;
    HWND m_start = nullptr;
    HWND m_tooltip = nullptr;
    HFONT m_font = nullptr;
    DWriteText m_text;          // DirectWrite 渲染（失败时回退 GDI）
    std::wstring m_fontKey;     // 字体相关设置的指纹，用来判断是否需要重建字体
    HDC m_dc = nullptr;
    HFONT m_oldFont = nullptr;
    UINT m_dpi = 96;
    int m_width = 0;
    int m_height = 0;
    int m_lastX = -10000;
    int m_lastY = -10000;
    int m_lastW = 0;
    int m_lastH = 0;
    bool m_attached = false;
    bool m_lightTheme = false;
    ULONGLONG m_lightThemeTick = 0;
    bool m_hovered = false;      // 鼠标是否悬停（用于 hover 高亮反馈）
    bool m_hoverTracked = false;  // 是否已注册 TrackMouseEvent

    std::vector<LineLayout> m_lines;
    Settings m_settings;

    NetCollector m_net;
    CpuCollector m_cpu;
    GpuCollector m_gpu;
    DiskCollector m_disk;
    Snapshot m_data;
    Snapshot m_lastData;  // 上一帧采样，用于无变化时跳过重绘

    // 分层窗口绘制用的 DIB
    HDC m_colorDc = nullptr;
    HBITMAP m_colorBitmap = nullptr;
    void* m_colorBits = nullptr;
    HDC m_maskDc = nullptr;
    HBITMAP m_maskBitmap = nullptr;
    void* m_maskBits = nullptr;
    HBITMAP m_colorOldBitmap = nullptr;
    HBITMAP m_maskOldBitmap = nullptr;
    int m_bufferWidth = 0;
    int m_bufferHeight = 0;
    bool m_layered = false;
};

static TaskbarWidget g_widget;
static HANDLE g_thread = nullptr;
static HANDLE g_stopEvent = nullptr;

TaskbarWidget* TaskbarWidget::Instance() {
    return &g_widget;
}
