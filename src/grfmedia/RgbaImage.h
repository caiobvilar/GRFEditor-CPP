#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace grfmedia {

// Top-down 32-bit bitmap used by every decoder in this module. Pixels are
// 4 bytes each in RGBA order, as expected by an OpenGL GL_RGBA texture.
struct RgbaImage
{
    int width = 0;
    int height = 0;
    std::vector<std::uint8_t> px; // sized width*height*4

    bool empty() const { return width == 0 || height == 0 || px.empty(); }

    void resize(int w, int h)
    {
        width = w;
        height = h;
        px.assign((std::size_t)w * (std::size_t)h * 4, 0);
    }

    std::uint8_t* at(int x, int y)
    {
        return px.data() +
               ((std::size_t)y * (std::size_t)width + (std::size_t)x) * 4;
    }
    const std::uint8_t* at(int x, int y) const
    {
        return px.data() +
               ((std::size_t)y * (std::size_t)width + (std::size_t)x) * 4;
    }

    // Premultiplied-not; straight-alpha source-over composite of "src" onto
    // this.
    void composite(const RgbaImage& src, int dstX, int dstY)
    {
        composite(src.px.data(),
                  src.width,
                  src.height,
                  std::uint8_t(255),
                  dstX,
                  dstY);
    }

    // Composite src with an extra per-pixel alpha multiplier in [0,255].
    void composite(const std::uint8_t* srcBgra,
                   int srcW,
                   int srcH,
                   std::uint8_t alphaMul,
                   int dstX,
                   int dstY);

    // Flip the image horizontally in place (used by the palette grid helper).
    void flipHorizontal();
};

} // namespace grfmedia