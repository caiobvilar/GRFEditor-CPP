#pragma once

#include "GrfUtil.h"

#include <string>

namespace grf {

// Compression layer, mirroring GRF/Core/Compression.cs + GrfCompression/*.
// - zlib stream (0x78 header) is used for BOTH the Gravity ("CpsCompression")
// and
//   the "DotNet" codecs: Gravity's native compress() and DotNet's ZOutputStream
//   both produce a standard zlib-wrapped stream. Default level 6.
// - LZSS and raw are used for legacy flagged entries.
// - LZMA is a proprietary comp_x64.dll/lzma.dll in the reference and is NOT
//   portable without the binary -> deferred, throws.
// - GrfEditorCrypt (file-table/file encryption) kernel is obfuscated in the
//   reference assembly -> deferred, throws when detected.
Bytes zlibCompress(const byte* data, std::size_t length, int level = 6);
// Full-stream zlib inflate (unknown output length; THOR file tables).
Bytes zlibDecompress(const byte* data, std::size_t length);
Bytes zlibDecompressExact(const byte* data,
                          std::size_t length,
                          std::size_t expected);

// gzip (RFC 1952) container decompress, used by RGZ (whole-file stream).
Bytes gzDecompress(const byte* data, std::size_t length);

Bytes lzssDecompress(const byte* data,
                     std::size_t length,
                     std::size_t expected);
Bytes rawDecompress(const byte* data, std::size_t length, std::size_t expected);

Bytes readFile(const std::string& path);
void writeFile(const std::string& path, const byte* data, std::size_t length);

} // namespace grf