#version 450

// Shapes in their straight-alpha color, blended over what's there.

layout(location = 0) in vec4 inColor;

layout(location = 0) out vec4 outColor;

void main()
{
    outColor = inColor;
}
