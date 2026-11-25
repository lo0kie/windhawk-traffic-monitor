// ---------------------------------------------------------------------------
// 字体与分层窗口绘制缓冲
// ---------------------------------------------------------------------------
#pragma once
#include "widget.h"

COLORREF TaskbarWidget::ResolveTextColor() {
    bool light;
    if (m_settings.theme == 1) {
        light = false;
    } else if (m_settings.theme == 2) {
        light = true;
    } else {
        ULONGLONG now = GetTickCount64();
        if (m_lightThemeTick == 0 || now - m_lightThemeTick > 5000) {
            m_lightTheme = IsSystemLightTheme();
            m_lightThemeTick = now;
        }
        light = m_lightTheme;
    }
    COLORREF fallback = light ? RGB(0, 0, 0) : RGB(255, 255, 255);
    return ParseColor(m_settings.textColor, fallback);
}

void TaskbarWidget::CreateFonts(int dpi) {
    if (m_font) {
        DeleteObject(m_font);
        m_font = nullptr;
    }
    LOGFONTW lf = {};
    // 字号现在是像素（em 高度），不是磅。GDI 的 lfHeight 是负的字符高度（像素），
    // 直接用像素值，不再做 DPI / 72 的磅换算。
    lf.lfHeight = -(LONG)m_settings.fontSize;
    lf.lfWeight = (LONG)m_settings.fontWeight;
    lf.lfCharSet = DEFAULT_CHARSET;
    lf.lfQuality = ANTIALIASED_QUALITY;
    lf.lfPitchAndFamily = DEFAULT_PITCH | FF_DONTCARE;
    wcsncpy(lf.lfFaceName, m_settings.fontFace.c_str(), LF_FACESIZE - 1);
    m_font = CreateFontIndirectW(&lf);
    if (!m_font) {
        m_font = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
    }

    // DirectWrite：字号按像素（em 高度）给，DPI 单独告诉它（内部换算成物理像素）
    m_text.SetDpi(dpi);
    m_text.SetFontPx(m_settings.fontFace, m_settings.fontSize, m_settings.fontWeight);
}

void TaskbarWidget::ReleaseBuffers() {
    if (m_colorDc && m_colorOldBitmap) {
        SelectObject(m_colorDc, m_colorOldBitmap);
        m_colorOldBitmap = nullptr;
    }
    if (m_colorBitmap) {
        DeleteObject(m_colorBitmap);
        m_colorBitmap = nullptr;
        m_colorBits = nullptr;
    }
    if (m_colorDc) {
        DeleteDC(m_colorDc);
        m_colorDc = nullptr;
    }
    if (m_maskDc && m_maskOldBitmap) {
        SelectObject(m_maskDc, m_maskOldBitmap);
        m_maskOldBitmap = nullptr;
    }
    if (m_maskBitmap) {
        DeleteObject(m_maskBitmap);
        m_maskBitmap = nullptr;
        m_maskBits = nullptr;
    }
    if (m_maskDc) {
        DeleteDC(m_maskDc);
        m_maskDc = nullptr;
    }
    m_bufferWidth = 0;
    m_bufferHeight = 0;
}

void TaskbarWidget::EnsureBuffers(int width, int height) {
    if (width <= 0 || height <= 0) {
        return;
    }
    if (m_colorDc && m_bufferWidth == width && m_bufferHeight == height) {
        return;
    }
    ReleaseBuffers();

    HDC screenDc = GetDC(nullptr);
    BITMAPINFO bi = {};
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = width;
    bi.bmiHeader.biHeight = -height;  // top-down
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;

    m_colorDc = CreateCompatibleDC(screenDc);
    m_colorBitmap = CreateDIBSection(m_colorDc, &bi, DIB_RGB_COLORS, &m_colorBits, nullptr, 0);
    m_colorOldBitmap = (HBITMAP)SelectObject(m_colorDc, m_colorBitmap);

    m_maskDc = CreateCompatibleDC(screenDc);
    m_maskBitmap = CreateDIBSection(m_maskDc, &bi, DIB_RGB_COLORS, &m_maskBits, nullptr, 0);
    m_maskOldBitmap = (HBITMAP)SelectObject(m_maskDc, m_maskBitmap);

    ReleaseDC(nullptr, screenDc);

    m_text.EnsureTarget(m_maskDc, width, height);

    m_bufferWidth = width;
    m_bufferHeight = height;
}

