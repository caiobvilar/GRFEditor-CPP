#include "GrfWriter.h"

#include "Compression.h"
#include "FileTable.h"

#include <algorithm>

namespace grf {

void writeGrf(GrfHeader& header,
              std::vector<FileEntry>& entries,
              std::ifstream& original,
              std::ostream& out)
{
    if (header.isMajor(1))
        throw GrfError("GRF 0x100 (version 1) save is not supported yet");

    // Mirror _getEntries: only non-removed entries are written, in data order.
    std::vector<FileEntry*> ordered;
    for (auto& e : entries)
    {
        if (e.isRemoved())
            continue;
        if ((e.flags & entry_type::RemoveFile) != 0)
            continue;
        ordered.push_back(&e);
    }
    std::stable_sort(ordered.begin(),
                     ordered.end(),
                     [](const FileEntry* a, const FileEntry* b) {
                         return a->fileExactOffset < b->fileExactOffset;
                     });

    long offset = GrfHeader::kDataByteSize;

    for (FileEntry* e : ordered)
    {
        if (e->isAdded())
        {
            // NewCompressedData (reference): raw source -> zlib stream ->
            // aligned.
            Bytes raw = e->sourceData.size() > 0 ? e->sourceData
                                                 : readFile(e->sourceFilePath);
            Bytes comp = zlibCompress(raw.data(), raw.size());
            e->sizeDecompressed = (std::int32_t)raw.size();
            e->sizeCompressed = (std::int32_t)comp.size();
            e->sizeCompressedAlignment =
                (std::int32_t)align8((long)comp.size());
            e->temporaryOffset = offset;

            out.seekp(offset);
            out.write((const char*)comp.data(), (std::streamsize)comp.size());
            for (long i = (long)comp.size(); i < e->sizeCompressedAlignment;
                 ++i)
                out.put(0);
            offset += e->sizeCompressedAlignment;
        } else
        {
            // Continuous copy: the aligned bytes, raw from the original stream.
            Bytes data = e->getCompressedRaw(original);
            data.resize((std::size_t)e->sizeCompressedAlignment, 0);
            e->temporaryOffset = offset;

            out.seekp(offset);
            out.write((const char*)data.data(), (std::streamsize)data.size());
            offset += e->sizeCompressedAlignment;
        }
    }

    long fto = offset - GrfHeader::kDataByteSize;
    header.fileTableOffset = fto;
    header.realFilesCount = (std::int32_t)ordered.size();

    // Header first, then the table block right after the data region.
    Bytes headerBytes = header.write();
    out.seekp(0);
    out.write((const char*)headerBytes.data(),
              (std::streamsize)headerBytes.size());

    FileTable table(header);
    table.entries() = entries;
    Bytes tableBlock = table.writeTable(header);

    out.seekp(offset);
    out.write((const char*)tableBlock.data(),
              (std::streamsize)tableBlock.size());
    out.flush();
}

} // namespace grf