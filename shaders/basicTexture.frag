#version 410 core

in vec2 fragTexCoord;
layout(location = 0) out vec4 outColor;

uniform sampler2D textureSampler;
uniform vec4 tint;

void main(void)
{
    outColor = texture(textureSampler, fragTexCoord) * tint;
}
