#pragma once

#include "FileEntry.h"
#include "GrfHeader.h"

#include <fstream>
#include <string>
#include <vector>

namespace grf {

// Holds the parsed file table (the "ContainerTable" of the reference).
class FileTable
{
  public:
    explicit FileTable(GrfHeader& header) : _header(header) {}

    GrfHeader& header() { return _header; }
    const GrfHeader& header() const { return _header; }
    std::vector<FileEntry>& entries() { return _entries; }
    const std::vector<FileEntry>& entries() const { return _entries; }

    // The stream must be positioned where the file table starts
    // (header.fileTableOffset + 46). Dispatches by version.
    void load(std::ifstream& stream);

    // Serialize the full table block: [v3: 4 zero bytes] + compressedLen +
    // tableLen + zlib-compressed records. Returns the block.
    Bytes writeTable(GrfHeader& header) const;

    std::int32_t tableSizeCompressed() const { return _tableSizeCompressed; }
    std::int32_t tableSize() const { return _tableSize; }

  private:
    void load300(std::ifstream& stream);
    void load200(std::ifstream& stream);
    void load100(std::ifstream& stream);
    void loadTable(std::ifstream& stream, int version);
    void markEncryptionFileRemoved();

    GrfHeader& _header;
    std::vector<FileEntry> _entries;
    mutable std::int32_t _tableSizeCompressed = -1;
    mutable std::int32_t _tableSize = -1;
};

} // namespace grf