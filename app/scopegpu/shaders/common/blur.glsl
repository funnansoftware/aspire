#ifndef BLUR_GLSL
#define BLUR_GLSL

#include "stored.glsl"

// The build defines MAX_TAPS, and gives the painters the same number, so they can't disagree.
const int MaxTaps = MAX_TAPS;

// The blur's uniforms, the same for every blur shader. axis.xy is the axis, (1, 0) or (0, 1), and axis.zw 1 / the
// source's size in pixels. used.xy is the part of the source the glow uses, and used.z how many taps. Each tap's x is
// its copy's shift in pixels, and its y the copy's weight.
layout(set = 3, binding = 0, std140) uniform Uniforms
{
    vec4 axis;
    vec4 used;
    vec4 taps[MaxTaps];
} blur;

// One axis of the glow's blur at a pixel, as scope draws it. Each tap is a copy of the source shifted along the axis and
// weighted: it covers only where it lands, and it's added to an 8-bit target, so every sum is stored at 8 bits before
// the next copy's added.
//
// The taps are read from the uniforms where they are: passed as an array, they'd be copied into one the shader indexes
// for each pixel.
vec3 Blur(sampler2D source, vec2 pixel)
{
    const int count = min(int(blur.used.z), MaxTaps);
    vec3 sum = vec3(0.0);

    for (int i = 0; i < count; ++i)
    {
        // The copy covers the used part shifted along the axis, and its pixels sample the source where they came from.
        const vec2 from = pixel - (blur.axis.xy * blur.taps[i].x);

        // Whether a copy covers a pixel differs between neighbors, so the texture's sampled at its one level rather
        // than at one worked out from them.
        if (all(greaterThanEqual(from, vec2(0.0))) && all(lessThan(from, blur.used.xy)))
        {
            sum = Stored(sum + (textureLod(source, from * blur.axis.zw, 0.0).rgb * blur.taps[i].y));
        }
    }

    return sum;
}

#endif
