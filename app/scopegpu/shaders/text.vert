#version 450

// Glyphs' quads, from target pixels to clip space as shapes are, with where each corner is in the glyph atlas.

layout(location = 0) in vec2 inPosition;
layout(location = 1) in vec4 inColor;
layout(location = 2) in vec2 inUv;

layout(location = 0) out vec4 outColor;
layout(location = 1) out vec2 outUv;

layout(set = 1, binding = 0, std140) uniform Target
{
    // (2 / width, -2 / height, -1, 1), for the whole target.
    vec4 projection;
} target;

void main()
{
    outColor = inColor;
    outUv = inUv;
    gl_Position = vec4((inPosition * target.projection.xy) + target.projection.zw, 0.0, 1.0);
}
