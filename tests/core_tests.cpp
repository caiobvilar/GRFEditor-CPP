#include <gtest/gtest.h>

#include "Compression.h"
#include "Container.h"
#include "DesCipher.h"
#include "GrfContainerProvider.h"
#include "GrfHeader.h"
#include "GrfUtil.h"

#include <zlib.h>

#include <cstring>
#include <filesystem>
#include <fstream>

using namespace grf;

namespace {

std::string tempPath(const std::string& name)
{
    auto tmp = std::filesystem::temp_directory_path();
    return (tmp / ("grfcore_" + name)).string();
}

Bytes makeBytes(std::size_t n)
{
    Bytes b(n);
    for (std::size_t i = 0; i < n; ++i)
        b[i] = (byte)((i * 31 + 7) & 0xFF);
    return b;
}

Bytes toBytes(const std::string& s) { return Bytes(s.begin(), s.end()); }

// gzip stream (RFC 1952), what Calendar .rgz files store.
Bytes gzEncode(const Bytes& payload)
{
    z_stream strm{};
    if (deflateInit2(&strm, 6, Z_DEFLATED, 31, 8, Z_DEFAULT_STRATEGY) != Z_OK)
        throw std::runtime_error("deflateInit2 failed");
    Bytes out(compressBound((uLong)payload.size()) + 64);
    strm.next_in = (Bytef*)payload.data();
    strm.avail_in = (uInt)payload.size();
    strm.next_out = out.data();
    strm.avail_out = (uInt)out.size();
    int res = deflate(&strm, Z_FINISH);
    deflateEnd(&strm);
    if (res != Z_STREAM_END)
        throw std::runtime_error("deflate failed");
    out.resize(strm.total_out);
    return out;
}

// Hand-crafted THOR patch (mode 0x30) with two zlib-compressed entries.
Bytes buildThorFile()
{
    const byte* a = (const byte*)"hello thor";
    const byte* b = (const byte*)"v3 data here";
    Bytes da = zlibCompress(a, 10);
    Bytes db = zlibCompress(b, 12);

    ByteWriter table;
    auto addEntry = [&](const std::string& name,
                        std::int32_t offset,
                        const Bytes& data,
                        std::int32_t decomp) {
        table.u8((byte)name.size());
        table.raw((const byte*)name.data(), name.size());
        table.u8(0x00);
        table.i32(offset);
        table.i32((std::int32_t)data.size());
        table.i32(decomp);
    };
    addEntry("a.txt", 40, da, 10);
    addEntry("b\\b.txt", (std::int32_t)(40 + da.size()), db, 12);
    Bytes tComp = zlibCompress(table.bytes().data(), table.bytes().size());

    std::int32_t tableOffset = (std::int32_t)(40 + da.size() + db.size());

    ByteWriter out;
    out.raw((const byte*)"ASSF (C) 2007 Aeomin DEV", 24);
    out.u8(0x00);  // no merging
    out.i32(2);    // number of files
    out.u16(0x30); // mode, LE int16
    out.u8(0x00);  // empty target GRF name
    out.i32((std::int32_t)tComp.size());
    out.i32(tableOffset);
    out.raw(da);
    out.raw(db);
    out.raw(tComp);
    return out.bytes();
}

void makeBigFile(const std::string& path, const Bytes& data)
{
    std::ofstream out(path, std::ios::binary);
    out.write((const char*)data.data(), (std::streamsize)data.size());
}

} // namespace

TEST(grfcore, zlibRoundTrips)
{
    Bytes data = makeBytes(100000);
    data.insert(data.end(), {'h', 'e', 'l', 'l', 'o'});
    Bytes comp = zlibCompress(data.data(), data.size());
    ASSERT_TRUE(comp[0] == 0x78); // zlib header
    Bytes back = zlibDecompressExact(comp.data(), comp.size(), data.size());
    ASSERT_EQ(back, data);
}

TEST(grfcore, zlibFailsOnGarbage)
{
    ASSERT_ANY_THROW(zlibDecompressExact((const byte*)"notzlib!", 8, 64));
}

TEST(grfcore, lzssHandEncodedVector)
{
    // control 0x04 -> bits: 0 (literal A), 0 (literal B), 1 (phrase).
    // codeword 0x0002 -> len = 2, index = 2 -> copies "AB".
    const byte input[] = {0x04, 'A', 'B', 0x02, 0x00};
    Bytes out = lzssDecompress(input, sizeof(input), 4);
    ASSERT_EQ(out, Bytes({'A', 'B', 'A', 'B'}));
}

TEST(grfcore, desBlockCipherIsDeterministic)
{
    byte buf[8] = {0xAB, 0x12, 0x34, 0x56, 0x78, 0x90, 0xDE, 0x00};
    byte before[8];
    std::copy_n(buf, 8, before);
    descipher::decodeFileName(buf, 8);
    ASSERT_FALSE(std::equal(buf, buf + 8, before));
}

