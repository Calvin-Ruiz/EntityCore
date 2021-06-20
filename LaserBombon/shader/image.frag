#version 450

layout (location=0) in vec2 texCoord;
layout (location=0) out vec4 fragColor;

layout (binding=0) uniform sampler2D tex;

void main()
{
    fragColor = texture(tex, texCoord);
}
