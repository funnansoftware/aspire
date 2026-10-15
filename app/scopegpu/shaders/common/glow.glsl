#ifndef GLOW_GLSL
#define GLOW_GLSL

// A glowing shape's color, keeping either its channels at full strength, which the glow screens over the frame, or its
// fainter ones, which it adds. A shape's color is the same at every corner, so a full channel is 1 here but for
// rounding.
vec4 GlowChannels(vec4 color, bool full)
{
    const vec3 strong = step(vec3(254.5 / 255.0), color.rgb);
    return vec4(color.rgb * (full ? strong : vec3(1.0) - strong), color.a);
}

#endif