TEST(grfcore, headerRoundTrips20)
{
    GrfHeader h;
    h.major = 2;
    h.minor = 0;
    h.fileTableOffset = 1234;
    h.realFilesCount = 5;
    Bytes bytes = h.write();
    ASSERT_EQ(bytes.size(), 46);

    GrfHeader r;
    r.parse(bytes.data(), 46);
    ASSERT_EQ(r.magic, std::string("Master of Magic\0", 16));
    ASSERT_EQ(r.major, 2);
    ASSERT_EQ(r.minor, 0);
    ASSERT_EQ(r.fileTableOffset, 1234);
    ASSERT_EQ(r.realFilesCount, 5);
    ASSERT_TRUE(r.isCompatibleWith(2, 0));
}

TEST(grfcore, headerRoundTrips30Int64)
{
    GrfHeader h;
    h.major = 3;
    h.minor = 0;
    h.fileTableOffset = 2000000000; // > 2^31, only valid with int64 storage
    h.realFilesCount = 42;
    Bytes bytes = h.write();

    GrfHeader r;
    r.parse(bytes.data(), 46);
    ASSERT_EQ(r.major, 3);
    ASSERT_EQ(r.fileTableOffset, 2000000000);
    ASSERT_EQ(r.realFilesCount, 42);
}

TEST(grfcore, cp1252Conversion)
{
    std::string euro = cp1252ToUtf8(std::string("\x80", 1));
    ASSERT_EQ(euro, std::string("\xE2\x82\xAC")); // U+20AC
    std::string back = utf8ToCp1252(euro);
    ASSERT_EQ(back, std::string("\x80", 1));

    std::string aacute = cp1252ToUtf8(std::string("\xE1", 1));
    ASSERT_EQ(aacute, std::string("\xC3\xA1"));
    ASSERT_EQ(utf8ToCp1252(aacute), std::string("\xE1", 1));
}

TEST(grfcore, v2ContainerRoundTrip)
{
    std::string path = tempPath("rt_v2.grf");
    {
        Container c = Container::create(2, 0);
        c.addFileData("data\\test\\hello.txt",
                      Bytes({'h', 'e', 'l', 'l', 'o'}));
        Bytes big = makeBytes(50000);
        c.addFileData("data\\test\\big.bin", big);
        c.save(path);
    }
    {
        Container c = Container::open(path);
        ASSERT_EQ(c.header().major, 2);
        ASSERT_EQ(c.entries().size(), 2);

        FileEntry* e = c.find("data\\test\\hello.txt");
        ASSERT_NE(e, nullptr);
        ASSERT_EQ(c.extract(*e), Bytes({'h', 'e', 'l', 'l', 'o'}));

        FileEntry* big = c.findCi("DATA/TEST/BIG.BIN");
        ASSERT_NE(big, nullptr);
        ASSERT_EQ(c.extract(*big), makeBytes(50000));
    }
    std::filesystem::remove(path);
}

TEST(grfcore, v3ContainerRoundTrip)
{
    std::string path = tempPath("rt_v3.grf");
    {
        Container c = Container::create(3, 0);
        c.addFileData("data\\readme.txt", toBytes("v3"));
        c.save(path);
    }
    {
        Container c = Container::open(path);
        ASSERT_EQ(c.header().major, 3);
        ASSERT_EQ(c.entries().size(), 1);
        FileEntry* e = c.find("data\\readme.txt");
        ASSERT_NE(e, nullptr);
        ASSERT_EQ(c.extract(*e), toBytes("v3"));
    }
    std::filesystem::remove(path);
}

TEST(grfcore, emptyContainer)
{
    std::string path = tempPath("empty.grf");
    {
        Container c = Container::create(2, 0);
        c.save(path);
    }
    Container c = Container::open(path);
    ASSERT_TRUE(c.entries().empty());
    std::filesystem::remove(path);
}

TEST(grfcore, removeThenSaveDropsEntry)
{
    std::string path = tempPath("remove.grf");
    {
        Container c = Container::create(2, 0);
        c.addFileData("data\\a.txt", toBytes("a"));
        c.addFileData("data\\b.txt", toBytes("b"));
        c.save(path);
    }
    {
        Container c = Container::open(path);
        c.removeFile("data/a.txt");
        c.save(path);
    }
    {
        Container c = Container::open(path);
        ASSERT_EQ(c.entries().size(), 1);
        ASSERT_NE(c.find("data\\b.txt"), nullptr);
        ASSERT_EQ(c.findCi("data/a.txt"), nullptr);
    }
    std::filesystem::remove(path);
}

TEST(grfcore, rawBytesOfWrittenV2Grf)
{
    std::string path = tempPath("rawformat.grf");
    {
        Container c = Container::create(2, 0);
        c.addFileData("file.bin", Bytes({1, 2, 3}));
        c.save(path);
    }
    Bytes raw = readFile(path);
    ASSERT_GE(raw.size(), 46);

    ASSERT_EQ(std::string((const char*)raw.data(), 16),
              std::string("Master of Magic\0", 16));
    ASSERT_EQ(std::string((const char*)raw.data() + 16, 14),
              std::string((const char*)GrfHeader::defaultKey().data(), 14));
    ASSERT_EQ(le32(raw.data() + 42), 0x0200);
    ASSERT_TRUE(raw[46] ==
                0x78); // zlib stream of "file.bin" right after the header

    std::filesystem::remove(path);
}

