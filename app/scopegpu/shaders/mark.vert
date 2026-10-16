#version 450

// Tracks' marks, each an instance of one mesh kept on the GPU: a mark at the origin pointing north, in target pixels.
// Each of the mesh's corners belongs to a part of the mark, which the instance moves into place: its history dots to
// where the track has been, its symbol to where it is, turned to its heading, and its leader and tick out along it.
// Parts that the track doesn't have, or that the scale ring clips out, collapse to a point off the target.

// The parts, as the painter numbers them. The first six are the history dots, newest first.
const uint DotParts = 6u;
const uint LeaderNear = 6u;
const uint LeaderFar = 7u;
const uint Ring = 8u;
const uint TickNear = 9u;
const uint TickFar = 10u;
const uint Outline = 11u;
const uint Filled = 12u;

// The instance's flags: which of its dots, its symbol, its leader and its tick the scale ring keeps, and its glyph.
const uint SymbolKept = 1u << 6u;
const uint LeaderKept = 1u << 7u;
const uint TickKept = 1u << 8u;
const uint GlyphShift = 9u;
const uint GlyphMask = 3u;

// The glyphs, as scope numbers them.
const uint CircleGlyph = 0u;
const uint OutlineGlyph = 1u;
const uint FilledGlyph = 2u;

// The corner: xy its place in pixels, z its alpha, and w, for a leader's or tick's end, how far that end is from the
// mark's center in logical units.
layout(location = 0) in vec4 inCorner;
layout(location = 1) in uint inPart;

// The instance, in logical units: xy where the track is and zw its heading's cosine and sine; its color, with in a how
// far its leader ends unclipped; how far its leader and tick start and end as the scale ring clips them; its dots; and
// its flags.
layout(location = 2) in vec4 inPlace;
layout(location = 3) in vec4 inColor;
layout(location = 4) in vec4 inSegments;
layout(location = 5) in vec4 inDots01;
layout(location = 6) in vec4 inDots23;
layout(location = 7) in vec4 inDots45;
layout(location = 8) in uint inFlags;

layout(location = 0) out vec4 outColor;

layout(set = 1, binding = 0, std140) uniform Target
{
    // (2 / width, -2 / height, -1, 1), for the whole target. It comes first, so shapes drawn after the marks find it
    // where their own shader reads it.
    vec4 projection;
    // x: target pixels per logical unit; y: 1 if the scale ring clips the marks, 0 if not.
    vec4 view;
} target;

// Where the dot of a part is, in logical units.
vec2 Dot(uint part)
{
    const vec4 pair = part < 2u ? inDots01 : (part < 4u ? inDots23 : inDots45);
    return (part & 1u) == 0u ? pair.xy : pair.zw;
}

// Whether the instance draws a part: one of its own glyph's, or every glyph's, that the scale ring keeps if it clips.
// Each function returns once, as early returns leave Direct3D's compilers warning of variables used uninitialized.
bool Drawn(uint part, bool clipped)
{
    const uint glyph = (inFlags >> GlyphShift) & GlyphMask;
    const bool circlePart = part == Ring || part == TickNear || part == TickFar;
    const bool own = (!circlePart || glyph == CircleGlyph) && (part != Outline || glyph == OutlineGlyph) && (part != Filled || glyph == FilledGlyph);
    uint kept = SymbolKept;

    if (part < DotParts)
    {
        kept = 1u << part;
    }
    else if (part == LeaderNear || part == LeaderFar)
    {
        kept = LeaderKept;
    }
    else if (part == TickNear || part == TickFar)
    {
        kept = TickKept;
    }

    return own && (!clipped || (inFlags & kept) != 0u);
}

// How far along the heading an end of a leader or tick goes, in logical units.
float Reach(uint part, bool clipped)
{
    // Unclipped, only the leader's far end moves from where the mesh has it, by the track's speed.
    float reach = part == LeaderFar ? inColor.a : inCorner.w;

    if (clipped)
    {
        reach = part == LeaderNear ? inSegments.x : (part == LeaderFar ? inSegments.y : (part == TickNear ? inSegments.z : inSegments.w));
    }

    return reach;
}

void main()
{
    const float scale = target.view.x;
    const bool clipped = target.view.y > 0.5;
    const uint part = inPart;
    outColor = vec4(inColor.rgb, inCorner.z);
    precise vec2 position;

    if (part < DotParts)
    {
        // Dots are circles, the same whichever way the track heads.
        position = (Dot(part) * scale) + inCorner.xy;
    }
    else if (part == Ring)
    {
        // So is a generic track's circle.
        position = (inPlace.xy * scale) + inCorner.xy;
    }
    else
    {
        vec2 corner = inCorner.xy;

        // A leader's or tick's end moves north, which is up the target, to where it reaches.
        if (part == LeaderNear || part == LeaderFar || part == TickNear || part == TickFar)
        {
            corner.y -= (Reach(part, clipped) - inCorner.w) * scale;
        }

        // Turned clockwise to the heading, on a target where y grows downward.
        const vec2 turned = vec2((corner.x * inPlace.z) - (corner.y * inPlace.w), (corner.x * inPlace.w) + (corner.y * inPlace.z));
        position = (inPlace.xy * scale) + turned;
    }

    // A part that isn't drawn has every corner at one point off the target, so its triangles have no area.
    gl_Position = Drawn(part, clipped) ? vec4((position * target.projection.xy) + target.projection.zw, 0.0, 1.0) : vec4(-2.0, -2.0, 0.0, 1.0);
}
