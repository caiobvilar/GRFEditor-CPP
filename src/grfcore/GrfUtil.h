#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

namespace grf {

using byte = std::uint8_t;
using Bytes = std::vector<byte>;

// ---------------------------------------------------------------------------
// Little-endian primitives (GRF is a LE format; matches C# BitConverter on x86)
// ---------------------------------------------------------------------------
inline std::uint16_t le16(const byte* p)
{
    return (std::uint16_t)(p[0] | ((std::uint16_t)p[1] << 8));
}

inline std::uint32_t le32(const byte* p)
{
    return (std::uint32_t)p[0] | ((std::uint32_t)p[1] << 8) |
           ((std::uint32_t)p[2] << 16) | ((std::uint32_t)p[3] << 24);
}

inline std::int32_t le32s(const byte* p) { return (std::int32_t)le32(p); }

inline std::uint64_t le64(const byte* p)
{
    std::uint64_t v = 0;
    for (int i = 7; i >= 0; --i)
        v = (v << 8) | p[i];
    return v;
}

inline std::int64_t le64s(const byte* p) { return (std::int64_t)le64(p); }

inline void put16(byte* p, std::uint16_t v)
{
    p[0] = (byte)v;
    p[1] = (byte)(v >> 8);
}

inline void put32(byte* p, std::uint32_t v)
{
    p[0] = (byte)v;
    p[1] = (byte)(v >> 8);
    p[2] = (byte)(v >> 16);
    p[3] = (byte)(v >> 24);
}

inline void put64(byte* p, std::uint64_t v)
{
    for (int i = 0; i < 8; ++i)
        p[i] = (byte)(v >> (8 * i));
}

// Methods.Align in the C# reference: round UP to a multiple of 8.
inline long align8(long v) { return (v + 7) & ~(long)7; }

// ---------------------------------------------------------------------------
// EntryType / Modification flags (byte entries flag distinguishes the codec)
// ---------------------------------------------------------------------------
namespace entry_type {
enum : std::uint32_t {
    Directory = 0,
    File = 1u << 0,                 // 0x01
    HeaderCrypted = 1u << 1,        // 0x02
    DataCrypted = 1u << 2,          // 0x04
    RemoveFile = 1u << 4,           // 0x10
    GrfEditorCrypted = 1u << 5,     // 0x20
    GravityEncryptedFile = 1u << 7, // 0x80
    Decrypt = 1u << 8,              // 0x100
    FileNameRenamed = 1u << 9,      // 0x200
    LzmaCompressed = 1u << 10,      // 0x400
    RawDataFile = 1u << 11,         // 0x800
    LZSS = 1u << 12,                // 0x1000

    FileAndHeaderCrypted = File | HeaderCrypted, // 0x03
    FileAndDataCrypted = File | DataCrypted,     // 0x05
};
}

namespace modification {
enum : std::uint32_t {
    None = 0,
    Removed = 1u << 1,         // 0x02
    Added = 1u << 2,           // 0x04
    FileNameRenamed = 1u << 4, // 0x10
};
}

// ---------------------------------------------------------------------------
// Errors
// ---------------------------------------------------------------------------
struct GrfError : public std::runtime_error
{
    explicit GrfError(const std::string& msg) : std::runtime_error(msg) {}
};

// ---------------------------------------------------------------------------
// Encoding: archive file names are stored as CP1252 ("Ansi" in the reference);
// the display side re-interprets them. We keep archive names as raw bytes and
// convert to/from UTF-8 only at the UI/CLI boundary.
// ---------------------------------------------------------------------------
std::string cp1252ToUtf8(const std::string& ansi);
std::string utf8ToCp1252(const std::string& utf8);
std::string toLowerAscii(std::string s);
std::string toUpperAscii(std::string s);

// ---------------------------------------------------------------------------
// ByteReader: LE reading over a byte span (used for the decompressed table and
// compressed payloads).
// ---------------------------------------------------------------------------
class ByteReader
{
  public:
    ByteReader(const byte* data, std::size_t size)
        : _data(data), _size(size), _pos(0)
    {
    }
    explicit ByteReader(const Bytes& bytes)
        : _owner(bytes), _data(_owner.data()), _size(_owner.size()), _pos(0)
    {
    }

    bool canRead() const { return _pos < _size; }
    std::size_t remaining() const { return _size - _pos; }
    std::size_t pos() const { return _pos; }
    void forward(std::size_t n) { _pos += n; }

    byte u8()
    {
        _require(1);
        return _data[_pos++];
    }
    std::uint16_t u16()
    {
        _require(2);
        std::uint16_t v = le16(_data + _pos);
        _pos += 2;
        return v;
    }
    std::uint32_t u32()
    {
        _require(4);
        std::uint32_t v = le32(_data + _pos);
        _pos += 4;
        return v;
    }
    std::int32_t i32() { return (std::int32_t)u32(); }
    float f32()
    {
        _require(4);
        std::uint32_t v = le32(_data + _pos);
        _pos += 4;
        return std::bit_cast<float>(v);
    }
    std::int64_t i64()
    {
        _require(8);
        std::int64_t v = le64s(_data + _pos);
        _pos += 8;
        return v;
    }
    void bytes(byte* out, std::size_t n)
    {
        _require(n);
        std::copy_n(_data + _pos, n, out);
        _pos += n;
    }
    const byte* data() const { return _data; }

  private:
    void _require(std::size_t n) const
    {
        if (_pos + n > _size)
            throw GrfError("ByteReader: unexpected end of data");
    }
    Bytes _owner;
    const byte* _data;
    std::size_t _size;
    std::size_t _pos;
};

// ByteWriter: little-endian growable buffer.
class ByteWriter
{
  public:
    void u8(byte v) { _data.push_back(v); }
    void u16(std::uint16_t v)
    {
        byte b[2];
        put16(b, v);
        _raw(b, 2);
    }
    void u32(std::uint32_t v)
    {
        byte b[4];
        put32(b, v);
        _raw(b, 4);
    }
    void i32(std::int32_t v) { u32((std::uint32_t)v); }
    void i64(std::int64_t v)
    {
        byte b[8];
        put64(b, (std::uint64_t)v);
        _raw(b, 8);
    }
    void raw(const byte* p, std::size_t n) { _raw(p, n); }
    void raw(const Bytes& b) { _raw(b.data(), b.size()); }
    void zero(std::size_t n) { _data.insert(_data.end(), n, 0); }
    const Bytes& bytes() const { return _data; }
    std::size_t size() const { return _data.size(); }
    void clear() { _data.clear(); }

  private:
    void _raw(const byte* p, std::size_t n)
    {
        _data.insert(_data.end(), p, p + n);
    }
    Bytes _data;
};

} // namespace grf