#include "Compression.h"

#include <zlib.h>

#include <cstdio>
#include <fstream>
#include <memory>

namespace grf {

Bytes zlibCompress(const byte* data, std::size_t length, int level)
{
    uLongf bound = compressBound((uLong)length);
    Bytes out(bound);
    int res = compress2(out.data(), &bound, data, (uLong)length, level);
    if (res != Z_OK)
        throw GrfError("zlib compress failed: " + std::to_string(res));
    out.resize(bound);
    return out;
}

Bytes zlibDecompress(const byte* data, std::size_t length)
{
    z_stream strm{};
    strm.next_in = (Bytef*)data;
    strm.avail_in = (uInt)length;
    if (inflateInit(&strm) != Z_OK)
        throw GrfError("zlib inflateInit failed");

    Bytes out;
    byte chunk[65536];
    int res;
    do
    {
        strm.next_out = chunk;
        strm.avail_out = sizeof chunk;
        res = inflate(&strm, Z_NO_FLUSH);
        if (res != Z_OK && res != Z_STREAM_END)
        {
            inflateEnd(&strm);
            throw GrfError("zlib inflate failed: " + std::to_string(res));
        }
        out.insert(out.end(), chunk, chunk + (sizeof chunk) - strm.avail_out);
    } while (res != Z_STREAM_END);
    inflateEnd(&strm);
    return out;
}

Bytes zlibDecompressExact(const byte* data,
                          std::size_t length,
                          std::size_t expected)
{
    Bytes out(expected);

    z_stream strm{};
    strm.next_in = (Bytef*)data;
    strm.avail_in = (uInt)length;
    strm.next_out = out.data();
    strm.avail_out = (uInt)expected;

    int res = inflateInit(&strm);
    if (res != Z_OK)
        throw GrfError("zlib inflateInit failed: " + std::to_string(res));

    res = inflate(&strm, Z_FINISH);
    if (res != Z_STREAM_END)
    {
        inflateEnd(&strm);
        throw GrfError("zlib inflate failed: " + std::to_string(res));
    }
    if ((std::size_t)strm.total_out != expected)
    {
        inflateEnd(&strm);
        throw GrfError("zlib inflate size mismatch");
    }
    inflateEnd(&strm);
    return out;
}

Bytes lzssDecompress(const byte* data, std::size_t length, std::size_t expected)
{
    if (length == 0 || expected == 0)
        return {};

    Bytes out(expected);
    std::size_t out_offset = 0;
    ByteReader input(data, length);

    byte control = input.u8();
    int control_count = 0;

    while (true)
    {
        if ((control & 1) == 0)
        {
            out[out_offset] = input.u8();
            out_offset++;
        } else
        {
            std::uint16_t codeword = input.u16();
            int phrase_length = ((codeword & 0xf000) >> 12) + 2;
            int phrase_index = (codeword & 0x0fff);

            for (int i = 0; i < phrase_length; i++)
            {
                if (out_offset < (std::size_t)phrase_index ||
                    out_offset >= out.size())
                {
                    throw GrfError("LZSS phrase out of bounds");
                }
                out[out_offset] = out[out_offset - phrase_index];
                out_offset++;
            }
        }

        control = (byte)(control >> 1);
        control_count++;

        if (!input.canRead())
            break;

        if (control_count >= 8)
        {
            control = input.u8();
            control_count = 0;
        }
    }

    return out;
}

Bytes gzDecompress(const byte* data, std::size_t length)
{
    // windowBits = 31 accepts both zlib and gzip streams (RGZ uses gzip).
    z_stream strm{};
    strm.next_in = (Bytef*)data;
    strm.avail_in = (uInt)length;
    if (inflateInit2(&strm, 31) != Z_OK)
        throw GrfError("gzip inflateInit2 failed");

    Bytes out;
    byte chunk[65536];
    int res;
    do
    {
        strm.next_out = chunk;
        strm.avail_out = sizeof chunk;
        res = inflate(&strm, Z_NO_FLUSH);
        if (res != Z_OK && res != Z_STREAM_END)
        {
            inflateEnd(&strm);
            throw GrfError("gzip inflate failed: " + std::to_string(res));
        }
        out.insert(out.end(), chunk, chunk + (sizeof chunk) - strm.avail_out);
    } while (res != Z_STREAM_END);
    inflateEnd(&strm);
    return out;
}

Bytes rawDecompress(const byte* data, std::size_t length, std::size_t expected)
{
    Bytes out(data, data + std::min(length, expected));
    out.resize(expected, 0);
    return out;
}

Bytes readFile(const std::string& path)
{
    std::ifstream in(path, std::ios::binary);
    if (!in)
        throw GrfError("Cannot open file: " + path);
    in.seekg(0, std::ios::end);
    std::streamoff size = in.tellg();
    in.seekg(0, std::ios::beg);
    Bytes out((std::size_t)size);
    if (size > 0)
        in.read((char*)out.data(), size);
    return out;
}

void writeFile(const std::string& path, const byte* data, std::size_t length)
{
    std::ofstream out(path, std::ios::binary);
    if (!out)
        throw GrfError("Cannot write file: " + path);
    out.write((const char*)data, (std::streamsize)length);
}

} // namespace grf