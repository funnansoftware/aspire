#version 450
#extension GL_GOOGLE_include_directive : require

// One axis of the glow's blur, for both of its textures at once.

#include "common/blur.glsl"

layout(set = 2, binding = 0) uniform sampler2D fullSource;
layout(set = 2, binding = 1) uniform sampler2D fainterSource;

layout(location = 0) out vec4 outFull;
layout(location = 1) out vec4 outFainter;

void main()
{
    // Like scope's copies, the blur adds light without touching alpha, which nothing reads.
    outFull = vec4(Blur(fullSource, gl_FragCoord.xy), 0.0);
    outFainter = vec4(Blur(fainterSource, gl_FragCoord.xy), 0.0);
}
