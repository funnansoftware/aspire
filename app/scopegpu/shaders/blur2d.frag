#version 450
#extension GL_GOOGLE_include_directive : require

// One axis of the glow's blur, for one texture, drawn through SDL's renderer as a texture over the whole target.

#include "common/blur.glsl"

// From SDL's vertex shader for textured geometry. The blur works from the pixel's position instead.
layout(location = 0) in vec4 sdlColor;
layout(location = 1) in vec2 sdlUv;

// The texture being drawn, which SDL binds first.
layout(set = 2, binding = 0) uniform sampler2D source;

layout(location = 0) out vec4 outColor;

void main()
{
    // Like scope's copies, the blur adds light without touching alpha, which nothing reads.
    outColor = vec4(Blur(source, gl_FragCoord.xy), 0.0);
}
