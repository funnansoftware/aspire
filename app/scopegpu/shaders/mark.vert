#version 450

// Tracks' marks, each an instance of one mesh kept on the GPU: a mark at the origin pointing north, in target pixels.
// Each of the mesh's corners belongs to a part of the mark, which the instance moves into place: its history dots to
// where the track was on its route, its symbol to where it is, turned to its heading, and its leader and tick out along
// it. Parts that the track doesn't have, or that the scale ring clips out, collapse to a point off the target.

// The parts, as the painter numbers them. The first six are the history dots, newest first.
const uint DotParts = 6u;
const uint LeaderNear = 6u;
const uint LeaderFar = 7u;
const uint Ring = 8u;
const uint TickNear = 9u;
const uint TickFar = 10u;
const uint Outline = 11u;
const uint Filled = 12u;

// The instance's flags: whether the scale ring keeps its symbol, its leader and its tick; its glyph; and its route.
const uint SymbolKept = 1u << 6u;
const uint LeaderKept = 1u << 7u;
const uint TickKept = 1u << 8u;
const uint GlyphShift = 9u;
const uint RouteShift = 11u;
const uint KindMask = 3u;

// The glyphs and routes, as scope numbers them.
const uint CircleGlyph = 0u;
const uint OutlineGlyph = 1u;
const uint FilledGlyph = 2u;
const uint OrbitRoute = 0u;
const uint FigureEightRoute = 1u;

const float RightAngle = 90.0;

// The corner: xy its place in pixels, z its alpha, and w, for a leader's or tick's end, how far that end is from the
// mark's center in logical units.
layout(location = 0) in vec4 inCorner;
layout(location = 1) in uint inPart;

// The instance. Its place: where the track is, in logical units, its heading, and how far along its route it is: an
// orbit's bearing from its center, an eight's phase, or a transit's miles flown. Its color, with in a how far its leader
// ends unclipped. How far its leader and tick start and end as the scale ring clips them. Its route, in nautical miles
// and degrees: an orbit's center and radius; a figure of eight's center, axis bearing and half its length; or a
// transit's start, heading and span. How fast the route goes: an orbit's degrees, an eight's radians, or a transit's
// miles, each a second. And its flags.
layout(location = 2) in vec4 inPlace;
layout(location = 3) in vec4 inColor;
layout(location = 4) in vec4 inSegments;
layout(location = 5) in vec4 inRoute;
layout(location = 6) in float inPace;
layout(location = 7) in uint inFlags;

layout(location = 0) out vec4 outColor;

layout(set = 1, binding = 0, std140) uniform Target
{
    // (2 / width, -2 / height, -1, 1), for the whole target. It comes first, so shapes drawn after the marks find it
    // where their own shader reads it.
    vec4 projection;
    // x: target pixels per logical unit; y: 1 if the scale ring clips the marks, 0 if not.
    vec4 view;
    // xy: where ownship is, in logical units; z: logical units per nautical mile; w: scope seconds between dots.
    vec4 world;
    // The scale ring, in target pixels: xy its center, z its radius.
    vec4 clip;
} target;

// A unit vector toward a bearing, east in x and north in y, as scope's routes turn.
vec2 Toward(float bearing)
{
    const float angle = radians(bearing);
    return vec2(sin(angle), cos(angle));
}

// Where the track was on its route some scope seconds ago, in nautical miles east and north of ownship, as scope samples
// it: how far along the route it is now, stepped back at the route's pace, so every number stays within a lap or so.
vec2 RouteBefore(float seconds)
{
    const uint kind = (inFlags >> RouteShift) & KindMask;
    const float progress = inPlace.w - (inPace * seconds);
    vec2 position;

    if (kind == OrbitRoute)
    {
        position = inRoute.xy + (Toward(progress) * inRoute.z);
    }
    else if (kind == FigureEightRoute)
    {
        // Along the axis the track swings out to each end; across it, twice as fast, to make the two loops.
        position = inRoute.xy + (Toward(inRoute.z) * (inRoute.w * sin(progress))) + (Toward(inRoute.z + RightAngle) * (inRoute.w * 0.5 * sin(2.0 * progress)));
    }
    else
    {
        // A transit starts over once it's flown its span.
        position = inRoute.xy + (Toward(inRoute.z) * mod(progress, inRoute.w));
    }

    return position;
}

// Whether the instance draws a part other than a dot: one of its own glyph's, or every glyph's, that the scale ring
// keeps if it clips. Each function returns once, as early returns leave Direct3D's compilers warning of variables used
// uninitialized.
bool Drawn(uint part, bool clipped)
{
    const uint glyph = (inFlags >> GlyphShift) & KindMask;
    const bool circlePart = part == Ring || part == TickNear || part == TickFar;
    const bool own = (!circlePart || glyph == CircleGlyph) && (part != Outline || glyph == OutlineGlyph) && (part != Filled || glyph == FilledGlyph);
    uint kept = SymbolKept;

    if (part == LeaderNear || part == LeaderFar)
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
    bool drawn = true;

    if (part < DotParts)
    {
        // Each dot is where the track was a dot's interval before the one before it, which is a circle the same
        // whichever way the track heads. The scale ring keeps or drops it whole by its center.
        const vec2 miles = RouteBefore(float(part + 1u) * target.world.w);
        const vec2 center = vec2(target.world.x + (miles.x * target.world.z), target.world.y - (miles.y * target.world.z)) * scale;
        position = center + inCorner.xy;
        drawn = !clipped || length(center - target.clip.xy) <= target.clip.z;
    }
    else if (part == Ring)
    {
        // So is a generic track's circle.
        position = (inPlace.xy * scale) + inCorner.xy;
        drawn = Drawn(part, clipped);
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
        const float angle = radians(inPlace.z);
        const float cosine = cos(angle);
        const float sine = sin(angle);
        const vec2 turned = vec2((corner.x * cosine) - (corner.y * sine), (corner.x * sine) + (corner.y * cosine));
        position = (inPlace.xy * scale) + turned;
        drawn = Drawn(part, clipped);
    }

    // A part that isn't drawn has every corner at one point off the target, so its triangles have no area.
    gl_Position = drawn ? vec4((position * target.projection.xy) + target.projection.zw, 0.0, 1.0) : vec4(-2.0, -2.0, 0.0, 1.0);
}
