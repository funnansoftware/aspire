#version 450
#extension GL_GOOGLE_include_directive : require

// The glowing shapes drawn through SDL's renderer, keeping one set of their channels. The renderer draws into one
// target at a time, so the full and fainter channels are drawn apart, each with its own render state.

#include "common/glow.glsl"

// From SDL's vertex shader for untextured geometry.
layout(location = 0) in vec4 sdlColor;

layout(set = 3, binding = 0, std140) uniform Uniforms
{
    // x: 1 to keep the channels at full strength, 0 to keep the fainter ones.
    vec4 channels;
} glow;

layout(location = 0) out vec4 outColor;

void main()
{
    outColor = GlowChannels(sdlColor, glow.channels.x > 0.5);
}
