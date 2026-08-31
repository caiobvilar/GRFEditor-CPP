#pragma once

#include "FileEntry.h"
#include "FileTable.h"
#include "GrfHeader.h"

#include <fstream>
#include <string>
#include <vector>

namespace grf {

enum class ContainerFormat { Grf, Thor, Rgz };

// An opened GRF/GPF archive. Mirrors GRF/Core/Container.cs: holds the file
// stream open for lazy entry reads and the parsed header + table. Thor and RGZ
// archives are converted to this representation on open (matching the
// reference) and are read-only.
class Container
{
  public:
    Container() : _table(_header) {}

    static Container open(const std::string& path);
    // Converted-from-Thor / converted-from-RGZ containers (read-only), defined
    // in Thor.cpp / Rgz.cpp.
    static Container openThor(const std::string& path);
    static Container openRgz(const std::string& path);
    // Fresh container with a given major/minor version (default 2.0).
    static Container create(int major = 2, int minor = 0);

    // --- inspection ---
    ContainerFormat format() const { return _format; }
    bool readOnly() const { return _format != ContainerFormat::Grf; }
    const char* formatLabel() const;
    GrfHeader& header() { return _header; }
    const std::vector<FileEntry>& entries() const { return _table.entries(); }
    FileEntry* find(const std::string& pathArchived);
    const FileEntry* find(const std::string& pathArchived) const;
    // Case-insensitive lookup over raw archive bytes.
    FileEntry* findCi(const std::string& pathUtf8);

    // Decompressed content of an entry (extract path).
    Bytes extract(FileEntry& entry);

    // --- mutation ---
    // path uses forward OR back slashes; stored as CP1252 bytes with '\'.
    void addFileData(const std::string& path,
                     const Bytes& data,
                     bool overwrite = true);
    void addFileFromDisk(const std::string& path,
                         const std::string& sourceFile,
                         bool overwrite = true);
    void removeFile(const std::string& path);
    void removeEntry(FileEntry& entry);

    // Repack to outPath (may equal the open path).
    void save(const std::string& outPath);

    // Wasted space between consecutive entry payloads (reference
    // GetWastedSpace).
    long getWastedSpace() const;

  private:
    bool tryAlphaFooter();

    std::string _path;
    std::ifstream _stream;
    GrfHeader _header;
    FileTable _table;
    ContainerFormat _format = ContainerFormat::Grf;
};

} // namespace grf