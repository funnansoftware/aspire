#version 450
#extension GL_GOOGLE_include_directive : require

// The glowing shapes, split between two targets at once: their channels at full strength and their fainter ones.

#include "common/glow.glsl"

layout(location = 0) in vec4 inColor;

layout(location = 0) out vec4 outFull;
layout(location = 1) out vec4 outFainter;

void main()
{
    outFull = GlowChannels(inColor, true);
    outFainter = GlowChannels(inColor, false);
}
