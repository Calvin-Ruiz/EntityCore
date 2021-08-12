#version 450

layout (location=0) out vec2 texOut;

void main()
{
    const vec4 pos[4] = {{-1, -1, 0, 1}, {1, -1, 0, 1}, {-1, 1, 0, 1}, {1, 1, 0, 1}};
    const vec2 tex[4] = {{0, 0}, {1, 0}, {0, 1}, {1, 1}};
    gl_Position = pos[gl_VertexIndex];
    texOut = tex[gl_VertexIndex];
}
