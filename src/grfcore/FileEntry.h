#pragma once

#include "GrfHeader.h"
#include "GrfUtil.h"

#include <fstream>
#include <string>

namespace grf {

// A single file table entry. Mirrors GRF/Core/FileEntry.cs.
struct FileEntry
{
    std::string
        relativePath; // raw bytes as stored (CP1252), backslash separated
    std::uint32_t flags = entry_type::File;
    std::int32_t sizeCompressed = 0; // actual compressed length
    std::int32_t sizeCompressedAlignment =
        0; // aligned (multiple of 8), on-disk length
    std::int32_t sizeDecompressed = 0;
    std::int64_t fileExactOffset =
        0; // absolute offset in the archive (raw + DataByteSize)
    std::int64_t temporaryOffset = 0; // output offset assigned during save
    std::int32_t cycle = -1;

    // session modifications (Modification enum)
    std::uint32_t mod = modification::None;

    // added-from-disk or added-from-stream payload
    std::string sourceFilePath; // "\...[data stream]" when in-memory
    Bytes sourceData; // in-memory payload for entries added from a stream

    // Converted containers (RGZ) hold the decompressed bytes directly instead
    // of pointing into an on-disk stream.
    Bytes contentData;

    bool isAdded() const { return (mod & modification::Added) != 0; }
    bool isRemoved() const { return (mod & modification::Removed) != 0; }
    bool isRenamed() const
    {
        return (mod & modification::FileNameRenamed) != 0;
    }

    // Parse a single entry from the already-decompressed table buffer.
    // version: 200 or 300 (0x100 entries are handled by FileTable::load100).
    static FileEntry
    parseEntry(ByteReader& reader, const GrfHeader& header, int version);

    // Compute the content "Cycle" from flags + compressed size (parse-time).
    void computeCycleFromFlags();

    // Serialize this entry into a 0x200 / 0x300 table buffer.
    // Throws for 0x100 targets (see GrfWriter).
    void writeMetadata(ByteWriter& writer, const GrfHeader& header) const;

    // Compressed content the way it is stored on disk (pre-save: for added
    // entries, the source payload).
    Bytes getCompressedRaw(std::ifstream& in) const;

    // Fully decompressed content (the extract path). Mirrors
    // FileEntry._getDecompressedData().
    Bytes getContent(std::ifstream& in, const GrfHeader& header) const;
};

} // namespace grf