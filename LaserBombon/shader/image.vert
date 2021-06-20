#version 450

layout (location=0) in vec4 pos;
layout (location=1) in vec2 tex;

layout (location=0) out vec2 texOut;

void main()
{
    gl_Position = pos;
    texOut = tex;
}
