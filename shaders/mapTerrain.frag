#version 410 core

in vec3 fragTexCoord;
layout(location = 0) out vec4 outColor;

uniform sampler2DArray textureArraySampler;

void main(void)
{
    outColor = texture(textureArraySampler, fragTexCoord);
}
