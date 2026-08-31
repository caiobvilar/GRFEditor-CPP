#include "FileEntry.h"

#include "Compression.h"
#include "DesCipher.h"

namespace grf {

FileEntry
FileEntry::parseEntry(ByteReader& reader, const GrfHeader& header, int version)
{
    (void)header;
    FileEntry entry;

    // Null-terminated name, no length byte (0.200 / 0.300).
    const byte* start = reader.data() + reader.pos();
    std::size_t nameLen = 0;
    while (reader.canRead() && reader.data()[reader.pos() + nameLen] != 0x00)
        nameLen++;

    entry.relativePath.assign((const char*)start, nameLen);
    reader.forward(nameLen + 1);

    entry.sizeCompressed = reader.i32();
    entry.sizeCompressedAlignment = reader.i32();
    entry.sizeDecompressed = reader.i32();
    entry.flags = reader.u8();

    if (version >= 300)
    {
        std::int64_t off = reader.i64();
        entry.fileExactOffset = entry.temporaryOffset =
            off + GrfHeader::kDataByteSize;
    } else
    {
        std::uint64_t off = reader.u32();
        entry.fileExactOffset = entry.temporaryOffset =
            (std::int64_t)off + GrfHeader::kDataByteSize;
    }

    entry.computeCycleFromFlags();
    return entry;
}

void FileEntry::computeCycleFromFlags()
{
    switch (flags)
    {
    case entry_type::File:
    case entry_type::Directory:
    case entry_type::GravityEncryptedFile:
    default:
        cycle = -1;
        break;
    case entry_type::FileAndHeaderCrypted:
        cycle = 1;
        for (int i = 10; sizeCompressed >= i; i *= 10)
            cycle++;
        break;
    case entry_type::FileAndDataCrypted:
        cycle = 0;
        break;
    }
}

void FileEntry::writeMetadata(ByteWriter& writer, const GrfHeader& header) const
{
    if (isRemoved())
        return;
    if ((flags & entry_type::RemoveFile) != 0)
        return; // overwriteFlags=true semantics (regular GRF save)

    std::int64_t offset = temporaryOffset;

    if (offset < GrfHeader::kDataByteSize)
    {
        offset = fileExactOffset;
        if (offset < GrfHeader::kDataByteSize)
            throw GrfError("Entry data invalid: " + cp1252ToUtf8(relativePath));
    }

    if (header.isMajor(1))
    {
        throw GrfError("GRF 0x100 (version 1) save is not supported yet: " +
                       cp1252ToUtf8(relativePath));
    }

    // 0.200 / 0.300: raw ANSI (CP1252) name, NUL terminated, then fixed fields.
    // relativePath already holds the raw archive bytes.
    writer.raw((const byte*)relativePath.data(), relativePath.size());
    writer.u8(0x00);
    writer.i32(sizeCompressed);
    writer.i32(sizeCompressedAlignment);
    writer.i32(sizeDecompressed);

    std::uint32_t baseFlag = ((flags & entry_type::RawDataFile) != 0)
                                 ? entry_type::Directory
                                 : entry_type::File;

    if ((flags & entry_type::GravityEncryptedFile) != 0)
    {
        writer.u8(entry_type::GravityEncryptedFile);
    } else
    {
        writer.u8((byte)baseFlag);
    }

    if (header.isCompatibleWith(3, 0))
    {
        writer.i64(offset - GrfHeader::kDataByteSize);
    } else
    {
        writer.u32((std::uint32_t)(offset - GrfHeader::kDataByteSize));
    }
}

Bytes FileEntry::getCompressedRaw(std::ifstream& in) const
{
    if (isAdded())
    {
        if (sourceData.size() > 0)
            return sourceData;
        if (!sourceFilePath.empty())
            return readFile(sourceFilePath);
        throw GrfError("Added entry has no data source: " +
                       cp1252ToUtf8(relativePath));
    }

    Bytes data(sizeCompressedAlignment);
    in.clear();
    in.seekg(fileExactOffset, std::ios::beg);
    if (!in.read((char*)data.data(), (std::streamsize)data.size()))
    {
        throw GrfError("Failed to read entry data: " +
                       cp1252ToUtf8(relativePath));
    }
    return data;
}

Bytes FileEntry::getContent(std::ifstream& in, const GrfHeader& header) const
{
    if (sizeDecompressed == 0)
        return {};

    // Converted-container entries carry their already-decompressed payload.
    if (!contentData.empty())
        return contentData;

    if ((flags & entry_type::GravityEncryptedFile) != 0)
        throw GrfError("Gravity-encrypted file cannot be read: " +
                       cp1252ToUtf8(relativePath));

    Bytes data = getCompressedRaw(in);
    data.resize(sizeCompressedAlignment, 0);

    // GrfEditorCrypt (obfuscated kernel in the reference) — deferred.
    bool needsCrypt = ((header.encrypted || header.encryptFileTable) &&
                       (flags & entry_type::GrfEditorCrypted) != 0) ||
                      (flags & entry_type::Decrypt) != 0;
    if (needsCrypt)
    {
        throw GrfError("Entry uses GrfEditorCrypt which is not yet ported: " +
                       cp1252ToUtf8(relativePath));
    }

    if (cycle >= 0)
    {
        descipher::decryptFileData(data.data(), data.size(), cycle == 0, cycle);
    }

    if ((flags & entry_type::LZSS) != 0)
        return lzssDecompress(
            data.data(), data.size(), (std::size_t)sizeDecompressed);

    if ((flags & entry_type::RawDataFile) != 0)
        return rawDecompress(
            data.data(), data.size(), (std::size_t)sizeDecompressed);

    // First byte 0 = LZMA-marker OR custom-compression marker. LZMA is a
    // proprietary DLL in the reference — not portable without the binary.
    if (data.size() > 1 && data[0] == 0)
        throw GrfError("Entry uses LZMA compression which is not yet ported: " +
                       cp1252ToUtf8(relativePath));

    if (data.size() < 1 || data[0] != 0x78)
        throw GrfError("Corrupted or encrypted entry: " +
                       cp1252ToUtf8(relativePath));

    return zlibDecompressExact(
        data.data(), data.size(), (std::size_t)sizeDecompressed);
}

} // namespace grf