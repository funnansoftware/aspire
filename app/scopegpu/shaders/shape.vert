#version 450

// Shapes' triangles, from target pixels to clip space as SDL's renderer maps them: (0, 0) at the top left, no
// half-pixel offset.

layout(location = 0) in vec2 inPosition;
layout(location = 1) in vec4 inColor;

layout(location = 0) out vec4 outColor;

layout(set = 1, binding = 0, std140) uniform Target
{
    // (2 / width, -2 / height, -1, 1), for the whole target.
    vec4 projection;
} target;

void main()
{
    outColor = inColor;
    gl_Position = vec4((inPosition * target.projection.xy) + target.projection.zw, 0.0, 1.0);
}
