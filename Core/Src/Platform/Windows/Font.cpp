/**
 * @file
 * @author Max Godefroy
 * @date 13/01/2026.
 */

#include "KryneEngine/Core/Platform/Platform.hpp"

#include <dwrite_2.h>
#include <wrl.h>
#include <string>
#include <d2d1.h>

namespace KryneEngine::Platform
{
    bool RetrieveSystemDefaultGlyph(
        u32 _unicodeCodePoint,
        void* _userData,
        FontGlyphMetricsFunction _fontMetrics,
        FontNewContourFunction _newContour,
        FontNewEdgeFunction _newEdge,
        FontNewConicFunction _newConic,
        FontNewCubicFunction _newCubic,
        FontEndContourFunction _endContour,
        bool _verticalLayout)
    {
        using Microsoft::WRL::ComPtr;

        // Lazily create DWrite factory (simple approach; production code should cache and handle failures better).
        // IDWriteFactory2 (Windows 8.1+) is required for system font fallback.
        static ComPtr<IDWriteFactory2> s_factory;
        static ComPtr<IDWriteFontFallback> s_fontFallback;
        if (!s_factory)
        {
            if (FAILED(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory2), reinterpret_cast<IUnknown**>(s_factory.GetAddressOf()))))
                return false;
        }
        if (!s_fontFallback)
        {
            if (FAILED(s_factory->GetSystemFontFallback(&s_fontFallback)))
                return false;
        }

        if (_unicodeCodePoint > 0x10FFFF)
            return false;

        // Single code point, encoded as UTF-16.
        WCHAR text[2];
        UINT32 textLength;
        if (_unicodeCodePoint < 0x10000)
        {
            text[0] = static_cast<WCHAR>(_unicodeCodePoint);
            textLength = 1;
        }
        else
        {
            const u32 v = _unicodeCodePoint - 0x10000;
            text[0] = static_cast<WCHAR>(0xD800 + (v >> 10));
            text[1] = static_cast<WCHAR>(0xDC00 + (v & 0x3FF));
            textLength = 2;
        }

        // Minimal text source: the analysis source is only used during the MapCharacters call, so it lives on the
        // stack and ignores reference counting.
        struct SingleRunTextSource final : IDWriteTextAnalysisSource
        {
            const WCHAR* m_text;
            UINT32 m_length;

            SingleRunTextSource(const WCHAR* _text, const UINT32 _length) : m_text(_text), m_length(_length) {}

            STDMETHODIMP QueryInterface(REFIID riid, void** ppvObject) override
            {
                if (!ppvObject) return E_POINTER;
                if (riid == __uuidof(IUnknown) || riid == __uuidof(IDWriteTextAnalysisSource))
                {
                    *ppvObject = static_cast<IDWriteTextAnalysisSource*>(this);
                    return S_OK;
                }
                *ppvObject = nullptr;
                return E_NOINTERFACE;
            }
            STDMETHODIMP_(ULONG) AddRef() override { return 1; }
            STDMETHODIMP_(ULONG) Release() override { return 1; }

            STDMETHODIMP GetTextAtPosition(UINT32 _position, const WCHAR** _textString, UINT32* _textLength) override
            {
                if (_position < m_length)
                {
                    *_textString = m_text + _position;
                    *_textLength = m_length - _position;
                }
                else
                {
                    *_textString = nullptr;
                    *_textLength = 0;
                }
                return S_OK;
            }
            STDMETHODIMP GetTextBeforePosition(UINT32 _position, const WCHAR** _textString, UINT32* _textLength) override
            {
                if (_position > 0 && _position <= m_length)
                {
                    *_textString = m_text;
                    *_textLength = _position;
                }
                else
                {
                    *_textString = nullptr;
                    *_textLength = 0;
                }
                return S_OK;
            }
            STDMETHODIMP_(DWRITE_READING_DIRECTION) GetParagraphReadingDirection() override
            {
                return DWRITE_READING_DIRECTION_LEFT_TO_RIGHT;
            }
            STDMETHODIMP GetLocaleName(UINT32 _position, UINT32* _textLength, const WCHAR** _localeName) override
            {
                *_localeName = L"";
                *_textLength = _position < m_length ? m_length - _position : 0;
                return S_OK;
            }
            STDMETHODIMP GetNumberSubstitution(
                UINT32 _position,
                UINT32* _textLength,
                IDWriteNumberSubstitution** _numberSubstitution) override
            {
                *_numberSubstitution = nullptr;
                *_textLength = _position < m_length ? m_length - _position : 0;
                return S_OK;
            }
        };

        SingleRunTextSource textSource(text, textLength);

        // Let the system pick the best font covering the code point. "Segoe UI" is tried first, then DirectWrite
        // walks the system fallback chain (Segoe UI Symbol/Emoji, Yu Gothic, Malgun Gothic, ...).
        UINT32 mappedLength = 0;
        ComPtr<IDWriteFont> font;
        FLOAT mappedScale = 1.f;
        if (FAILED(s_fontFallback->MapCharacters(
            &textSource,
            0,
            textLength,
            nullptr,
            L"Segoe UI",
            DWRITE_FONT_WEIGHT_REGULAR,
            DWRITE_FONT_STYLE_NORMAL,
            DWRITE_FONT_STRETCH_NORMAL,
            &mappedLength,
            &font,
            &mappedScale)))
        {
            return false;
        }

        // No installed font supports this code point.
        if (!font || mappedLength != textLength)
            return false;

        ComPtr<IDWriteFontFace> fontFace;
        if (FAILED(font->CreateFontFace(&fontFace)))
            return false;

        // Map unicode codepoint to glyph index
        UINT16 glyphIndex = 0;
        UINT32 codePoint = _unicodeCodePoint;
        if (FAILED(fontFace->GetGlyphIndicesA(&codePoint, 1, &glyphIndex)))
            return false;
        if (glyphIndex == 0)
        {
            // glyph index 0 often means missing glyph (depends on font); still attempt if desired.
            // Return false to indicate not found.
            return false;
        }

        // Get design units and metrics to scale coordinates to a convenient unit.
        DWRITE_FONT_METRICS metrics{};
        fontFace->GetMetrics(&metrics);
        // We'll pick an emSize in device-independent units; choose 1000 to make values reasonable.
        const auto designUnitsPerEm = static_cast<FLOAT>(metrics.designUnitsPerEm);

        {
            DWRITE_GLYPH_METRICS dGlyphMetrics{};
            if (SUCCEEDED(fontFace->GetDesignGlyphMetrics(&glyphIndex, 1, &dGlyphMetrics, _verticalLayout)))
            {
                const s32 width = static_cast<s32>(dGlyphMetrics.advanceWidth) - dGlyphMetrics.leftSideBearing - dGlyphMetrics.rightSideBearing;
                const s32 height = static_cast<s32>(dGlyphMetrics.advanceHeight) - dGlyphMetrics.topSideBearing - dGlyphMetrics.bottomSideBearing;
                const s32 yMax = dGlyphMetrics.verticalOriginY - dGlyphMetrics.topSideBearing;
                const s32 yMin = yMax - height;

                const GlyphMetrics glyphMetrics = {
                    .m_bounds = { dGlyphMetrics.leftSideBearing, yMin, width, height },
                    .m_advance = static_cast<double>(_verticalLayout ? dGlyphMetrics.advanceHeight : dGlyphMetrics.advanceWidth),
                };

                const FontMetrics fontMetrics {
                    .m_ascender = static_cast<double>(metrics.ascent),
                    .m_descender = static_cast<double>(metrics.descent),
                    .m_lineHeight = static_cast<double>(metrics.ascent + metrics.descent + metrics.lineGap),
                    .m_unitPerEm = static_cast<double>(metrics.designUnitsPerEm)
                };

                _fontMetrics(fontMetrics, glyphMetrics, _userData);
            }
            else
            {
                return false;
            }
        }

        // Prepare a small geometry sink that forwards commands to the provided callbacks.
        struct GeometrySink final : IDWriteGeometrySink
        {
            volatile long m_ref;
            void* m_user;
            FontGlyphMetricsFunction m_fontMetrics;
            FontNewContourFunction m_newContour;
            FontNewEdgeFunction m_newEdge;
            FontNewConicFunction m_newConic;
            FontNewCubicFunction m_newCubic;
            FontEndContourFunction m_endContour;

            GeometrySink(
                void* user,
                FontGlyphMetricsFunction fm,
                FontNewContourFunction nc,
                FontNewEdgeFunction ne,
                FontNewConicFunction nco,
                FontNewCubicFunction ncu,
                FontEndContourFunction ec)
                    : m_ref(1)
                    , m_user(user)
                    , m_fontMetrics(fm)
                    , m_newContour(nc)
                    , m_newEdge(ne)
                    , m_newConic(nco)
                    , m_newCubic(ncu)
                    , m_endContour(ec)
            {}

            // IUnknown
            STDMETHODIMP QueryInterface(REFIID riid, void** ppvObject) override
            {
                if (!ppvObject) return E_POINTER;
                if (riid == __uuidof(IUnknown) || riid == __uuidof(IDWriteGeometrySink))
                {
                    *ppvObject = static_cast<IDWriteGeometrySink*>(this);
                    AddRef();
                    return S_OK;
                }
                *ppvObject = nullptr;
                return E_NOINTERFACE;
            }
            STDMETHODIMP_(ULONG) AddRef() override { return InterlockedIncrement(&m_ref); }
            STDMETHODIMP_(ULONG) Release() override
            {
                long v = InterlockedDecrement(&m_ref);
                if (v == 0) delete this;
                return v;
            }

            // IDWriteGeometrySink
            STDMETHODIMP_(void) BeginFigure(D2D1_POINT_2F _startPoint, D2D1_FIGURE_BEGIN _figureBegin) noexcept override
            {
                KE_ASSERT(_figureBegin == D2D1_FIGURE_BEGIN_FILLED);
                m_newContour(
                    { _startPoint.x, -_startPoint.y },
                    m_user);
            }

            STDMETHODIMP_(void) AddLines(const D2D1_POINT_2F* points, UINT pointsCount) noexcept override
            {
                for (UINT i = 0; i < pointsCount; ++i)
                {
                    m_newEdge(
                        { points[i].x, -points[i].y },
                        m_user);
                }
            }

            STDMETHODIMP_(void) AddBeziers(const D2D1_BEZIER_SEGMENT* beziers, UINT beziersCount) noexcept override
            {
                for (UINT i = 0; i < beziersCount; ++i)
                {
                    const D2D1_BEZIER_SEGMENT& b = beziers[i];
                    m_newCubic(
                        { b.point1.x, -b.point1.y },
                        { b.point2.x, -b.point2.y },
                        { b.point3.x, -b.point3.y },
                        m_user);
                }
            }

            STDMETHODIMP_(void) EndFigure(D2D1_FIGURE_END figureEnd) noexcept override
            {
                KE_ASSERT(figureEnd == D2D1_FIGURE_END_CLOSED);
                m_endContour(m_user);
            }

            STDMETHODIMP_(HRESULT) Close() noexcept override
            {
                return S_OK;
            }

            STDMETHODIMP_(void) SetFillMode(D2D1_FILL_MODE fillMode) noexcept override {}
            STDMETHODIMP_(void) SetSegmentFlags(D2D1_PATH_SEGMENT vertexFlags) noexcept override {}
        };

        // Build glyph arrays for a single glyph.
        UINT16 glyphIndices[1] = { glyphIndex };

        // Create our sink and call GetGlyphRunOutline.
        GeometrySink sinkRaw(
            _userData,
            _fontMetrics,
            _newContour,
            _newEdge,
            _newConic,
            _newCubic,
            _endContour);

        // Call GetGlyphRunOutline: emSize is design scaled already above.
        HRESULT hr = fontFace->GetGlyphRunOutline(
            designUnitsPerEm,
            glyphIndices,
            nullptr,
            nullptr,
            1,
            _verticalLayout,
            FALSE,
            &sinkRaw);

        if (FAILED(hr))
        {
            return false;
        }

        return true;
    }
}