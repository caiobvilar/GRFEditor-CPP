#include "FileTable.h"

#include "Compression.h"
#include "DesCipher.h"

#include <algorithm>
#include <cstring>

namespace grf {

namespace {

constexpr const char* kEncryptionFilename = "__encryption.info";

// "\\\\" -> "\\" rename detection from the reference table loaders.
void applyRenameFixup(FileEntry& entry)
{
    auto idx = entry.relativePath.find("\\\\");
    if (idx != std::string::npos)
    {
        entry.mod |= modification::FileNameRenamed;
        std::string s;
        s.reserve(entry.relativePath.size());
        for (std::size_t i = 0; i < entry.relativePath.size();)
        {
            if (entry.relativePath[i] == '\\' &&
                i + 1 < entry.relativePath.size() &&
                entry.relativePath[i + 1] == '\\')
            {
                s += '\\';
                i += 2;
            } else
            {
                s += entry.relativePath[i];
                ++i;
            }
        }
        entry.relativePath = s;
    }
}

// Same overwrite-semantics as _indexedEntries[] in the reference: on conflict,
// a FileNameRenamed supersedes the previous entry.
void indexInsert(std::vector<FileEntry>& entries, FileEntry&& entry)
{
    for (auto& existing : entries)
    {
        if (existing.relativePath == entry.relativePath)
        {
            if (existing.isRenamed())
            {
                existing = std::move(entry);
            }
            return;
        }
    }
    entries.push_back(std::move(entry));
}

} // namespace

void FileTable::load(std::ifstream& stream)
{
    GrfHeader& h = _header;

    if (h.isCompatibleWith(3, 0))
        load300(stream);
    else if (h.isCompatibleWith(2, 0))
        load200(stream);
    else if (h.isCompatibleWith(1, 0))
        load100(stream);
    else if (h.isCompatibleWith(0, 18))
        throw GrfError("Alpha GRF (version 0.18) is not supported");
    else
        throw GrfError("Unsupported file version");
}

void FileTable::load300(std::ifstream& stream)
{
    byte skip[4];
    stream.read((char*)skip, 4); // "Unknown, always 0?"
    _tableSizeCompressed = 0;
    stream.read((char*)&_tableSizeCompressed, 4);
    _tableSize = 0;
    stream.read((char*)&_tableSize, 4);

    if (_tableSizeCompressed == 0 || _tableSize == 0)
        return;

    loadTable(stream, 300);
}

void FileTable::load200(std::ifstream& stream)
{
    stream.read((char*)&_tableSizeCompressed, 4);
    stream.read((char*)&_tableSize, 4);

    if (_tableSizeCompressed == 0 || _tableSize == 0)
        return;

    loadTable(stream, 200);
}

void FileTable::loadTable(std::ifstream& stream, int version)
{
    if (_tableSizeCompressed <= 0 || _tableSize <= 0)
        return;

    Bytes compressed((std::size_t)_tableSizeCompressed);
    if (!stream.read((char*)compressed.data(),
                     (std::streamsize)compressed.size()))
    {
        throw GrfError("File table is truncated");
    }

    Bytes data;
    try
    {
        data = zlibDecompressExact(
            compressed.data(), compressed.size(), (std::size_t)_tableSize);
    } catch (const GrfError&)
    {
        // The obfuscated reference detects file-table encryption here
        // (Ee322.a184...). GrfEditorCrypt is not yet ported.
        if (_header.encrypted)
            throw GrfError(
                "File table is GrfEditor-encrypted (not yet supported)");
        throw;
    }

    ByteReader reader(data);
    while (reader.canRead())
    {
        FileEntry entry = FileEntry::parseEntry(reader, _header, version);
        applyRenameFixup(entry);

        if (entry.flags == entry_type::Directory)
        {
            if (entry.relativePath.find('.') == std::string::npos)
                continue;
            entry.flags |= entry_type::RawDataFile;
        }

        if ((entry.flags & (entry_type::File | entry_type::RawDataFile |
                            entry_type::GravityEncryptedFile)) != 0)
        {
            indexInsert(_entries, std::move(entry));
        }
    }

    markEncryptionFileRemoved();
}

void FileTable::markEncryptionFileRemoved()
{
    for (auto& e : _entries)
    {
        if (e.relativePath == kEncryptionFilename)
            e.mod |= modification::Removed;
    }
}

void FileTable::load100(std::ifstream& stream)
{
    stream.clear();
    stream.seekg((std::streamoff)_header.fileTableOffset +
                     GrfHeader::kDataByteSize,
                 std::ios::end);

    std::streamoff fileSize = stream.tellg();
    std::streamoff listStart =
        (std::streamoff)_header.fileTableOffset + GrfHeader::kDataByteSize;
    std::streamoff listSize = fileSize - listStart;

    Bytes fileListData((std::size_t)listSize);
    stream.seekg(listStart, std::ios::beg);
    if (listSize > 0)
        stream.read((char*)fileListData.data(), listSize);

    int filelistEntries = _header.realFilesCount;

    // First pass: count directories.
    int directoryIndexCount = 0;
    for (int entry = 0, offset = 0; entry < filelistEntries; entry++)
    {
        int offset2 =
            offset + (int)le32(fileListData.data() + (std::size_t)offset) + 4;
        byte entryType = fileListData[(std::size_t)offset2 + 12];

        if (entryType == 0) // EntryType.Directory
            directoryIndexCount++;

        offset = offset2 + 17;
    }

    _header.realFilesCount = filelistEntries - directoryIndexCount;
    _header.filesCount = _header.realFilesCount;

    int filesCount = 0;

    for (int entry = 0, offset = 0; entry < filelistEntries; entry++)
    {
        byte listOffsetValue = fileListData[(std::size_t)offset];
        int offset2 =
            offset + (int)le32(fileListData.data() + (std::size_t)offset) + 4;
        byte entryType = fileListData[(std::size_t)offset2 + 12];

        int length = listOffsetValue - 6;
        if (length < 0)
            throw GrfError("Invalid 0x100 entry (negative name length)");

        Bytes fileName(length);
        std::memcpy(fileName.data(),
                    fileListData.data() + (std::size_t)offset + 6,
                    (std::size_t)length);
        descipher::decodeFileName(fileName.data(), fileName.size());

        // Name = ANSI bytes -> string, truncated at the first NUL.
        std::string name((const char*)fileName.data(), fileName.size());
        auto nul = name.find('\0');
        if (nul == std::string::npos)
        {
            throw GrfError("Failed null string in 0x100 entry");
        }
        name = name.substr(0, nul);
        if (name.find('?') != std::string::npos)
        {
            throw GrfError("File name is wrongly encoded: " + name);
        }

        if (entryType == 0)
        { // Directory
            if (name.find('.') == std::string::npos)
            {
                offset = offset2 + 17;
                continue;
            }
            entryType = (byte)(entryType | entry_type::RawDataFile);
        }

        int compressedLenAligned =
            le32s(fileListData.data() + (std::size_t)offset2 + 4) - 37579;
        int realLen = le32s(fileListData.data() + (std::size_t)offset2 + 8);
        std::uint32_t pos =
            le32(fileListData.data() + (std::size_t)offset2 + 13);

        int cycle = 0;
        int compressedLen = 0;

        std::string ext;
        if (name.size() > 4)
        {
            ext = name.substr(name.size() - 4);
            compressedLen =
                le32s(fileListData.data() + (std::size_t)offset2) -
                le32s(fileListData.data() + (std::size_t)offset2 + 8) - 715;

            ext = toLowerAscii(ext);
            if (ext != ".gnd" && ext != ".gat" && ext != ".act" &&
                ext != ".str")
            {
                cycle = 1;
                for (int i = 10; compressedLen >= i; i *= 10)
                    cycle++;
            }
        }

        FileEntry fileEntry;
        fileEntry.cycle = cycle;
        fileEntry.sizeCompressed = fileEntry.sizeCompressedAlignment =
            compressedLen;
        fileEntry.sizeCompressedAlignment = compressedLenAligned;
        fileEntry.sizeDecompressed = realLen;
        fileEntry.fileExactOffset = fileEntry.temporaryOffset =
            (std::int64_t)pos + GrfHeader::kDataByteSize;
        fileEntry.flags = entryType;
        fileEntry.relativePath = name;
        fileEntry.computeCycleFromFlags();

        applyRenameFixup(fileEntry);
        indexInsert(_entries, std::move(fileEntry));

        offset = offset2 + 17;
        filesCount++;
    }

    _header.realFilesCount = filesCount;
    _header.filesCount = filesCount;

    markEncryptionFileRemoved();
}

Bytes FileTable::writeTable(GrfHeader& header) const
{
    ByteWriter table;
    for (const auto& entry : _entries)
    {
        entry.writeMetadata(table, header);
    }
    _tableSize = (std::int32_t)table.size();
    Bytes compressed = zlibCompress(table.bytes().data(), table.bytes().size());
    _tableSizeCompressed = (std::int32_t)compressed.size();

    ByteWriter block;
    if (header.isCompatibleWith(3, 0))
    {
        block.zero(4);
    }
    block.i32(_tableSizeCompressed);
    block.i32(_tableSize);
    block.raw(compressed);
    return block.bytes();
}

} // namespace grf