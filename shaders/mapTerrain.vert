#version 410 core

uniform mat4 mvpMatrix;

layout(location = 0) in vec3 position;
layout(location = 1) in vec3 texCoord;

out vec3 fragTexCoord;

void main(void)
{
    gl_Position = mvpMatrix * vec4(position, 1.0);
    fragTexCoord = texCoord;
}
