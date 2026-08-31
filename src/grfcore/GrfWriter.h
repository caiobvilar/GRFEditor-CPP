#pragma once

#include "FileEntry.h"
#include "GrfHeader.h"

#include <fstream>
#include <ostream>
#include <vector>

namespace grf {

// Single-threaded repack, mirroring GrfWriter.WriteData / WriteRepack:
// content data occupies [46 .. 46+dataLen), the header is written at 0 and the
// compressed file table right after the data region.
//
// Unsupported writers (0x100 content DES encrypt, LZMA, GrfEditorCrypt) throw.
void writeGrf(GrfHeader& header,
              std::vector<FileEntry>& entries,
              std::ifstream& original,
              std::ostream& out);

} // namespace grf