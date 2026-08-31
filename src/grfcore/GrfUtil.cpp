#include "GrfUtil.h"

namespace grf {

namespace {

// Highest 128 codepoints of Windows-1252. The 0x00-0x7F range is plain ASCII.
const std::uint16_t kCp1252High[128] = {
    // 0x80-0x9F
    0x20AC,
    0xFFFD,
    0x201A,
    0x0192,
    0x201E,
    0x2026,
    0x2020,
    0x2021,
    0x02C6,
    0x2030,
    0x0160,
    0x2039,
    0x0152,
    0xFFFD,
    0x017D,
    0xFFFD,
    0xFFFD,
    0x2018,
    0x2019,
    0x201C,
    0x201D,
    0x2022,
    0x2013,
    0x2014,
    0x02DC,
    0x2122,
    0x0161,
    0x203A,
    0x0153,
    0xFFFD,
    0x017E,
    0x0178,
    // 0xA0-0xFF (Latin-1)
    0x00A0,
    0x00A1,
    0x00A2,
    0x00A3,
    0x00A4,
    0x00A5,
    0x00A6,
    0x00A7,
    0x00A8,
    0x00A9,
    0x00AA,
    0x00AB,
    0x00AC,
    0x00AD,
    0x00AE,
    0x00AF,
    0x00B0,
    0x00B1,
    0x00B2,
    0x00B3,
    0x00B4,
    0x00B5,
    0x00B6,
    0x00B7,
    0x00B8,
    0x00B9,
    0x00BA,
    0x00BB,
    0x00BC,
    0x00BD,
    0x00BE,
    0x00BF,
    0x00C0,
    0x00C1,
    0x00C2,
    0x00C3,
    0x00C4,
    0x00C5,
    0x00C6,
    0x00C7,
    0x00C8,
    0x00C9,
    0x00CA,
    0x00CB,
    0x00CC,
    0x00CD,
    0x00CE,
    0x00CF,
    0x00D0,
    0x00D1,
    0x00D2,
    0x00D3,
    0x00D4,
    0x00D5,
    0x00D6,
    0x00D7,
    0x00D8,
    0x00D9,
    0x00DA,
    0x00DB,
    0x00DC,
    0x00DD,
    0x00DE,
    0x00DF,
    0x00E0,
    0x00E1,
    0x00E2,
    0x00E3,
    0x00E4,
    0x00E5,
    0x00E6,
    0x00E7,
    0x00E8,
    0x00E9,
    0x00EA,
    0x00EB,
    0x00EC,
    0x00ED,
    0x00EE,
    0x00EF,
    0x00F0,
    0x00F1,
    0x00F2,
    0x00F3,
    0x00F4,
    0x00F5,
    0x00F6,
    0x00F7,
    0x00F8,
    0x00F9,
    0x00FA,
    0x00FB,
    0x00FC,
    0x00FD,
    0x00FE,
    0x00FF,
};

void appendUtf8(std::string& out, std::uint32_t cp)
{
    if (cp <= 0x7F)
    {
        out += (char)cp;
    } else if (cp <= 0x7FF)
    {
        out += (char)(0xC0 | (cp >> 6));
        out += (char)(0x80 | (cp & 0x3F));
    } else
    {
        out += (char)(0xE0 | (cp >> 12));
        out += (char)(0x80 | ((cp >> 6) & 0x3F));
        out += (char)(0x80 | (cp & 0x3F));
    }
}

} // namespace

std::string cp1252ToUtf8(const std::string& ansi)
{
    std::string out;
    out.reserve(ansi.size());
    for (unsigned char c : ansi)
    {
        if (c < 0x80)
        {
            out += (char)c;
        } else if (kCp1252High[c - 0x80] == 0xFFFD)
        {
            out += '?';
        } else
        {
            appendUtf8(out, kCp1252High[c - 0x80]);
        }
    }
    return out;
}

std::string utf8ToCp1252(const std::string& utf8)
{
    std::string out;
    out.reserve(utf8.size());
    for (std::size_t i = 0; i < utf8.size(); ++i)
    {
        unsigned char c = (unsigned char)utf8[i];
        if (c < 0x80)
        {
            out += (char)c;
            continue;
        }
        std::uint32_t cp = 0;
        int len = 0;
        if ((c >> 5) == 0x6 && i + 1 < utf8.size())
        {
            cp = ((c & 0x1F) << 6) | ((unsigned char)utf8[i + 1] & 0x3F);
            len = 2;
        } else if ((c >> 4) == 0xE && i + 2 < utf8.size())
        {
            cp = ((c & 0x0F) << 12) |
                 (((unsigned char)utf8[i + 1] & 0x3F) << 6) |
                 ((unsigned char)utf8[i + 2] & 0x3F);
            len = 3;
        } else
        {
            out += '?';
            continue;
        }
        if (cp < 0x80)
        {
            out += (char)cp;
        } else
        {
            // sparse 0x80-0x9F slots first, then Latin-1 straight-through
            bool found = false;
            for (int j = 0; j < 32; ++j)
            {
                if (kCp1252High[j] != 0xFFFD && kCp1252High[j] == cp)
                {
                    out += (char)(0x80 + j);
                    found = true;
                    break;
                }
            }
            if (!found && cp <= 0xFF)
                out += (char)cp;
            else if (!found)
                out += '?';
        }
        i += len - 1;
    }
    return out;
}

std::string toLowerAscii(std::string s)
{
    for (char& c : s)
        if (c >= 'A' && c <= 'Z')
            c = (char)(c - 'A' + 'a');
    return s;
}

std::string toUpperAscii(std::string s)
{
    for (char& c : s)
        if (c >= 'a' && c <= 'z')
            c = (char)(c - 'a' + 'A');
    return s;
}

} // namespace grf