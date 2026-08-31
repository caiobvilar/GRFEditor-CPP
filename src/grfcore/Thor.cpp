#include "Container.h"

#include "Compression.h"

#include <string>

namespace grf {

namespace {

constexpr const char* kThorMagic = "ASSF (C) 2007 Aeomin DEV"; // 24 chars
constexpr int kThorMagicLen = 24;

struct ThorHeader
{
    bool useGrfMerging = false;
    std::int32_t numberOfFiles = 0;
    std::int16_t mode = 0;
    std::string targetGrf;
    std::int32_t fileTableCompressedLength = 0;
    std::int32_t fileTableOffset = 0;
};

ThorHeader readThorHeader(ByteReader& r, std::size_t fileSize)
{
    ThorHeader h;

    std::string magic(kThorMagicLen, '\0');
    r.bytes((byte*)magic.data(), kThorMagicLen);
    if (magic != kThorMagic)
        throw GrfError("Invalid THOR header, expected '" +
                       std::string(kThorMagic) + "'");

    h.useGrfMerging = r.u8() != 0;
    h.numberOfFiles = r.i32();
    h.mode = (std::int16_t)r.u16();

    switch (h.mode)
    {
    case 0x30: {
        std::uint8_t len = r.u8();
        h.targetGrf.assign((const char*)(r.data() + r.pos()), len);
        r.forward(len);
        h.fileTableCompressedLength = r.i32();
        h.fileTableOffset = r.i32();
        break;
    }
    case 0x21: {
        std::uint8_t len = r.u8();
        h.targetGrf.assign((const char*)(r.data() + r.pos()), len);
        r.forward(len);
        r.forward(1); // unused byte
        h.fileTableOffset = (std::int32_t)r.pos();
        break;
    }
    default:
        throw GrfError("Unsupported THOR mode: " + std::to_string(h.mode));
    }

    if (h.fileTableOffset < 0 || (std::size_t)h.fileTableOffset > fileSize)
        throw GrfError(
            "Corrupted THOR header: file table offset out of bounds");

    return h;
}

void fillConvertedHeader(GrfHeader& header)
{
    header.magic.assign("Master of Magic", 16);
    header.key.assign((const char*)GrfHeader::defaultKey().data(), 14);
    header.major = 2;
    header.minor = 0;
    header.seed = 0;
    header.filesCount = 0;
    header.realFilesCount = 0;
    header.fileTableOffset = 0;
    header.encrypted = false;
}

FileEntry makeEntry(const std::string& name,
                    std::int32_t offset,
                    std::int32_t sizeComp,
                    std::int32_t sizeDecomp)
{
    FileEntry e;
    e.flags = entry_type::File;
    e.relativePath = name;
    e.fileExactOffset = offset;
    e.sizeCompressed = sizeComp;
    e.sizeCompressedAlignment = sizeComp;
    e.sizeDecompressed = sizeDecomp;
    e.cycle = -1;
    return e;
}

} // namespace

Container Container::openThor(const std::string& path)
{
    Bytes file = readFile(path);
    ByteReader reader(file.data(), file.size());

    ThorHeader h;
    try
    {
        h = readThorHeader(reader, file.size());
    } catch (const GrfError&)
    {
        throw GrfError(
            "Not a valid THOR patcher file (or unsupported variant): " + path);
    }

    Container c;
    c._format = ContainerFormat::Thor;
    c._path = path;
    c._stream.open(path, std::ios::binary);
    if (!c._stream)
        throw GrfError("Cannot open file: " + path);
    fillConvertedHeader(c._header);

    if (h.mode == 0x30)
    {
        if (h.fileTableCompressedLength < 0 ||
            (std::size_t)(h.fileTableOffset + h.fileTableCompressedLength) >
                file.size())
            throw GrfError("Corrupted THOR file table bounds");

        Bytes table = zlibDecompress(file.data() + h.fileTableOffset,
                                     h.fileTableCompressedLength);
        ByteReader t(table.data(), table.size());

        while (t.canRead())
        {
            std::uint8_t nameLen = t.u8();
            std::string name((const char*)(t.data() + t.pos()), nameLen);
            t.forward(nameLen);
            std::uint8_t flags = t.u8();

            FileEntry e;
            if ((flags & 0x01) == 0x01)
            {
                e.flags = entry_type::File | entry_type::RemoveFile;
                e.relativePath = name;
                c._table.entries().push_back(std::move(e));
                continue;
            }

            std::int32_t offset = t.i32();
            std::int32_t sizeComp = t.i32();
            std::int32_t sizeDecomp = t.i32();
            c._table.entries().push_back(
                makeEntry(name, offset, sizeComp, sizeDecomp));
        }
    } else
    {
        // Mode 0x21: single patched file (patcher/game EXE), SEALED! block.
        ByteReader t(file.data() + h.fileTableOffset,
                     file.size() - h.fileTableOffset);
        std::int32_t sizeComp = t.i32();
        std::int32_t sizeDecomp = t.i32();
        std::uint8_t nameLen = t.u8();
        std::string name((const char*)(t.data() + t.pos()), nameLen);
        t.forward(nameLen);
        c._table.entries().push_back(
            makeEntry(name,
                      h.fileTableOffset + (std::int32_t)t.pos(),
                      sizeComp,
                      sizeDecomp));
    }

    c._header.filesCount = c._header.realFilesCount =
        (std::int32_t)c._table.entries().size();
    return c;
}

} // namespace grf