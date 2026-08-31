#include "GrfHeader.h"

#include <cstring>

namespace grf {

namespace {
constexpr const char* kMasterOfMagic = "Master of Magic\0";
}

void GrfHeader::parse(const byte* data, std::size_t size)
{
    if (size < kDataByteSize)
    {
        throw GrfError(
            "File header is invalid, file is smaller than the header size");
    }

    magic.assign((const char*)data, 16);
    key.assign((const char*)data + 16, 14);

    std::uint32_t version = le32(data + 42);
    major = (std::uint8_t)(version >> 8);
    minor = (std::uint8_t)(version & 0xFF);

    // Fix: 2024-10-25 — int64 offsets (GRF 3.0 > 2GB). Detected when the
    // old u32 filetable offset at [30..34) is nonzero-long zero.
    if (is(3, 0) && data[35] == 0 && data[36] == 0 && data[37] == 0)
    {
        fileTableOffset = (std::int64_t)le64(data + 30); // stays relative
        seed = 0;
        filesCount = le32s(data + 38);
        realFilesCount = filesCount;
    } else
    {
        fileTableOffset = le32(data + 30); // relative to header end
        seed = le32s(data + 34);
        filesCount = le32s(data + 38);
        realFilesCount = filesCount - seed - 7;
    }
}

Bytes GrfHeader::write() const
{
    Bytes out(kDataByteSize, 0);

    std::uint8_t magicBytes[16];
    std::uint8_t keyBytes[14];

    if (magic.size() >= 16)
    {
        std::memcpy(magicBytes, magic.data(), 16);
    } else
    {
        std::memcpy(magicBytes, kMasterOfMagic, 16);
    }
    magicBytes[15] = 0;

    for (int i = 0; i < 14; ++i)
        keyBytes[i] = key.size() > (std::size_t)i ? (std::uint8_t)key[i]
                                                  : (std::uint8_t)(i + 1);

    std::memcpy(out.data(), magicBytes, 16);
    std::memcpy(out.data() + 16, keyBytes, 14);

    if (isCompatibleWith(3, 0))
    {
        put64(out.data() + 30, (std::uint64_t)fileTableOffset);
        put32(out.data() + 38, (std::uint32_t)realFilesCount);
    } else
    {
        put32(out.data() + 30, (std::uint32_t)fileTableOffset);
        put32(out.data() + 34, (std::uint32_t)seed);
        put32(out.data() + 38, (std::uint32_t)(realFilesCount + 7 + seed));
    }

    std::uint32_t version = ((std::uint32_t)major << 8) | minor;
    put32(out.data() + 42, version);

    return out;
}

} // namespace grf