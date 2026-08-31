#pragma once

#include "grfmedia/Act.h"
#include "grfmedia/RgbaImage.h"
#include "grfmedia/Spr.h"

#include <cmath>
#include <cstdint>
#include <optional>

namespace grfmedia {

// Render one ACT frame into an RGBA canvas. Layers are composited in
// list order (later layers on top). The transform mirrors the WPF
// LayerDraw pipeline in GRFEditor (mirror -> translate-to-center ->
// scale -> rotate -> offset), evaluated via inverse mapping so every
// destination pixel is sampled exactly once.
struct ActRender
{
    struct Options
    {
        float zoom = 1.0f;      // uniform zoom around the image center
        int frameMs = 0;        // unused here (caller selects the frame)
        int margin = 32;        // px border around the bounding box
        bool roundToInt = true; // sample to nearest integer pixel
    };

    // Render (action, frame) from *spr* and *act*. Returns a transparent
    // canvas sized to the union of all transformed layers plus *margin*.
    static RgbaImage render(const Spr& spr,
                            const ActFile& act,
                            int actionIdx,
                            int frameIdx,
                            const Options& opt = Options());

    // Compute the axis-aligned bounding box (in canvas-local coords,
    // before centering) of the union of transformed layers.
    struct Bounds
    {
        float xMin = 0.0f, yMin = 0.0f;
        float xMax = 0.0f, yMax = 0.0f;
    };
    static Bounds bounds(const Spr& spr,
                         const ActFile& act,
                         int actionIdx,
                         int frameIdx,
                         const Options& opt);
};

} // namespace grfmedia