// 覆盖率锐化：强度按实际字号走。
// 小字号笔画细，必须把中间调往两端压才"实"；字号一大，笔画本身就有 2~3px，
// 再压只会把弧线和斜线啃成硬台阶（看着更糙），所以强度随 em 像素高度递减。
// 表里带一小段死区：覆盖率低于死区直接归零，专门去掉笔画边上那圈 1px 淡灰雾边。
static const BYTE* AlphaLut(int emPixels) {
    struct Entry {
        BYTE table[256];
        bool built;
    };
    // 0: em<=16 强（3 遍）／1: em<=24 中（2 遍）／2: 更大 轻（1 遍）
    const int bucket = emPixels <= 16 ? 0 : (emPixels <= 24 ? 1 : 2);
    static Entry entries[3];
    Entry& entry = entries[bucket];
    if (entry.built) {
        return entry.table;
    }
    const int passes = bucket == 0 ? 3 : (bucket == 1 ? 2 : 1);
    const double deadZone = bucket == 2 ? 0.06 : 0.10;
    for (int i = 0; i < 256; i++) {
        double t = i / 255.0;
        if (t <= deadZone) {
            entry.table[i] = 0;
            continue;
        }
        t = (t - deadZone) / (1.0 - deadZone);
        for (int k = 0; k < passes; k++) {
            t = t * t * (3.0 - 2.0 * t);
        }
        entry.table[i] = (BYTE)(t * 255.0 + 0.5);
    }
    entry.built = true;
    return entry.table;
}

// 把 mask 里 [x0,x1)x[y0,y1) 区域的白字按 color 混合到颜色缓冲区（alpha 预乘）
static void CompositeText(BYTE* dst, const BYTE* mask, int stride, int x0, int y0, int x1,
                          int y1, COLORREF color, int width, int height, const BYTE* lut) {
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 > width) x1 = width;
    if (y1 > height) y1 = height;

    const BYTE srcB = GetBValue(color);
    const BYTE srcG = GetGValue(color);
    const BYTE srcR = GetRValue(color);

    for (int y = y0; y < y1; y++) {
        const BYTE* maskRow = mask + (size_t)y * stride + (size_t)x0 * 4;
        BYTE* dstRow = dst + (size_t)y * stride + (size_t)x0 * 4;
        for (int x = x0; x < x1; x++) {
            // mask 是黑底白字，绿通道即可代表覆盖率
            const unsigned int sa = lut[maskRow[1]];
            if (sa) {
                const unsigned int inv = 255 - sa;
                dstRow[0] = (BYTE)((srcB * sa + dstRow[0] * inv) / 255);
                dstRow[1] = (BYTE)((srcG * sa + dstRow[1] * inv) / 255);
                dstRow[2] = (BYTE)((srcR * sa + dstRow[2] * inv) / 255);
                dstRow[3] = (BYTE)(sa + dstRow[3] * inv / 255);
            }
            maskRow += 4;
            dstRow += 4;
        }
    }
}

// ---------------------------------------------------------------------------
// 像素箭头：字体里的 ↑ / ↓ 同样要过抗锯齿，小字号下箭头尖端会糊成一团。
// 这里改成整像素画出来的实心箭头（尖端 + 竖杆），边缘全是硬像素，一个灰阶都没有。
// ---------------------------------------------------------------------------
struct ArrowMetrics {
    int width;    // 箭头像素宽（奇数，保证左右对称）
    int height;   // 箭头像素高
    int advance;  // 占位宽度（含右侧留白）
};

