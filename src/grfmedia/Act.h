#pragma once

#include "grfcore/GrfUtil.h"
#include "grfmedia/RgbaImage.h"

#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

namespace grfmedia {

// Per-layer render parameters read from the ACT (GRF/FileFormats/ActFormat).
struct ActLayer
{
    int offsetX = 0;
    int offsetY = 0;
    int spriteIndex = -1; // RELATIVE within its type pool (indexed8 or bgra32)
    bool mirror = false;
    std::uint8_t color[4] = {255, 255, 255, 255}; // R,G,B,A mask
    float scaleX = 1.0f;
    float scaleY = 1.0f;
    int rotation = 0;   // degrees
    int spriteType = 0; // 0 = indexed8, 1 = bgra32
    int width = 0;      // 2.5+, else filled from the sprite image
    int height = 0;
};

struct ActFrame
{
    std::vector<ActLayer> layers;
    int soundId = -1;         // 2.0+
    std::vector<int> anchors; // offsetX,offsetY,index per anchor (2.3+)
};

struct ActAction
{
    std::vector<ActFrame> frames;
    float animationSpeed =
        6.0f; // default; times frameIntervalMs for per-frame delay
};

// .act animation file: "AC" + minor + major, versions 2.0-2.6.
struct ActFile
{
    int major = 0;
    int minor = 0;
    std::vector<ActAction> actions;
    std::vector<std::string> soundFiles;

    static ActFile load(const grf::Bytes& data);

    bool empty() const { return actions.empty(); }
    // Frame delay in milliseconds (matches
    // FrameRendererConfiguration.FrameInterval=24).
    int frameIntervalMs(int actionIndex) const
    {
        if (actionIndex < 0 || actionIndex >= (int)actions.size())
            return 24;
        return (int)(actions[actionIndex].animationSpeed * 24.0f);
    }
    std::string actionLabel(int actionIndex) const
    {
        char buf[32];
        std::snprintf(buf,
                      sizeof buf,
                      "Action %d (%d frames)",
                      actionIndex,
                      actionIndex >= 0 && actionIndex < (int)actions.size()
                          ? (int)actions[actionIndex].frames.size()
                          : 0);
        return buf;
    }
    bool isV26() const { return major * 10 + minor >= 26; }
};

} // namespace grfmedia