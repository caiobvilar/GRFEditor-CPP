#include "grfmedia/Spr.h"

#include "grfcore/Compression.h"
#include "grfcore/GrfUtil.h"

#include <algorithm>
#include <cstring>
#include <utility>

namespace grfmedia {
namespace {

// RLE used by indexed8 SPR frames (GRF/FileFormats/Rle.cs): a 0 byte means
// "skip the NEXT byte's worth of transparent pixels", anything else is a
// literal palette index. The remaining pixels are transparent (index 0).
grf::Bytes
rleDecompress(const grf::byte* data, std::size_t length, std::size_t expected)
{
    grf::Bytes out(expected, 0);
    std::size_t pos = 0;
    for (std::size_t k = 0; k < length && pos < expected; ++k)
    {
        grf::byte b = data[k];
        if (b == 0)
        {
            if (k + 1 >= length)
                break;
            pos += data[++k];
        } else
        {
            out[pos] = b;
            ++pos;
        }
    }
    return out;
}

void convertIndexed8ToRgba(const grf::byte* indices,
                           std::size_t pixelCount,
                           const std::array<std::uint8_t, 1024>& palette,
                           SprFrame& f)
{
    f.rgba.resize(pixelCount * 4);
    for (std::size_t k = 0; k < pixelCount; ++k)
    {
        grf::byte idx = indices[k];
        const grf::byte* pal = palette.data() + (std::size_t)idx * 4;
        // File palette order {R,G,B,A}; display order swaps R/B -> GL RGBA.
        f.rgba[k * 4 + 0] = pal[2];
        f.rgba[k * 4 + 1] = pal[1];
        f.rgba[k * 4 + 2] = pal[0];
        f.rgba[k * 4 + 3] = pal[3]; // index 0 already has alpha 0 after load
    }
}

} // namespace

Spr Spr::load(const grf::Bytes& data)
{
    Spr spr;
    grf::ByteReader r(data);
    if (r.remaining() < 8)
        throw grf::GrfError("SPR: file too small for header");
    if (r.u8() != 'S' || r.u8() != 'P')
        throw grf::GrfError("SPR: bad magic (expected \"SP\")");
    spr.minor = r.u8();
    spr.major = r.u8();

    if (spr.isV32())
    {
        spr.bgraCount = r.i32();
    } else
    {
        spr.indexed8Count = r.u16();
        spr.bgraCount = r.u16();
    }

    // The C# loader starts scanning frames at offset 8 (even for 1.0).
    r = grf::ByteReader(data);
    r.forward(8);

    const std::size_t palOffset = spr.indexed8Count > 0 && data.size() >= 1024
                                      ? data.size() - 1024
                                      : data.size();

    // The trailing 1024-byte palette (indexed8 sprites), loaded before any
    // frame conversion so the palette is available.
    if (spr.indexed8Count > 0 && data.size() >= 1024)
    {
        std::memcpy(spr.palette.data(), data.data() + data.size() - 1024, 1024);
        for (int i = 0; i < 256; ++i)
            spr.palette[i * 4 + 3] = 255;
        spr.palette[3] = 0; // index 0 = transparent
        spr.hasPalette = true;
    }

    int framesRead = 0;

    // Indexed8 frames.
    for (int i = 0; i < spr.indexed8Count; ++i)
    {
        if (r.pos() >= palOffset)
            break; // palette region reached early; C# clamps the count
        int width = r.u16();
        int height = r.u16();
        if (width <= 0 || height <= 0 || width > 8192 || height > 8192)
            throw grf::GrfError("SPR: implausible indexed8 frame size");

        std::size_t clen;
        bool compressed = true;
        if (spr.minor >= 2)
        { // 2.2+
            clen = (std::size_t)(std::uint32_t)r.i32();
        } else if (spr.minor >= 1)
        { // 2.1
            clen = r.u16();
        } else
        { // 1.0 / 2.0: raw width*height
            clen = (std::size_t)width * (std::size_t)height;
            compressed = false;
        }
        if (r.remaining() < clen)
            throw grf::GrfError("SPR: truncated indexed8 frame data");

        grf::Bytes pixels;
        const grf::byte* src = r.data() + r.pos();
        if (compressed)
            pixels = rleDecompress(
                src, clen, (std::size_t)width * (std::size_t)height);
        else
            pixels.assign(src, src + clen);
        r.forward(clen);

        SprFrame f;
        f.width = width;
        f.height = height;
        convertIndexed8ToRgba(pixels.data(),
                              (std::size_t)width * (std::size_t)height,
                              spr.palette,
                              f);
        spr.frames.push_back(std::move(f));
        ++framesRead;
    }
    spr.indexed8Count = framesRead;

    // Bgra32 frames.
    for (int i = 0; i < spr.bgraCount; ++i)
    {
        if (r.pos() >= palOffset && spr.indexed8Count > 0)
            break;
        int width = r.u16();
        int height = r.u16();
        if (width <= 0 || height <= 0 || width > 8192 || height > 8192)
            throw grf::GrfError("SPR: implausible bgra32 frame size");

        SprFrame f;
        f.width = width;
        f.height = height;

        if (spr.isV32())
        {
            std::size_t clen = (std::size_t)(std::uint32_t)r.i32();
            if (r.remaining() < clen)
                throw grf::GrfError("SPR: truncated 3.2 frame data");
            grf::Bytes raw = grf::zlibDecompressExact(
                r.data() + r.pos(),
                clen,
                (std::size_t)width * (std::size_t)height * 4);
            r.forward(clen);
            f.rgba.resize(raw.size());
            // Raw data is BGRA display order -> RGBA for GL.
            for (std::size_t k = 0, n = raw.size() / 4; k < n; ++k)
            {
                f.rgba[k * 4 + 0] = raw[k * 4 + 2];
                f.rgba[k * 4 + 1] = raw[k * 4 + 1];
                f.rgba[k * 4 + 2] = raw[k * 4 + 0];
                f.rgba[k * 4 + 3] = raw[k * 4 + 3];
            }
        } else
        {
            std::size_t n = (std::size_t)width * (std::size_t)height * 4;
            if (r.remaining() < n)
                throw grf::GrfError("SPR: truncated bgra32 frame data");
            f.rgba.resize(n);
            const grf::byte* src = r.data() + r.pos();
            // Stored bottom-up, per-pixel {A,B,G,R}; output top-down RGBA.
            for (int y = 0; y < height; ++y)
            {
                const grf::byte* row = src + (std::size_t)(height - 1 - y) *
                                                 (std::size_t)width * 4;
                std::uint8_t* dst =
                    f.rgba.data() + (std::size_t)y * (std::size_t)width * 4;
                for (int x = 0; x < width; ++x)
                {
                    const grf::byte* p = row + (std::size_t)x * 4;
                    dst[x * 4 + 0] = p[3];
                    dst[x * 4 + 1] = p[2];
                    dst[x * 4 + 2] = p[1];
                    dst[x * 4 + 3] = p[0];
                }
            }
            r.forward(n);
        }
        spr.frames.push_back(std::move(f));
    }
    spr.bgraCount = (int)spr.frames.size() - spr.indexed8Count;
    if (spr.bgraCount < 0)
        spr.bgraCount = 0;

    return spr;
}

PalFile PalFile::load(const grf::Bytes& data)
{
    PalFile pal;
    if (data.size() < 1024)
        throw grf::GrfError("PAL: file too small (expected 1024 bytes)");
    std::memcpy(pal.colors.data(), data.data(), 1024);
    // File palette order {B,G,R,A}-ish; swap to RGBA for GL.
    for (int i = 0; i < 256; ++i)
    {
        std::uint8_t& b = pal.colors[i * 4 + 0];
        std::uint8_t& r = pal.colors[i * 4 + 2];
        std::swap(b, r);
    }
    pal.colors[3] = 0; // index 0 = transparent
    for (int i = 1; i < 256; ++i)
        pal.colors[i * 4 + 3] = 255;
    return pal;
}

RgbaImage PalFile::toGrid(int cell) const
{
    RgbaImage img;
    img.resize(cell * 16, cell * 16);
    for (int i = 0; i < 256; ++i)
    {
        int row = i / 16;
        int col = i % 16;
        const std::uint8_t* c = colors.data() + i * 4;
        for (int y = 0; y < cell; ++y)
            for (int x = 0; x < cell; ++x)
            {
                std::uint8_t* p = img.at(col * cell + x, row * cell + y);
                p[0] = c[0];
                p[1] = c[1];
                p[2] = c[2];
                p[3] = c[3];
            }
    }
    return img;
}

} // namespace grfmedia