#include "Container.h"

#include "Compression.h"

#include <string>

namespace grf {

namespace {
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
} // namespace

Container Container::openRgz(const std::string& path)
{
    Bytes file = readFile(path);
    Bytes decomp = gzDecompress(file.data(), file.size());

    Container c;
    c._format = ContainerFormat::Rgz;
    c._path = path;
    fillConvertedHeader(c._header);

    ByteReader r(decomp.data(), decomp.size());
    while (r.canRead())
    {
        char type = (char)r.u8();
        if (type == 'f')
        {
            std::uint8_t nameLen = r.u8();
            std::string name((const char*)(r.data() + r.pos()), nameLen);
            r.forward(nameLen);
            std::size_t nul = name.find('\0'); // trailing NUL padding
            if (nul != std::string::npos)
                name = name.substr(0, nul);

            std::int32_t length = r.i32();
            if (length < 0 || (std::size_t)length > r.remaining())
                throw GrfError("Corrupted RGZ entry length");

            FileEntry e;
            e.flags = entry_type::File;
            e.relativePath = name;
            e.sizeCompressed = length;
            e.sizeCompressedAlignment = length;
            e.sizeDecompressed = length;
            e.contentData.assign(r.data() + r.pos(),
                                 r.data() + r.pos() + length);
            r.forward(length);
            c._table.entries().push_back(std::move(e));
        } else if (type == 'd')
        {
            r.forward(r.u8()); // directory entry: skip its name
        } else if (type == 'e')
        {
            // end marker: nothing to do, keep scanning (matches reference)
        } else
        {
            break; // unknown record type: stop
        }
    }

    c._header.filesCount = c._header.realFilesCount =
        (std::int32_t)c._table.entries().size();
    return c;
}

} // namespace grf