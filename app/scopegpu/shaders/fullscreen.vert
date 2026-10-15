#version 450

// One triangle that covers the whole target, for passes that work out each pixel from where it is.

void main()
{
    const vec2 corner = vec2(float((gl_VertexIndex << 1) & 2), float(gl_VertexIndex & 2));
    gl_Position = vec4((corner * 2.0) - 1.0, 0.0, 1.0);
}
