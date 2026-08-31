#include "Container.h"

#include "Compression.h"
#include "GrfStrings.h"
#include "GrfWriter.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <system_error>

namespace grf {

namespace {
constexpr const char* kMasterOfMagic = "Master of Magic\0";

std::string canonicalArchivePath(const std::string& utf8Path)
{
    std::string normalized = utf8Path;
    for (char& c : normalized)
        if (c == '/')
            c = '\\';
    return utf8ToCp1252(normalized);
}

std::int32_t swap16Words(std::int32_t v) { return (v << 16) | (v >> 16); }
} // namespace

Container Container::open(const std::string& path)
{
    Container c;
    c._path = path;
    c._stream.open(path, std::ios::binary);
    if (!c._stream)
        throw GrfError("Cannot open file: " + path);

    byte headerBytes[GrfHeader::kDataByteSize];
    c._stream.read((char*)headerBytes, GrfHeader::kDataByteSize);
    if (c._stream.gcount() != GrfHeader::kDataByteSize)
        throw GrfError(
            "File header is invalid, file is smaller than the header size");

    c._header.parse(headerBytes, GrfHeader::kDataByteSize);

    // Magic compare is case-insensitive in the reference.
    std::string magic = toLowerAscii(c._header.magic);
    if (magic != toLowerAscii(std::string(kMasterOfMagic, 16)))
    {
        if (!c.tryAlphaFooter())
            throw GrfError(
                "File header is invalid, expected 'Master of Magic', found '" +
                cp1252ToUtf8(c._header.magic) + "'");
    }

    c._stream.clear();
    c._stream.seekg((std::streamoff)c._header.fileTableOffset +
                        GrfHeader::kDataByteSize,
                    std::ios::beg);
    c._table.load(c._stream);

    // __encryption.info presence -> encrypted container (GrfEditorCrypt).
    for (const auto& e : c._table.entries())
    {
        if (e.relativePath == kEncryptionFilename)
            c._header.encrypted = true;
    }

    return c;
}

Container Container::create(int major, int minor)
{
    Container c;
    c._header.magic.assign(kMasterOfMagic, 16);
    c._header.key.assign((const char*)GrfHeader::defaultKey().data(), 14);
    c._header.major = (std::uint8_t)major;
    c._header.minor = (std::uint8_t)minor;
    c._header.seed = 0;
    c._header.filesCount = 0;
    c._header.realFilesCount = 0;
    c._header.fileTableOffset = 0;
    return c;
}

bool Container::tryAlphaFooter()
{
    _stream.clear();
    _stream.seekg(0, std::ios::end);
    std::streamoff fileSize = _stream.tellg();
    _stream.seekg(-9, std::ios::end);

    std::uint32_t fto = 0;
    std::int32_t realCount = 0;
    byte minor = 0;
    _stream.read((char*)&fto, 4);
    _stream.read((char*)&realCount, 4);
    _stream.read((char*)&minor, 1);

    realCount = swap16Words(realCount);

    if (fto < (std::uint32_t)fileSize)
    {
        _header.magic.assign(kMasterOfMagic, 16);
        _header.key.assign((const char*)GrfHeader::defaultKey().data(), 14);
        _header.major = 0;
        _header.minor = minor;
        _header.seed = 0;
        _header.fileTableOffset = fto;
        _header.realFilesCount = realCount;
        _header.filesCount = realCount;
        return true;
    }
    return false;
}

FileEntry* Container::find(const std::string& pathArchived)
{
    for (auto& e : _table.entries())
        if (e.relativePath == pathArchived)
            return &e;
    return nullptr;
}

const FileEntry* Container::find(const std::string& pathArchived) const
{
    for (const auto& e : _table.entries())
        if (e.relativePath == pathArchived)
            return &e;
    return nullptr;
}

FileEntry* Container::findCi(const std::string& pathUtf8)
{
    std::string target = toLowerAscii(canonicalArchivePath(pathUtf8));
    for (auto& e : _table.entries())
        if (toLowerAscii(e.relativePath) == target)
            return &e;
    return nullptr;
}

Bytes Container::extract(FileEntry& entry)
{
    return entry.getContent(_stream, _header);
}

const char* Container::formatLabel() const
{
    switch (_format)
    {
    case ContainerFormat::Grf:
        return "GRF";
    case ContainerFormat::Thor:
        return "THOR";
    case ContainerFormat::Rgz:
        return "RGZ";
    }
    return "?";
}

void Container::addFileData(const std::string& path,
                            const Bytes& data,
                            bool overwrite)
{
    if (readOnly())
        throw GrfError("Container is read-only (" + std::string(formatLabel()) +
                       "), cannot add files");
    FileEntry e;
    e.relativePath = canonicalArchivePath(path);
    e.flags = entry_type::File;
    e.fileExactOffset = 0;
    e.temporaryOffset = 0;
    e.mod |= modification::Added;
    e.sourceData = data;

    if (FileEntry* existing = find(e.relativePath))
    {
        if (overwrite)
        {
            *existing = std::move(e);
        }
        return;
    }
    _table.entries().push_back(std::move(e));
}

void Container::addFileFromDisk(const std::string& path,
                                const std::string& sourceFile,
                                bool overwrite)
{
    if (readOnly())
        throw GrfError("Container is read-only (" + std::string(formatLabel()) +
                       "), cannot add files");
    FileEntry e;
    e.relativePath = canonicalArchivePath(path);
    e.flags = entry_type::File;
    e.fileExactOffset = 0;
    e.temporaryOffset = 0;
    e.mod |= modification::Added;
    e.sourceFilePath = sourceFile;

    if (FileEntry* existing = find(e.relativePath))
    {
        if (overwrite)
        {
            *existing = std::move(e);
        }
        return;
    }
    _table.entries().push_back(std::move(e));
}

void Container::removeFile(const std::string& path)
{
    if (readOnly())
        throw GrfError("Container is read-only (" + std::string(formatLabel()) +
                       "), cannot remove files");
    FileEntry* e = findCi(path);
    if (!e)
        throw GrfError("File not found: " + path);
    removeEntry(*e);
}

void Container::removeEntry(FileEntry& entry)
{
    entry.mod |= modification::Removed;
}

void Container::save(const std::string& outPath)
{
    if (readOnly())
        throw GrfError("Container is read-only (" + std::string(formatLabel()) +
                       "), cannot save");
    // Write to a temp file first: the read stream may point at outPath itself,
    // and truncating it before the continuous-copy phase would destroy the
    // source data (the reference never reads + truncates the same file).
    std::string tmp = outPath + ".grftmp";
    std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
    if (!out)
        throw GrfError("Cannot write file: " + tmp);

    writeGrf(_header, _table.entries(), _stream, out);
    out.close();

    std::error_code ec;
    std::filesystem::rename(tmp, outPath, ec);
    if (ec)
        throw GrfError("Cannot replace file: " + outPath + " (" + ec.message() +
                       ")");

    if (outPath == _path)
    {
        // Reload from disk so unsaved state and offsets reflect the written
        // file.
        _stream.close();
        _stream.clear();
        _stream.open(_path, std::ios::binary);
        byte headerBytes[GrfHeader::kDataByteSize];
        _stream.read((char*)headerBytes, GrfHeader::kDataByteSize);
        _header.parse(headerBytes, GrfHeader::kDataByteSize);
        _table.entries().clear();
        _stream.clear();
        _stream.seekg((std::streamoff)_header.fileTableOffset +
                          GrfHeader::kDataByteSize,
                      std::ios::beg);
        _table.load(_stream);
    }
}

long Container::getWastedSpace() const
{
    long size = 0;
    std::vector<const FileEntry*> list;
    for (const auto& e : _table.entries())
        if (!e.isRemoved())
            list.push_back(&e);
    std::sort(
        list.begin(), list.end(), [](const FileEntry* a, const FileEntry* b) {
            return a->fileExactOffset < b->fileExactOffset;
        });
    for (std::size_t i = 0; i + 1 < list.size(); ++i)
    {
        size += list[i + 1]->fileExactOffset - list[i]->fileExactOffset -
                list[i]->sizeCompressedAlignment;
    }
    return size;
}

} // namespace grf