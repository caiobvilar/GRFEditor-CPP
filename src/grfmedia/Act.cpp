#include "grfmedia/Act.h"

#include <cmath>
#include <cstring>

namespace grfmedia {

namespace {

std::string fixed40(const grf::byte* p)
{
    std::string s;
    for (int i = 0; i < 40; ++i)
    {
        if (p[i] == '\0')
            break;
        s += (char)p[i];
    }
    return s;
}

} // namespace

ActFile ActFile::load(const grf::Bytes& data)
{
    ActFile act;
    grf::ByteReader r(data);
    if (r.remaining() < 4)
        throw grf::GrfError("ACT: file too small for header");
    if (r.u8() != 'A' || r.u8() != 'C')
        throw grf::GrfError("ACT: bad magic (expected \"AC\")");
    act.minor = r.u8();
    act.major = r.u8();

    // Number of actions lives at offset 4 (u16); the loader then jumps to 16.
    r.forward(2);
    int actionCount = r.u16();

    // Re-read from the canonical frame-scan position.
    r = grf::ByteReader(data);
    r.forward(16);

    for (int a = 0; a < actionCount; ++a)
    {
        if (r.remaining() < 4)
            throw grf::GrfError("ACT: truncated action header");
        int frameCount = r.i32();
        ActAction action;
        action.animationSpeed = 6.0f;

        for (int f = 0; f < frameCount; ++f)
        {
            if (r.remaining() < 32)
                throw grf::GrfError("ACT: truncated frame header (name)");
            r.forward(32);
            int layerCount = r.i32();
            ActFrame frame;
            frame.soundId = -1;

            for (int l = 0; l < layerCount; ++l)
            {
                ActLayer layer;
                if (act.isV26())
                {
                    layer.offsetX = (int)std::floor(r.f32());
                    layer.offsetY = (int)std::floor(r.f32());
                } else
                {
                    layer.offsetX = r.i32();
                    layer.offsetY = r.i32();
                }
                layer.spriteIndex = r.i32();
                layer.mirror = r.i32() != 0;

                if (act.major >= 2)
                {
                    layer.color[0] = r.u8();
                    layer.color[1] = r.u8();
                    layer.color[2] = r.u8();
                    layer.color[3] = r.u8();
                    layer.scaleX = r.f32();
                    layer.scaleY = layer.scaleX;
                    if (act.major * 10 + act.minor >= 24)
                    { // 2.4+
                        layer.scaleY = r.f32();
                    }
                    layer.rotation = r.i32();
                    layer.spriteType = r.i32();
                    if (act.major * 10 + act.minor >= 25)
                    { // 2.5+
                        layer.width = r.i32();
                        layer.height = r.i32();
                    }
                }

                frame.layers.push_back(layer);
            }

            if (act.major >= 2)
            { // soundId
                frame.soundId = r.i32();
            }

            if (act.major * 10 + act.minor >= 23)
            { // anchors (2.3+)
                int anchorCount = r.i32();
                for (int k = 0; k < anchorCount; ++k)
                {
                    r.forward(4);
                    frame.anchors.push_back(r.i32());
                    frame.anchors.push_back(r.i32());
                    frame.anchors.push_back(r.i32());
                }
            }

            action.frames.push_back(std::move(frame));
        }

        act.actions.push_back(std::move(action));
    }

    if (act.major * 10 + act.minor >= 21)
    { // sound table (2.1+)
        int soundCount = r.i32();
        for (int i = 0; i < soundCount; ++i)
        {
            if (r.remaining() < 40)
                break;
            act.soundFiles.push_back(fixed40(r.data() + r.pos()));
            r.forward(40);
        }

        if (act.major * 10 + act.minor >= 22)
        { // per-action speeds (2.2+)
            for (ActAction& action : act.actions)
            {
                if (!r.canRead())
                    break;
                action.animationSpeed = std::bit_cast<float>(r.u32());
            }
        }
    }

    return act;
}

} // namespace grfmedia