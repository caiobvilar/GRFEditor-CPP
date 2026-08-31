#include "Compression.h"
#include "Container.h"
#include "GrfContainerProvider.h"
#include "GrfUtil.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

namespace fs = std::filesystem;

using namespace grf;

namespace {

void usage()
{
    std::cout
        << "grfcl - GRF archive command-line tool\n"
        << "\n"
        << "usage:\n"
        << "  grfcl info <file>                     show header + table info\n"
        << "  grfcl ls <file> [pattern]             list entries (substring, "
           "case-insensitive)\n"
        << "  grfcl extract <file> <path> [-o dir]  decompress one entry to "
           "stdout or -o file\n"
        << "  grfcl extract-all <file> -o dir       decompress every entry "
           "into dir\n"
        << "  grfcl add <file> <path> <src>         add src into the archive "
           "(saves in place)\n"
        << "  grfcl remove <file> <path>            delete an entry (saves in "
           "place)\n"
        << "  grfcl save <file> [out]               repack (defragment); "
           "default writes in place\n"
        << "  grfcl verify <file>                   decompress every entry, "
           "report failures\n";
}

void printEntryInfo(const FileEntry& e)
{
    std::cout << cp1252ToUtf8(e.relativePath) << "  (compressed "
              << e.sizeCompressed << ", aligned " << e.sizeCompressedAlignment
              << ", decompressed " << e.sizeDecompressed << ")\n";
}

int cmdInfo(const std::string& file)
{
    Container c = openContainer(file);
    auto& h = c.header();
    std::cout << "file: " << file << "\n"
              << "format: " << c.formatLabel();
    if (c.readOnly())
        std::cout << " (read-only: converted " << c.formatLabel() << ")";
    std::cout << "\n"
              << "magic: " << cp1252ToUtf8(h.magic) << " (length "
              << h.magic.size() << ")\n"
              << "version: " << (int)h.major << "." << (int)h.minor << "\n"
              << "file table offset: " << h.fileTableOffset << "\n"
              << "entries: " << c.entries().size() << "\n";
    long totalComp = 0, totalDecomp = 0;
    for (const auto& e : c.entries())
    {
        totalComp += std::max(0L, (long)e.sizeCompressedAlignment);
        totalDecomp += std::max(0L, (long)e.sizeDecompressed);
    }
    std::cout << "data compressed: " << totalComp << "\n"
              << "data decompressed: " << totalDecomp << "\n"
              << "wasted space: " << c.getWastedSpace() << "\n";
    return 0;
}

int cmdLs(const std::string& file, const std::string& pattern)
{
    Container c = openContainer(file);
    std::string needle = toLowerAscii(pattern);
    for (const auto& e : c.entries())
    {
        std::string name = cp1252ToUtf8(e.relativePath);
        if (pattern.empty() ||
            toLowerAscii(name).find(needle) != std::string::npos)
        {
            printEntryInfo(e);
        }
    }
    return 0;
}

int cmdExtract(const std::string& file,
               const std::string& archivePath,
               const std::string& out)
{
    Container c = openContainer(file);
    FileEntry* e = c.findCi(archivePath);
    if (!e)
        throw GrfError("Entry not found: " + archivePath);
    Bytes data = c.extract(*e);
    if (!out.empty())
    {
        writeFile(out, data.data(), data.size());
    } else
    {
        std::cout.write((const char*)data.data(), (std::streamsize)data.size());
    }
    return 0;
}

int cmdExtractAll(const std::string& file, const std::string& outDir)
{
    Container c = openContainer(file);
    std::size_t saved = 0;
    for (const auto& e : c.entries())
    {
        Bytes data = c.extract(const_cast<FileEntry&>(e));
        std::string rel = cp1252ToUtf8(e.relativePath);
        for (char& ch : rel)
            if (ch == '\\')
                ch = '/';
        fs::path outPath = fs::path(outDir) / rel;
        fs::create_directories(outPath.parent_path());
        writeFile(outPath.string(), data.data(), data.size());
        saved++;
    }
    std::cout << "extracted " << saved << " entries to " << outDir << "\n";
    return 0;
}

int cmdAdd(const std::string& file,
           const std::string& archivePath,
           const std::string& src)
{
    Container c = openContainer(file);
    c.addFileFromDisk(archivePath, src, true);
    c.save(file);
    std::cout << "added " << src << " -> " << archivePath << " (" << file
              << ")\n";
    return 0;
}

int cmdRemove(const std::string& file, const std::string& archivePath)
{
    Container c = openContainer(file);
    c.removeFile(archivePath);
    c.save(file);
    std::cout << "removed " << archivePath << " (" << file << ")\n";
    return 0;
}

int cmdSave(const std::string& file, const std::string& out)
{
    Container c = openContainer(file);
    std::string target = out.empty() ? file : out;
    c.save(target);
    std::cout << "saved " << target << " (" << c.entries().size()
              << " entries, version " << (int)c.header().major << "."
              << (int)c.header().minor << ")\n";
    return 0;
}

int cmdVerify(const std::string& file)
{
    Container c = openContainer(file);
    std::size_t okCount = 0, failCount = 0;
    for (const auto& e : c.entries())
    {
        try
        {
            Bytes data = c.extract(const_cast<FileEntry&>(e));
            if (data.size() != (std::size_t)e.sizeDecompressed)
                throw GrfError("size mismatch");
            okCount++;
        } catch (const std::exception& ex)
        {
            failCount++;
            std::cerr << "FAIL " << cp1252ToUtf8(e.relativePath) << ": "
                      << ex.what() << "\n";
        }
    }
    std::cout << okCount << " ok, " << failCount << " failed\n";
    return failCount == 0 ? 0 : 1;
}

} // namespace

int main(int argc, char** argv)
{
    try
    {
        if (argc < 2)
        {
            usage();
            return 1;
        }
        std::string cmd = argv[1];

        if (cmd == "info" && argc >= 3)
            return cmdInfo(argv[2]);
        if (cmd == "ls" && argc >= 3)
            return cmdLs(argv[2], argc >= 4 ? argv[3] : "");
        if (cmd == "extract" && argc >= 4)
        {
            std::string out;
            for (std::size_t i = 4; i < (std::size_t)argc; ++i)
            {
                if (std::string(argv[i]) == "-o" && i + 1 < (std::size_t)argc)
                {
                    out = argv[++i];
                }
            }
            return cmdExtract(argv[2], argv[3], out);
        }
        if (cmd == "extract-all" && argc >= 4)
        {
            std::string out = "out";
            for (std::size_t i = 3; i < (std::size_t)argc; ++i)
            {
                if (std::string(argv[i]) == "-o" && i + 1 < (std::size_t)argc)
                {
                    out = argv[++i];
                }
            }
            return cmdExtractAll(argv[2], out);
        }
        if (cmd == "add" && argc >= 5)
            return cmdAdd(argv[2], argv[3], argv[4]);
        if (cmd == "remove" && argc >= 4)
            return cmdRemove(argv[2], argv[3]);
        if (cmd == "save" && argc >= 3)
            return cmdSave(argv[2], argc >= 4 ? argv[3] : "");
        if (cmd == "verify" && argc >= 3)
            return cmdVerify(argv[2]);

        usage();
        return 1;
    } catch (const std::exception& ex)
    {
        std::cerr << "error: " << ex.what() << "\n";
        return 1;
    }
}