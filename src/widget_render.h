// ---------------------------------------------------------------------------
// 合成并提交到分层窗口
// ---------------------------------------------------------------------------
#pragma once
#include "widget_paint.h"
#include "widget_layout.h"

// 往颜色缓冲里画一个带圆角的半透明实心矩形（alpha 预乘，直接覆盖该区域）。
// 圆角用逐像素距离衰减做轻量抗锯齿，模拟任务栏按钮 hover 的圆角高亮。
static void FillRoundedRect(BYTE* dst, int stride, int width, int height, int x0, int y0,
                            int x1, int y1, int radius, COLORREF color, unsigned int alpha) {
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 > width) x1 = width;
    if (y1 > height) y1 = height;
    if (x0 >= x1 || y0 >= y1) {
        return;
    }
    const BYTE cb = GetBValue(color);
    const BYTE cg = GetGValue(color);
    const BYTE cr = GetRValue(color);

    for (int y = y0; y < y1; y++) {
        for (int x = x0; x < x1; x++) {
            // 圆角：判断到四个圆心的距离，落在圆角外的像素 alpha 衰减
            double a = 1.0;
            const int r = radius;
            if (x < x0 + r && y < y0 + r) {
                double dx = (x0 + r) - x - 0.5;
                double dy = (y0 + r) - y - 0.5;
                double d = r - std::sqrt(dx * dx + dy * dy);
                a = std::max(0.0, std::min(1.0, d + 0.5));
            } else if (x >= x1 - r && y < y0 + r) {
                double dx = x - (x1 - r) + 0.5;
                double dy = (y0 + r) - y - 0.5;
                double d = r - std::sqrt(dx * dx + dy * dy);
                a = std::max(0.0, std::min(1.0, d + 0.5));
            } else if (x < x0 + r && y >= y1 - r) {
                double dx = (x0 + r) - x - 0.5;
                double dy = y - (y1 - r) + 0.5;
                double d = r - std::sqrt(dx * dx + dy * dy);
                a = std::max(0.0, std::min(1.0, d + 0.5));
            } else if (x >= x1 - r && y >= y1 - r) {
                double dx = x - (x1 - r) + 0.5;
                double dy = y - (y1 - r) + 0.5;
                double d = r - std::sqrt(dx * dx + dy * dy);
                a = std::max(0.0, std::min(1.0, d + 0.5));
            }
            if (a <= 0.0) {
                continue;
            }
            const unsigned int a8 = (unsigned int)(alpha * a + 0.5);
            BYTE* p = dst + (size_t)y * stride + (size_t)x * 4;
            p[0] = (BYTE)(cb * a8 / 255);
            p[1] = (BYTE)(cg * a8 / 255);
            p[2] = (BYTE)(cr * a8 / 255);
            p[3] = (BYTE)a8;
        }
    }
}

#ifdef TM_PREVIEW
static void DumpRenderToBmp(const BYTE* bits, int width, int height, const char* path,
                            BYTE bgGray) {
    const int stride = width * 4;
    BITMAPFILEHEADER fh = {};
    BITMAPINFOHEADER ih = {};
    ih.biSize = sizeof(ih);
    ih.biWidth = width;
    ih.biHeight = -height;
    ih.biPlanes = 1;
    ih.biBitCount = 24;
    ih.biCompression = BI_RGB;
    const int outStride = ((width * 3 + 3) / 4) * 4;
    fh.bfType = 0x4D42;
    fh.bfOffBits = sizeof(fh) + sizeof(ih);
    fh.bfSize = fh.bfOffBits + outStride * height;

    FILE* f = fopen(path, "wb");
    if (!f) {
        return;
    }
    fwrite(&fh, sizeof(fh), 1, f);
    fwrite(&ih, sizeof(ih), 1, f);
    std::vector<BYTE> row(outStride, 0);
    for (int y = 0; y < height; y++) {
        const BYTE* src = bits + (size_t)y * stride;
        for (int x = 0; x < width; x++) {
            // 反预乘 + 叠加到指定灰度的背景上
            unsigned int a = src[x * 4 + 3];
            unsigned int b = src[x * 4 + 0], g = src[x * 4 + 1], r = src[x * 4 + 2];
            unsigned int inv = 255 - a;
            row[x * 3 + 0] = (BYTE)(b + bgGray * inv / 255);
            row[x * 3 + 1] = (BYTE)(g + bgGray * inv / 255);
            row[x * 3 + 2] = (BYTE)(r + bgGray * inv / 255);
        }
        fwrite(row.data(), 1, outStride, f);
    }
    fclose(f);
}
#endif

