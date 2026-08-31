#include "grfmedia/RgbaImage.h"

namespace grfmedia {

void RgbaImage::composite(const std::uint8_t* src,
                          int srcW,
                          int srcH,
                          std::uint8_t alphaMul,
                          int dstX,
                          int dstY)
{
    if (srcW <= 0 || srcH <= 0 || !src)
        return;

    for (int y = 0; y < srcH; ++y)
    {
        int cy = dstY + y;
        if (cy < 0 || cy >= height)
            continue;
        const std::uint8_t* sp = src + (std::size_t)y * (std::size_t)srcW * 4;
        for (int x = 0; x < srcW; ++x)
        {
            int cx = dstX + x;
            if (cx < 0 || cx >= width)
                continue;

            std::uint8_t* dp = at(cx, cy);
            const std::uint8_t* pp = sp + (std::size_t)x * 4;

            unsigned sa = (unsigned)pp[3] * alphaMul / 255u;
            if (sa == 0)
                continue;

            // Straight-alpha source-over.
            unsigned da = 255u - sa;
            for (int c = 0; c < 3; ++c)
                dp[c] = (std::uint8_t)(((unsigned)pp[c] * sa +
                                        (unsigned)dp[c] * da) /
                                       255u);
            dp[3] = (std::uint8_t)(sa + (unsigned)dp[3] * da / 255u);
        }
    }
}

void RgbaImage::flipHorizontal()
{
    int stride = width * 4;
    std::vector<std::uint8_t> row((std::size_t)stride);
    for (int y = 0; y < height; ++y)
    {
        std::uint8_t* r = px.data() + (std::size_t)y * (std::size_t)stride;
        std::copy(r, r + stride, row.begin());
        for (int x = 0; x < width; ++x)
        {
            std::uint8_t* dst = r + (std::size_t)x * 4;
            const std::uint8_t* src =
                row.data() + (std::size_t)(width - 1 - x) * 4;
            dst[0] = src[0];
            dst[1] = src[1];
            dst[2] = src[2];
            dst[3] = src[3];
        }
    }
}

} // namespace grfmedia