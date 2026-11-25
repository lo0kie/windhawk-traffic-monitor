// ---------------------------------------------------------------------------
// DirectWrite 文字渲染（灰度抗锯齿，8 位覆盖率）
// ---------------------------------------------------------------------------
#pragma once
#include "preamble.h"

#include <dwrite.h>

// GDI 的灰度抗锯齿只有 16 级覆盖率（ClearType 在彩色通道里，取单通道更糟），
// 小字号下笔画边缘会有明显台阶，看起来就是"毛边"。
// 这里改用 DirectWrite 的 grayscale 模式画到遮罩位图上，覆盖率 256 级。
static const GUID kDWriteFactoryIID = {
    0xb859ee5a, 0xd838, 0x4b5b, {0xa2, 0xe8, 0x1a, 0xdc, 0x7d, 0x93, 0xdb, 0x48}};

// 把 IDWriteTextLayout 的 glyph 转发给 bitmap render target
class DWriteGlyphSink : public IDWriteTextRenderer {
   public:
    DWriteGlyphSink(IDWriteBitmapRenderTarget* target, IDWriteRenderingParams* params,
                    float pixelsPerDip)
        : m_target(target), m_params(params), m_pixelsPerDip(pixelsPerDip) {}

    HRESULT STDMETHODCALLTYPE DrawGlyphRun(void*, FLOAT baselineX, FLOAT baselineY,
                                           DWRITE_MEASURING_MODE mode,
                                           const DWRITE_GLYPH_RUN* run,
                                           const DWRITE_GLYPH_RUN_DESCRIPTION*,
                                           IUnknown*) override {
        // 遮罩统一画白色，真正的颜色在合成阶段再乘上去
        return m_target->DrawGlyphRun(baselineX, baselineY, mode, run, m_params,
                                      RGB(255, 255, 255), nullptr);
    }

    HRESULT STDMETHODCALLTYPE DrawUnderline(void*, FLOAT, FLOAT, const DWRITE_UNDERLINE*,
                                            IUnknown*) override {
        return E_NOTIMPL;
    }

    HRESULT STDMETHODCALLTYPE DrawStrikethrough(void*, FLOAT, FLOAT,
                                                const DWRITE_STRIKETHROUGH*,
                                                IUnknown*) override {
        return E_NOTIMPL;
    }

    HRESULT STDMETHODCALLTYPE DrawInlineObject(void*, FLOAT, FLOAT, IDWriteInlineObject*, BOOL,
                                               BOOL, IUnknown*) override {
        return E_NOTIMPL;
    }

