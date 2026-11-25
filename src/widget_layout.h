// ---------------------------------------------------------------------------
// 窗口尺寸与行布局
// ---------------------------------------------------------------------------
#pragma once
#include "widget_text.h"

void TaskbarWidget::UpdateLayout() {
    UINT dpi = 96;
    if (m_taskbar) {
        dpi = GetDpiForWindow(m_taskbar);
        if (dpi == 0) {
            dpi = 96;
        }
    }
    if (!m_dc) {
        HDC screenDc = GetDC(nullptr);
        m_dc = CreateCompatibleDC(screenDc);
        ReleaseDC(nullptr, screenDc);
        m_oldFont = (HFONT)SelectObject(m_dc, GetStockObject(DEFAULT_GUI_FONT));
    }

    m_dpi = dpi;

    // 字体指纹：字体名、字号、粗体、DPI 任何一项变了都要重建，否则改设置要重装才生效
    std::wstring fontKey = m_settings.fontFace;
    fontKey += L'|';
    fontKey += std::to_wstring(m_settings.fontSize);
    fontKey += L'|';
    fontKey += std::to_wstring(m_settings.fontWeight);
    fontKey += L'|';
    fontKey += std::to_wstring(dpi);
    if (!m_font || fontKey != m_fontKey) {
        if (m_font) {
            // 旧字体还被 m_dc 选着，先换回原来的字体对象再删
            SelectObject(m_dc, m_oldFont ? m_oldFont : GetStockObject(DEFAULT_GUI_FONT));
        }
        CreateFonts((int)m_dpi);
        SelectObject(m_dc, m_font);
        m_fontKey = fontKey;
    }

    // 单排一行；双排两行，各自一个格式串
    std::vector<std::wstring> formats;
    if (m_settings.arrange == 0) {
        formats.push_back(m_settings.singleRow);
    } else {
        formats.push_back(m_settings.topRow);
        formats.push_back(m_settings.bottomRow);
    }

    std::vector<std::wstring> texts;
    int maxTextWidth = 0;
    SIZE size = {};
    for (const auto& format : formats) {
        std::wstring text = ExpandFormat(format, m_data, m_settings);
        texts.push_back(text);
        // 用真实文本量宽度：窗口宽度随数值变化，紧贴文字（不再用样本串防抖动）
        // 有 DirectWrite 就用它的度量，避免"量宽度用 GDI、画字用 DWrite"对不上被裁
        int measured = m_text.HasFormat() ? m_text.Measure(text) : -1;
        if (measured < 0) {
            GetTextExtentPoint32W(m_dc, text.c_str(), (int)text.size(), &size);
            measured = size.cx;
        }
        if (measured > maxTextWidth) {
            maxTextWidth = measured;
        }
    }

    RECT rcTaskbar = {};
    GetClientRect(m_taskbar, &rcTaskbar);
    int taskbarHeight = rcTaskbar.bottom - rcTaskbar.top;

    // 单排高度取 TASKBAR_WND_HEIGHT 的 2/3，双排用整高（与原版一致）
    int baseHeight = MulDiv(32, (int)m_dpi, 96);
    int height = texts.size() > 1 ? baseHeight : baseHeight * 2 / 3;

    // 字号调大以后，单排的 2/3 高度可能装不下一行字，按实际行高补足
    TEXTMETRICW tm = {};
    if (GetTextMetricsW(m_dc, &tm) && tm.tmHeight > 0) {
        int needed = texts.size() > 1 ? tm.tmHeight * 2 : tm.tmHeight;
        needed += Dpi(2);
        if (height < needed) {
            height = needed;
        }
    }

    if (taskbarHeight > 8 && height > taskbarHeight - Dpi(4)) {
        height = taskbarHeight - Dpi(4);
    }
    if (height < 10) {
        height = 10;
    }

    int padding = Dpi(5);
    int width = maxTextWidth + padding * 2;
    if (rcTaskbar.right > 0 && width > rcTaskbar.right) {
        width = rcTaskbar.right;
    }

    m_width = width;
    m_height = height;

    m_lines.clear();
    int count = (int)texts.size();
    for (int i = 0; i < count; i++) {
        LineLayout line;
        line.rect.left = padding;
        line.rect.right = width - padding;
        line.rect.top = height * i / count;
        line.rect.bottom = height * (i + 1) / count;
        // 双排时在上下两行之间加留白（可正可负）。正数把两行拉开、负数拉近（可为负，
        // 甚至重叠）。行矩形只调整相邻边，文字在各自半行内仍垂直居中。
        if (i == 0 && count == 2) {
            line.rect.bottom -= m_settings.rowGap / 2;
        } else if (i == 1 && count == 2) {
            line.rect.top += m_settings.rowGap / 2;
        }
        line.text = texts[i];
        m_lines.push_back(line);
    }
}