void TaskbarWidget::Render() {
    if (!m_hwnd || m_width <= 0 || m_height <= 0) {
        return;
    }

    EnsureBuffers(m_width, m_height);
    if (!m_colorDc || !m_maskDc || !m_colorBits || !m_maskBits) {
        return;
    }

    const int stride = m_width * 4;
    auto* dst = (BYTE*)m_colorBits;
    auto* mask = (BYTE*)m_maskBits;

    // 1) 背景（用户配置的背景色，覆盖整个窗口）
    memset(dst, 0, (size_t)stride * m_height);
    COLORREF backColor = ParseColor(m_settings.backColor, RGB(0, 0, 0));
    bool hasBack = !TrimString(m_settings.backColor).empty() && m_settings.backOpacity > 0;
    if (hasBack) {
        const unsigned int alpha = (unsigned int)m_settings.backOpacity;
        BYTE b = (BYTE)(GetBValue(backColor) * alpha / 255);
        BYTE g = (BYTE)(GetGValue(backColor) * alpha / 255);
        BYTE r = (BYTE)(GetRValue(backColor) * alpha / 255);
        for (int i = 0; i < m_width * m_height; i++) {
            dst[i * 4 + 0] = b;
            dst[i * 4 + 1] = g;
            dst[i * 4 + 2] = r;
            dst[i * 4 + 3] = (BYTE)alpha;
        }
    }

    // 1.5) hover 高亮：鼠标悬停且配置了任一鼠标按键动作时，画一个带圆角的半透明
    // 高亮块，贴合文字实际渲染区域。窗口宽度已经随真实文字变化，所以直接用窗口
    // 客户区（文字区域就是整个窗口）。模拟任务栏按钮的悬停反馈（深色主题用白色、
    // 浅色主题用黑色，约 8% 不透明度）。
    bool hoverable = m_settings.singleClick != ACTION_NONE ||
                     m_settings.middleClick != ACTION_NONE ||
                     m_settings.rightClick != ACTION_NONE;
    if (m_hovered && hoverable && !m_lines.empty()) {
        const int innerPad = Dpi(3);
        const int top = m_lines.front().rect.top;
        const int bottom = m_lines.back().rect.bottom;
        int hx0 = innerPad;
        int hx1 = m_width - innerPad;
        int hy0 = top - innerPad;
        int hy1 = bottom + innerPad;
        if (hx1 > hx0 && hy1 > hy0) {
            bool light = false;
            if (m_settings.theme == 1) {
                light = false;
            } else if (m_settings.theme == 2) {
                light = true;
            } else {
                light = m_lightTheme;
            }
            COLORREF hoverColor = light ? RGB(0, 0, 0) : RGB(255, 255, 255);
            FillRoundedRect(dst, stride, m_width, m_height, hx0, hy0, hx1, hy1, Dpi(4),
                            hoverColor, 20u);
        }
    }

    // 2) 文字遮罩：优先 DirectWrite（8 位覆盖率），不可用时退回 GDI（16 级）
    memset(mask, 0, (size_t)stride * m_height);
    bool maskReady = false;
    std::vector<ArrowMark> arrows;
    if (m_text.Ready()) {
        m_text.Clear();
        bool any = false;
        for (const auto& line : m_lines) {
            DrawLineRuns(m_text, line.text, line.rect, m_settings.align, &arrows, &any);
        }
        if (any) {
            GdiFlush();
            HDC writeDc = m_text.targetDc();
            if (writeDc) {
                BitBlt(m_maskDc, 0, 0, m_width, m_height, writeDc, 0, 0, SRCCOPY);
                maskReady = true;
            }
        }
    }

    if (!maskReady) {
        arrows.clear();
        HFONT oldMaskFont = (HFONT)SelectObject(m_maskDc, m_font);
        SetBkMode(m_maskDc, TRANSPARENT);
        SetTextColor(m_maskDc, RGB(255, 255, 255));

        UINT align = DT_LEFT;
        if (m_settings.align == 1) {
            align = DT_CENTER;
        } else if (m_settings.align == 2) {
            align = DT_RIGHT;
        }

        for (const auto& line : m_lines) {
            RECT rc = line.rect;
            DrawTextW(m_maskDc, line.text.c_str(), -1, &rc,
                      align | DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX);
        }
        SelectObject(m_maskDc, oldMaskFont);
    } else {
        // 箭头不进抗锯齿流程，直接按整像素写进遮罩
        for (const auto& mark : arrows) {
            DrawArrowMask(mask, m_width, m_height, mark.left, mark.centerY, ArrowSize(mark.em),
                          mark.up);
        }
    }
    GdiFlush();

    // 3) 合成文字颜色
    COLORREF textColor = ResolveTextColor();
    const BYTE* lut = AlphaLut(m_text.Ready() ? m_text.EmPixels() : 0);
    for (const auto& line : m_lines) {
        CompositeText(dst, mask, stride, line.rect.left, line.rect.top, line.rect.right,
                      line.rect.bottom, textColor, m_width, m_height, lut);
    }

#ifdef TM_PREVIEW
    {
        wchar_t dumpPath[MAX_PATH] = L"";
        if (GetEnvironmentVariableW(L"TM_DUMP", dumpPath, MAX_PATH) > 0) {
            char narrow[MAX_PATH] = "";
            WideCharToMultiByte(CP_ACP, 0, dumpPath, -1, narrow, MAX_PATH, nullptr, nullptr);
            DumpRenderToBmp(dst, m_width, m_height, narrow, 200);
        }
    }
#endif

    // 4) 把完全透明的像素（alpha==0）提成 alpha==1。分层窗口用 per-pixel alpha
    // 提交时，alpha==0 的像素会在系统层自动穿透鼠标（不经过 WM_NCHITTEST，也不管
    // WM_RBUTTONUP 是否 return 0），导致透明留白处的右键/点击落到任务栏、弹系统
    // 菜单。改成 1 后肉眼仍不可见（1/255 ≈ 0.4%），但能阻止系统穿透，让整个窗口
    // 统一拦截鼠标——这样「无操作」时也能吞掉右键，只不执行动作。
    for (int i = 0; i < m_width * m_height; i++) {
        if (dst[i * 4 + 3] == 0) {
            dst[i * 4 + 3] = 1;
        }
    }

    // 5) 提交到分层窗口
    HDC screenDc = GetDC(nullptr);
    POINT src = {0, 0};
    SIZE size = {m_width, m_height};
    BLENDFUNCTION blend = {AC_SRC_OVER, 0, 255, AC_SRC_ALPHA};
    BOOL ok = UpdateLayeredWindow(m_hwnd, screenDc, nullptr, &size, m_colorDc, &src, 0, &blend,
                                  ULW_ALPHA);
    ReleaseDC(nullptr, screenDc);

    if (!ok) {
        if (m_layered) {
            LOG(L"UpdateLayeredWindow failed: %lu", GetLastError());
            m_layered = false;
            // 退化成不透明窗口重绘
            SetWindowLongPtrW(m_hwnd, GWL_EXSTYLE,
                              GetWindowLongPtrW(m_hwnd, GWL_EXSTYLE) & ~WS_EX_LAYERED);
            InvalidateRect(m_hwnd, nullptr, TRUE);
        }
    } else {
        m_layered = true;
    }
}
