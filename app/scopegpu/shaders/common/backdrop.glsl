#ifndef BACKDROP_GLSL
#define BACKDROP_GLSL

#include "stored.glsl"

// The backdrop at a pixel: inside its rectangle (rect: left, top, right, bottom), shading from its inner color at the
// disc's center (disc.xy) to its outer one at its radius (disc.z) and beyond, stored at 8 bits as scope's target stores
// it; outside, the background, which scope clears the frame to first.
vec3 Backdrop(vec2 pixel, vec4 inner, vec4 outer, vec4 disc, vec4 rect, vec4 background)
{
    const bool inside = all(greaterThanEqual(pixel, rect.xy)) && all(lessThan(pixel, rect.zw));
    const float reach = clamp(distance(pixel, disc.xy) / max(disc.z, 1e-6), 0.0, 1.0);
    return inside ? Stored(mix(inner.rgb, outer.rgb, reach)) : background.rgb;
}

// The glow over a color, as scope draws it into an 8-bit target: the fainter channels added and stored, then the full
// ones screened.
vec3 Glow(vec3 color, vec3 full, vec3 fainter)
{
    const vec3 added = Stored(color + fainter);
    return full + (added * (vec3(1.0) - full));
}

#endif