TEST(grfcore, providerDispatchByExtension)
{
    std::string path = tempPath("prov.grf");
    {
        Container c = Container::create(2, 0);
        c.addFileData("x.txt", toBytes("x"));
        c.save(path);
    }
    Container c = openContainer(path);
    ASSERT_EQ(c.entries().size(), 1);
    ASSERT_EQ(c.format(), ContainerFormat::Grf);
    ASSERT_FALSE(c.readOnly());

    std::string thorPath = tempPath("prov.thor");
    makeBigFile(thorPath, buildThorFile());
    Container t = openContainer(thorPath);
    ASSERT_EQ(t.format(), ContainerFormat::Thor);
    ASSERT_TRUE(t.readOnly());
    ASSERT_EQ(t.entries().size(), 2);

    std::filesystem::remove(path);
    std::filesystem::remove(thorPath);
}

TEST(grfcore, thorMode0x30ParsesAndExtracts)
{
    std::string path = tempPath("sample.thor");
    makeBigFile(path, buildThorFile());

    Container c = Container::openThor(path);
    ASSERT_EQ(c.format(), ContainerFormat::Thor);
    ASSERT_TRUE(c.readOnly());
    ASSERT_EQ(c.entries().size(), 2);

    FileEntry* a = c.find("a.txt");
    ASSERT_NE(a, nullptr);
    ASSERT_EQ(c.extract(*a), toBytes("hello thor"));

    FileEntry* b = c.find("b\\b.txt");
    ASSERT_NE(b, nullptr);
    ASSERT_EQ(c.extract(*b), toBytes("v3 data here"));

    // read-only guards
    ASSERT_THROW(c.addFileData("x", toBytes("x")), GrfError);
    ASSERT_THROW(c.removeFile("a.txt"), GrfError);
    ASSERT_THROW(c.save(path), GrfError);

    std::filesystem::remove(path);
}

TEST(grfcore, thorMode0x21SingleFile)
{
    const char* name = "patch.exe";
    Bytes data = zlibCompress((const byte*)"patched exe bytes", 17);

    ByteWriter out;
    out.raw((const byte*)"ASSF (C) 2007 Aeomin DEV", 24);
    out.u8(0x00);
    out.i32(1);
    out.u16(0x21);
    out.u8(0x00); // target
    out.u8(0x00); // unused byte
    // file table offset = position here = 24+1+4+2+1+1 = 33
    out.i32((std::int32_t)data.size());
    out.i32(17);
    out.u8((byte)std::strlen(name));
    out.raw((const byte*)name, std::strlen(name));
    out.raw(data);
    out.raw((const byte*)"SEALED!", 7);

    std::string path = tempPath("single.thor");
    makeBigFile(path, out.bytes());

    Container c = Container::openThor(path);
    ASSERT_EQ(c.entries().size(), 1);
    FileEntry* e = c.find("patch.exe");
    ASSERT_NE(e, nullptr);
    ASSERT_EQ(c.extract(*e), toBytes("patched exe bytes"));

    std::filesystem::remove(path);
}

TEST(grfcore, rgzParsesAndExtracts)
{
    Bytes big = makeBytes(70000);

    std::string path = tempPath("sample.rgz");
    // 'f' record and 'd' record, then a second 'f' record, gzip-wrapped.
    ByteWriter p2;
    p2.raw((const byte*)"f", 1);
    std::string bn = "data\\big.bin";
    p2.u8((byte)bn.size());
    p2.raw((const byte*)bn.data(), bn.size());
    p2.i32((std::int32_t)big.size());
    p2.raw(big);

    ByteWriter final;
    final.raw((const byte*)"f", 1);
    std::string hname = "hello.txt";
    final.u8((byte)hname.size());
    final.raw((const byte*)hname.data(), hname.size());
    final.i32(5);
    final.raw((const byte*)"hello", 5);
    final.raw((const byte*)"d", 1);
    std::string dname = "root\\";
    final.u8((byte)dname.size());
    final.raw((const byte*)dname.data(), dname.size());
    final.raw(p2.bytes());

    makeBigFile(path, gzEncode(final.bytes()));

    Container c = openContainer(path);
    ASSERT_EQ(c.format(), ContainerFormat::Rgz);
    ASSERT_TRUE(c.readOnly());
    ASSERT_EQ(c.entries().size(), 2);

    FileEntry* h = c.find("hello.txt");
    ASSERT_NE(h, nullptr);
    ASSERT_EQ(c.extract(*h), toBytes("hello"));

    FileEntry* b = c.find("data\\big.bin");
    ASSERT_NE(b, nullptr);
    ASSERT_EQ(c.extract(*b), big);

    ASSERT_THROW(c.save(path), GrfError);

    std::filesystem::remove(path);
}