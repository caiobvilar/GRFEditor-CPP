#pragma once

#include "GrfUtil.h"

#include <string>
#include <vector>

namespace grf {

// The GRF header is exactly 46 bytes:
//   [0..16)  magic   "Master of Magic\0" (ASCII)
//   [16..30) key     14 ASCII bytes (default {1..14})
//   [30..]   version dependent (see parse())
//   [42..46) version int32 = (MajorVersion << 8) + MinorVersion
class GrfHeader
{
  public:
    static constexpr int kDataByteSize = 46;

    std::string magic; // 16 bytes
    std::string key;   // 14 bytes
    std::uint8_t major = 2;
    std::uint8_t minor = 0;
    std::int64_t fileTableOffset =
        0; // relative to the end of the 46-byte header
    std::int32_t seed = 0;
    std::int32_t filesCount =
        0; // stored 3rd int (v<3): RealFilesCount + 7 + Seed
    std::int32_t realFilesCount = 0;

    bool encrypted =
        false; // __encryption.info present / file table decryption required
    bool encryptFileTable = false; // requested on save
    Bytes encryptionKey; // optional 14-byte key bytes for GrfEditorCrypt
                         // (deferred)

    void parse(const byte* data, std::size_t size);
    Bytes write() const;

    bool is(int m, int mn) const { return major == m && minor == mn; }
    // IsCompatibleWith: (major == MajorVersion && MinorVersion >= minor) ||
    // MajorVersion > major
    bool isCompatibleWith(int m, int mn) const
    {
        return (m == major && minor >= mn) || major > m;
    }
    bool isMajor(int m) const { return major == m; }
    double version() const { return (double)major + minor / 100.0; }

    // Default unit key, as used by the reference when creating a new container.
    static Bytes defaultKey()
    {
        Bytes k(14);
        for (int i = 0; i < 14; ++i)
            k[i] = (byte)(i + 1);
        return k;
    }
};

} // namespace grf