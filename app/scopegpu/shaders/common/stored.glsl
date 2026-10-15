#ifndef STORED_GLSL
#define STORED_GLSL

// What an 8-bit target stores for a color: scope draws each step of its glow into one, so a shader that does several
// steps at once stores each, to come out as scope does.
vec3 Stored(vec3 x)
{
    return floor((clamp(x, 0.0, 1.0) * 255.0) + 0.5) / 255.0;
}

#endif
