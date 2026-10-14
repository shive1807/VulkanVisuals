#version 450

layout(location = 0) out vec3 fragColor;

const vec2 positions[3] = vec2[](
    vec2( 0.0, -0.65),
    vec2( 0.65, 0.55),
    vec2(-0.65,  0.55)
);

const vec3 colors[3] = vec3[](
    vec3(0.10, 0.90, 1.00),
    vec3(1.00, 0.20, 0.65),
    vec3(0.50, 0.30, 1.00)
);

void main()
{
    gl_Position = vec4(positions[gl_VertexIndex], 0.0, 1.0);
    fragColor = colors[gl_VertexIndex];
}
