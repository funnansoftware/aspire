#version 450

// Glyphs from the atlas, which holds white with coverage in alpha, tinted by their straight-alpha color.

layout(set = 2, binding = 0) uniform sampler2D atlas;

layout(location = 0) in vec4 inColor;
layout(location = 1) in vec2 inUv;

layout(location = 0) out vec4 outColor;

void main()
{
    outColor = texture(atlas, inUv) * inColor;
}