static ArrowMetrics ArrowSize(int emPixels) {
    ArrowMetrics m = {};
    m.height = std::max(7, (int)(emPixels * 0.78f + 0.5f)) | 1;  // 奇数高，尖端正好落一像素
    m.width = std::max(5, (int)(emPixels * 0.46f + 0.5f)) | 1;   // 奇数宽，左右对称
    m.advance = m.width + std::max(2, emPixels / 8);
    return m;
}

// 往 mask（32 位、黑底）里画一个对称实心箭头。
// 结构：一个三角形箭头头部（约占 60% 高）+ 一根 1~2px 竖杆，竖杆严格居中。
// 全部整像素，无任何灰阶。
static void DrawArrowMask(BYTE* mask, int width, int height, int left, int centerY,
                          const ArrowMetrics& am, bool up) {
    const int cx = left + am.width / 2;
    const int top = centerY - am.height / 2;
    const int headRows = std::max(3, am.height * 3 / 5);
    const int halfMax = am.width / 2;
    const int shaftHalf = am.width >= 9 ? 1 : 0;  // 杆半宽：大箭头 1，小箭头 0

    for (int row = 0; row < am.height; row++) {
        const int y = top + row;
        if (y < 0 || y >= height) {
            continue;
        }
        const int dist = up ? row : (am.height - 1 - row);
        int half;
        if (dist < headRows) {
            // 三角形头部：半宽从 0 线性增大到 halfMax
            half = (halfMax * dist) / (headRows - 1);
        } else {
            half = shaftHalf;
        }
        int x0 = cx - half;
        int x1 = cx + half;
        if (x0 < 0) x0 = 0;
        if (x1 >= width) x1 = width - 1;
        BYTE* row0 = mask + (size_t)y * width * 4;
        for (int x = x0; x <= x1; x++) {
            row0[x * 4 + 0] = 255;
            row0[x * 4 + 1] = 255;
            row0[x * 4 + 2] = 255;
            row0[x * 4 + 3] = 255;
        }
    }
}

// 一行文字里可能夹着 ↑ / ↓，拆成"文字段 / 箭头段"分别画：
// 文字走 DirectWrite（整像素起步），箭头走上面的整像素图形。
// 位置全部自己算，不再交给 layout 做居中（那会引入小数偏移，正是雾边的来源）。
struct ArrowMark {
    int left;
    int centerY;
    int em;
    bool up;
};

static void DrawLineRuns(DWriteText& text, const std::wstring& line, const RECT& rc, int align,
                         std::vector<ArrowMark>* arrows, bool* anyText) {
    const int em = text.EmPixels();
    const ArrowMetrics am = ArrowSize(em);

    struct Run {
        std::wstring text;
        bool arrow;
        bool up;
        int advance;
    };
    std::vector<Run> runs;
    std::wstring buffer;
    for (wchar_t ch : line) {
        if (ch == L'\u2191' || ch == L'\u2193') {
            if (!buffer.empty()) {
                runs.push_back({buffer, false, false, 0});
                buffer.clear();
            }
            runs.push_back({L"", true, ch == L'\u2191', am.advance});
        } else {
            buffer += ch;
        }
    }
    if (!buffer.empty()) {
        runs.push_back({buffer, false, false, 0});
    }

    int total = 0;
    for (auto& run : runs) {
        if (!run.arrow) {
            const int measured = text.Measure(run.text);
            run.advance = measured > 0 ? measured : 0;
        }
        total += run.advance;
    }

    const int avail = rc.right - rc.left;
    int x = rc.left;
    if (align == 1) {
        x += (avail - total) / 2;
    } else if (align == 2) {
        x += avail - total;
    }
    if (x < rc.left) {
        x = rc.left;
    }
    x = (int)floorl((long double)x + 0.5L);  // 整像素起步

    const int centerY = (rc.top + rc.bottom) / 2;
    for (const auto& run : runs) {
        if (run.arrow) {
            arrows->push_back({x, centerY, em, run.up});
        } else if (!run.text.empty()) {
            if (text.DrawAt(run.text, x, rc.top, x + run.advance, rc.bottom) >= 0) {
                *anyText = true;
            }
        }
        x += run.advance;
    }
}
