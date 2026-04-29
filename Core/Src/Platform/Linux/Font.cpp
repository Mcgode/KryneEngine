/**
 * @file
 * @author Max Godefroy
 * @date 10/04/2026.
 */

#include "KryneEngine/Core/Platform/Platform.hpp"

#include <fontconfig/fontconfig.h>

#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

namespace KryneEngine::Platform
{
    class FcInstance
    {
        FcInstance() = default;
        ~FcInstance()
        {
            if (m_initialized)
                FcFini();
        }

        static FcInstance s_instance;
        bool m_initialized = false;

    public:
        static void Init()
        {
            if (!s_instance.m_initialized)
            {
                KE_VERIFY(FcInit());
            }
        }
    };

    FcInstance FcInstance::s_instance = {};

    bool RetrieveSystemDefaultGlyph(
        const u32 _unicodeCodePoint,
        void* _userData,
        FontGlyphMetricsFunction _fontMetrics,
        FontNewContourFunction _newContour,
        FontNewEdgeFunction _newEdge,
        FontNewConicFunction _newConic,
        FontNewCubicFunction _newCubic,
        FontEndContourFunction _endContour,
        const bool _verticalLayout)
    {
        FcInstance::Init();

        FcCharSet* charSet = FcCharSetCreate();
        if (!charSet)
            return false;
        FcCharSetAddChar(charSet, _unicodeCodePoint);

        FcPattern* pattern = FcPatternCreate();
        if (!pattern)
        {
            FcCharSetDestroy(charSet);
            return false;
        }
        FcPatternAddCharSet(pattern, FC_CHARSET, charSet);

        FcConfigSubstitute(nullptr, pattern, FcMatchPattern);
        FcDefaultSubstitute(pattern);

        FcResult result;
        FcPattern* match = FcFontMatch(nullptr, pattern, &result);
        if (match == nullptr)
        {
            FcCharSetDestroy(charSet);
            FcPatternDestroy(pattern);
            return false;
        }

        FcChar8* file = nullptr;
        KE_VERIFY(FcPatternGetString(match, FC_FILE, 0, &file) == FcResultMatch);

        u8* fileBuffer = nullptr;
        size_t fileSize = 0;

        FILE* fontFile = fopen(reinterpret_cast<const char*>(file), "rb");
        FcPatternDestroy(match);
        FcCharSetDestroy(charSet);
        FcPatternDestroy(pattern);

        if (fontFile == nullptr)
            return false;

        fseek(fontFile, 0, SEEK_END);
        fileSize = ftell(fontFile);
        fseek(fontFile, 0, SEEK_SET);

        fileBuffer = new u8[fileSize];
        fread(fileBuffer, fileSize, 1, fontFile);
        fclose(fontFile);

        stbtt_fontinfo fontInfo;
        stbtt_InitFont(&fontInfo, fileBuffer, 0);

        const s32 glyphIndex = stbtt_FindGlyphIndex(&fontInfo, static_cast<s32>(_unicodeCodePoint));

        {
            s32 ascent, descent, lineGap;
            stbtt_GetFontVMetrics(&fontInfo, &ascent, &descent, &lineGap);

            const float unitsPerEm = 1.f / stbtt_ScaleForMappingEmToPixels(&fontInfo, 1);

            const FontMetrics fontMetrics = {
                .m_ascender = static_cast<double>(ascent),
                .m_descender = static_cast<double>(descent),
                .m_lineHeight = static_cast<double>(ascent + descent + lineGap),
                .m_unitPerEm = unitsPerEm,
            };

            s32 advanceX, bearingX;
            stbtt_GetGlyphHMetrics(&fontInfo, glyphIndex, &advanceX, &bearingX);

            s32 bboxLeft, bboxTop, bboxRight, bboxBottom;
            stbtt_GetGlyphBox(&fontInfo, glyphIndex, &bboxLeft, &bboxTop, &bboxRight, &bboxBottom);

            const GlyphMetrics glyphMetrics {
                .m_bounds = {
                    static_cast<double>(bboxLeft),
                    static_cast<double>(bboxTop),
                    static_cast<double>(bboxRight - bboxLeft),
                    static_cast<double>(bboxBottom - bboxTop)
                },
                .m_advance = static_cast<double>(advanceX),
            };

            _fontMetrics(fontMetrics, glyphMetrics, _userData);
        }

        {
            stbtt_vertex* vertices = nullptr;
            const s32 vertexCount = stbtt_GetGlyphShape(&fontInfo, glyphIndex, &vertices);

            for (auto i = 0; i < vertexCount; i++)
            {
                const stbtt_vertex& vertex = vertices[i];
                switch (vertex.type)
                {
                case STBTT_vmove:
                    if (i > 0)
                        _endContour(_userData);
                    _newContour({ vertex.x, vertex.y }, _userData);
                    break;
                case STBTT_vline:
                    _newEdge( { vertex.x, vertex.y }, _userData);
                    break;
                case STBTT_vcurve:
                    _newConic({ vertex.cx, vertex.cy }, { vertex.x, vertex.y }, _userData);
                    break;
                case STBTT_vcubic:
                    _newCubic(
                        { vertex.cx, vertex.cy },
                        { vertex.cx1, vertex.cy1 },
                        { vertex.x, vertex.y },
                        _userData);
                    break;
                default:
                    KE_ERROR("Invalid vertex type");
                }
            }

            if (vertexCount > 0)
            {
                _endContour(_userData);
            }

            stbtt_FreeShape(&fontInfo, vertices);
        }

        delete[] fileBuffer;

        return true;
    }
}