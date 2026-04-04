#version 410 core

uniform mat4 vpMatrix;
uniform mat4 modelMatrix;
uniform float groundHeight;

layout(location = 0) in vec3 position;
layout(location = 1) in vec2 texCoord;

out vec2 fragTexCoord;
out float height;

void main(void)
{
    vec4 worldPosition = modelMatrix * vec4(position, 1.0);

    float shadowOffset = (worldPosition.y - groundHeight) * 0.25;

    vec4 shadowPosition = vec4(
        worldPosition.x + shadowOffset,
        groundHeight,
        worldPosition.z - shadowOffset,
        1.0);

    gl_Position = vpMatrix * shadowPosition;
    fragTexCoord = texCoord;
    height = worldPosition.y;
}
