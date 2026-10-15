#version 450
#extension GL_GOOGLE_include_directive : require

// The backdrop with the glow over it, over the whole target.

#include "common/backdrop.glsl"

layout(set = 2, binding = 0) uniform sampler2D fullGlow;
layout(set = 2, binding = 1) uniform sampler2D fainterGlow;

layout(set = 3, binding = 0, std140) uniform Uniforms
{
    vec4 inner;
    vec4 outer;
    // xy: the center, in target pixels. z: the radius, in pixels. w: 1 if the glow is drawn, otherwise 0.
    vec4 disc;
    // xy: scales a target pixel's position to where it samples the glow textures.
    vec4 glow;
    vec4 rect;
    vec4 background;
} backdrop;

layout(location = 0) out vec4 outColor;

void main()
{
    const vec2 pixel = gl_FragCoord.xy;
    vec3 color = Backdrop(pixel, backdrop.inner, backdrop.outer, backdrop.disc, backdrop.rect, backdrop.background);

    if (backdrop.disc.w > 0.5)
    {
        const vec2 uv = pixel * backdrop.glow.xy;
        color = Glow(color, textureLod(fullGlow, uv, 0.0).rgb, textureLod(fainterGlow, uv, 0.0).rgb);
    }

    outColor = vec4(color, 1.0);
}
