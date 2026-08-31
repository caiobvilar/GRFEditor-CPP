#pragma once

#include "grfcore/GrfUtil.h"
#include "grfmedia/RgbaImage.h"

#include <array>
#include <cstdint>
#include <vector>

namespace grfmedia {

// One decoded sprite frame, display-ready (top-down RGBA).
struct SprFrame
{
    int width = 0;
    int height = 0;
    std::vector<std::uint8_t> rgba; // width*height*4
};

// .spr sprite file: "SP" + minor + major. Supports the common 2.0-2.2
// indexed8+RLE format and the modern 3.2 RGBA/zlib format, matching
// GRFEditor/GRF/FileFormats/SprFormat/SprLoader.cs.
struct Spr
{
    int major = 0;
    int minor = 0;
    int indexed8Count = 0; // first `indexed8Count` frames use the palette
    int bgraCount = 0;
    bool hasPalette = false;
    std::array<std::uint8_t, 1024> palette =
        {};                       // raw file order (4 bytes/color)
    std::vector<SprFrame> frames; // indexed8 frames first, then bgra32

    static Spr load(const grf::Bytes& data);

    bool isV32() const { return major >= 3 || (major == 3 && minor >= 2); }

    // Absolute index -> frame with bounds checking.
    const SprFrame* frame(int absoluteIndex) const
    {
        if (absoluteIndex < 0 || absoluteIndex >= (int)frames.size())
            return nullptr;
        return &frames[absoluteIndex];
    }
};

// .pal palette file: raw 1024 bytes (256 colors). Palette entries are stored
// as B,G,R,A in the classic RO files; we expose them display-swapped to RGBA.
struct PalFile
{
    std::array<std::uint8_t, 1024> colors = {}; // RGBA (display order)

    static PalFile load(const grf::Bytes& data);

    // 256 <-> 16x16 swatch grid.
    RgbaImage toGrid(int cell = 16) const;
};

} // namespace grfmedia