    // 打开像素对齐，baseline 落在整像素上更锐利；1:1 对应我们自己的 DIB
    HRESULT STDMETHODCALLTYPE IsPixelSnappingDisabled(void*, BOOL* disabled) override {
        *disabled = FALSE;
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE GetCurrentTransform(void*, DWRITE_MATRIX* matrix) override {
        *matrix = DWRITE_MATRIX{1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f};
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE GetPixelsPerDip(void*, FLOAT* pixelsPerDip) override {
        *pixelsPerDip = m_pixelsPerDip;
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** object) override {
        if (riid == __uuidof(IDWriteTextRenderer) || riid == __uuidof(IUnknown)) {
            *object = this;
            AddRef();
            return S_OK;
        }
        *object = nullptr;
        return E_NOINTERFACE;
    }

    ULONG STDMETHODCALLTYPE AddRef() override { return ++m_ref; }
    ULONG STDMETHODCALLTYPE Release() override { return --m_ref; }

   private:
    IDWriteBitmapRenderTarget* m_target;
    IDWriteRenderingParams* m_params;
    float m_pixelsPerDip;
    ULONG m_ref = 1;
};

class DWriteText {
   public:
    bool Ready() const { return m_factory && m_gdi && m_format && m_target; }
    bool HasFormat() const { return m_factory && m_format; }
    HDC targetDc() const { return m_target ? m_target->GetMemoryDC() : nullptr; }

    bool Init() {
        if (m_factory) {
            return true;
        }
        if (FAILED(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, kDWriteFactoryIID,
                                       (IUnknown**)&m_factory)) ||
            !m_factory) {
            Release();
            return false;
        }
        if (FAILED(m_factory->GetGdiInterop(&m_gdi)) || !m_gdi) {
            Release();
            return false;
        }
        // 关键权衡：带 hinting 的模式（GDI_CLASSIC / GDI_NATURAL / NATURAL）在灰度输出时
        // 只有 7 级覆盖率（和 GDI 的 4-bit 一样糙，弧线会出明显台阶）；NATURAL_SYMMETRIC
        // 是唯一有 93 级灰阶的，但没有 hinting，小字号笔画两侧都是半透明像素、看着发糊。
        // 两害相权取后者：灰阶多、弧线平滑，糊的问题在 CompositeText 里用 S 形锐化收窄
        // 过渡带（把中间调往实心压），而不是退回 7 级。
        if (FAILED(m_factory->CreateCustomRenderingParams(1.8f, 1.0f, 0.0f,
                                                          DWRITE_PIXEL_GEOMETRY_FLAT,
                                                          DWRITE_RENDERING_MODE_NATURAL_SYMMETRIC,
                                                          &m_params)) ||
            !m_params) {
            m_factory->CreateRenderingParams(&m_params);
        }
        return true;
    }

    void Release() {
        if (m_target) {
            m_target->Release();
            m_target = nullptr;
        }
        if (m_format) {
            m_format->Release();
            m_format = nullptr;
        }
        if (m_params) {
            m_params->Release();
            m_params = nullptr;
        }
        if (m_gdi) {
            m_gdi->Release();
            m_gdi = nullptr;
        }
        if (m_factory) {
            m_factory->Release();
            m_factory = nullptr;
        }
        m_targetWidth = 0;
        m_targetHeight = 0;
        m_sizePx = 0;
    }

    // 字号按物理像素（em 高度）给，内部换算成 DIP（DWrite 单位是 1/96 英寸）；
    // weight 用 100-900，和 LOGFONT / DWRITE_FONT_WEIGHT 同一套数值
    bool SetFontPx(const std::wstring& face, int sizePx, int weight) {
        if (!Init()) {
            return false;
        }
        if (m_format && m_face == face && m_sizePx == sizePx && m_weight == weight) {
            return true;
        }
        if (m_format) {
            m_format->Release();
            m_format = nullptr;
        }
        // 中文界面字体通常把细体注册成独立字族（如 "Microsoft YaHei UI Light"），
        // 这种字族里没有 100-300 这些字重，直接给会退回常规，所以先换成 Light 字族
        std::wstring family = face;
        if (weight <= 300) {
            std::wstring lightFamily = face + L" Light";
            if (FamilyExists(lightFamily)) {
                family = lightFamily;
            }
        }

        // 像素 → DIP：DWrite 的字号是 DIP，物理像素 = DIP × pixelsPerDip
        const FLOAT sizeDip = (FLOAT)sizePx / m_pixelsPerDip;
        HRESULT hr = m_factory->CreateTextFormat(
            family.c_str(), nullptr, (DWRITE_FONT_WEIGHT)weight, DWRITE_FONT_STYLE_NORMAL,
            DWRITE_FONT_STRETCH_NORMAL, sizeDip, L"", &m_format);
        if (FAILED(hr) || !m_format) {
            m_format = nullptr;
            m_sizePx = 0;
            return false;
        }
        m_format->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
        m_face = face;
        m_sizePx = sizePx;
        m_weight = weight;
        return true;
    }

    // 判断某个字族是否存在（中文字体常把细体做成独立字族，见 SetFont）
    bool FamilyExists(const std::wstring& name) {
        if (!m_factory) {
            return false;
        }
        IDWriteFontCollection* collection = nullptr;
        if (FAILED(m_factory->GetSystemFontCollection(&collection, FALSE)) || !collection) {
            return false;
        }
        UINT32 index = 0;
        BOOL exists = FALSE;
        HRESULT hr = collection->FindFamilyName(name.c_str(), &index, &exists);
        collection->Release();
        return SUCCEEDED(hr) && exists;
    }

    // DWrite 的坐标单位是 DIP，这里告诉它 1 DIP 等于几个物理像素
    void SetDpi(int dpi) {
        float ppd = dpi > 0 ? (float)dpi / 96.0f : 1.0f;
        if (ppd == m_pixelsPerDip) {
            return;
        }
        m_pixelsPerDip = ppd;
        if (m_target) {
            m_target->SetPixelsPerDip(m_pixelsPerDip);
        }
    }

    bool EnsureTarget(HDC refDc, int width, int height) {
        if (!Init() || !m_gdi) {
            return false;
        }
        if (m_target && m_targetWidth == width && m_targetHeight == height) {
            return true;
        }
        if (m_target) {
            m_target->Release();
            m_target = nullptr;
        }
        if (FAILED(m_gdi->CreateBitmapRenderTarget(refDc, (UINT32)width, (UINT32)height,
                                                   &m_target)) ||
            !m_target) {
            m_target = nullptr;
            return false;
        }
        m_target->SetPixelsPerDip(m_pixelsPerDip);
        m_targetWidth = width;
        m_targetHeight = height;
        return true;
    }

    // 当前字号占多少物理像素（em 高度），锐化强度按它来定。
    // 字号已经是像素语义，直接返回，不再换算。
    int EmPixels() const { return m_sizePx; }

    // 自然宽度（像素）；失败返回 -1
    int Measure(const std::wstring& text) {
        if (!m_factory || !m_format || text.empty()) {
            return -1;
        }
        IDWriteTextLayout* layout = nullptr;
        if (FAILED(m_factory->CreateTextLayout(text.c_str(), (UINT32)text.size(), m_format, 4096.0f,
                                               4096.0f, &layout)) ||
            !layout) {
            return -1;
        }
        DWRITE_TEXT_METRICS metrics = {};
        int width = -1;
        if (SUCCEEDED(layout->GetMetrics(&metrics))) {
            // 度量是 DIP，换算成物理像素
            width = (int)(metrics.widthIncludingTrailingWhitespace * m_pixelsPerDip + 0.5f);
        }
        layout->Release();
        return width;
    }

    void Clear() {
        if (!m_target) {
            return;
        }
        HDC dc = m_target->GetMemoryDC();
        if (dc) {
            PatBlt(dc, 0, 0, m_targetWidth, m_targetHeight, BLACKNESS);
        }
    }

    // 左对齐画一行，落点对齐到整像素。
    // 对齐整像素是"去雾边"的关键：DirectWrite 的字形推进量是小数（例如 15.63px），
    // 每个字的子像素相位都不一样，同一行里就会各自带一条深浅不同的灰边，看着发虚。
    // 这里自己做垂直居中（只算一次、取整），不再让 layout 在行盒里做小数偏移。
    // 返回实际绘制宽度（像素），失败返回 -1；这样测量和绘制用同一次 layout，省一次重建。
    int DrawAt(const std::wstring& text, int leftPx, int topPx, int rightPx, int bottomPx) {
        if (!Ready() || text.empty()) {
            return -1;
        }
        const int widthPx = rightPx - leftPx;
        const int heightPx = bottomPx - topPx;
        if (widthPx <= 0 || heightPx <= 0) {
            return -1;
        }
        IDWriteTextLayout* layout = nullptr;
        if (FAILED(m_factory->CreateTextLayout(text.c_str(), (UINT32)text.size(), m_format,
                                               (FLOAT)widthPx / m_pixelsPerDip,
                                               (FLOAT)heightPx / m_pixelsPerDip, &layout)) ||
            !layout) {
            return -1;
        }
        layout->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
        layout->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_NEAR);

        DWRITE_TEXT_METRICS metrics = {};
        FLOAT offsetY = 0.0f;
        if (SUCCEEDED(layout->GetMetrics(&metrics))) {
            offsetY = ((FLOAT)heightPx - metrics.height * m_pixelsPerDip) * 0.5f;
        }
        const FLOAT x = (FLOAT)leftPx / m_pixelsPerDip;
        const FLOAT y = (FLOAT)(int)floorf((FLOAT)topPx + offsetY + 0.5f) / m_pixelsPerDip;

        DWriteGlyphSink sink(m_target, m_params, m_pixelsPerDip);
        layout->Draw(nullptr, &sink, x, y);

        const int drawnWidth =
            (int)(metrics.widthIncludingTrailingWhitespace * m_pixelsPerDip + 0.5f);
        layout->Release();
        return drawnWidth;
    }

   private:
    IDWriteFactory* m_factory = nullptr;
    IDWriteGdiInterop* m_gdi = nullptr;
    IDWriteRenderingParams* m_params = nullptr;
    IDWriteTextFormat* m_format = nullptr;
    IDWriteBitmapRenderTarget* m_target = nullptr;
    int m_targetWidth = 0;
    int m_targetHeight = 0;
    std::wstring m_face;
    int m_sizePx = 0;
    int m_weight = 400;
    float m_pixelsPerDip = 1.0f;
};
