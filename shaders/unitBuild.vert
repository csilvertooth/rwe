#version 410 core

uniform mat4 mvpMatrix;
uniform mat4 modelMatrix;

layout(location = 0) in vec3 position;
layout(location = 1) in vec2 texCoord;
layout(location = 2) in vec3 normal;

out vec2 fragTexCoord;
out float height;
out vec3 worldNormal;

void main(void)
{
    vec4 worldPosition = modelMatrix * vec4(position, 1.0);
    gl_Position = mvpMatrix * vec4(position, 1.0);
    fragTexCoord = texCoord;
    height = worldPosition.y;
    worldNormal = mat3(modelMatrix) * normal;
}
