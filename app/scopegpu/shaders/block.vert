#version 450

// Data blocks' glyphs, each an instance of a glyph's quad, placed as scope places text: at a whole pixel, a column and a
// line on from where its block starts. The rounding's done as the CPU does it, so the glyphs land on the same pixels.

// The quad's corner, from (0, 0) at its top left to (1, 1) at its bottom right.
layout(location = 0) in vec2 inCorner;

// The glyph: where its block starts and its baseline is, in target pixels, and its column and line; its quad from its
// pen position, in pixels; where it is in the atlas; and its straight-alpha color.
layout(location = 1) in vec4 inPlace;
layout(location = 2) in vec4 inQuad;
layout(location = 3) in vec4 inUv;
layout(location = 4) in vec4 inColor;

layout(location = 0) out vec4 outColor;
layout(location = 1) out vec2 outUv;

layout(set = 1, binding = 0, std140) uniform Target
{
    // (2 / width, -2 / height, -1, 1), for the whole target. It comes first, as text.vert's does, so text and shapes
    // drawn after the blocks find it where their own shaders read it.
    vec4 projection;
    // How far a column and a line step, and the font's ascent, in pixels.
    vec4 metrics;
} target;

// Rounds halfway cases away from zero, as std::round does. The fraction's worked out exactly, where adding a half first
// could round up a value just short of one.
float RoundHalfAway(float x)
{
    const float size = abs(x);
    const float whole = floor(size);
    return sign(x) * ((size - whole) >= 0.5 ? whole + 1.0 : whole);
}

void main()
{
    // Each step as the CPU takes it, unfused, so it rounds the same.
    precise float across = inPlace.x + (target.metrics.x * inPlace.z);
    precise float down = inPlace.y + (target.metrics.y * inPlace.w);
    precise float left = RoundHalfAway(across) + inQuad.x;
    precise float top = (RoundHalfAway(down) - target.metrics.z) + inQuad.y;
    precise vec2 position = vec2(inCorner.x > 0.5 ? left + inQuad.z : left, inCorner.y > 0.5 ? top + inQuad.w : top);

    outColor = inColor;
    outUv = vec2(inCorner.x > 0.5 ? inUv.z : inUv.x, inCorner.y > 0.5 ? inUv.w : inUv.y);
    gl_Position = vec4((position * target.projection.xy) + target.projection.zw, 0.0, 1.0);
}
