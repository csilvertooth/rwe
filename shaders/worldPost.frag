#version 410 core

in vec2 fragTexCoord;
layout(location = 0) out vec4 outColor;

uniform sampler2D screenTexture;
uniform sampler2D dodgeMask;
uniform sampler2D fogMask;

void main(void)
{
    vec4 screenValue = texture(screenTexture, fragTexCoord);
    vec4 dodgeMaskValue = texture(dodgeMask, fragTexCoord);
    vec3 result = screenValue.rgb / (vec3(1.0, 1.0, 1.0) - dodgeMaskValue.rgb);

    // Apply fog of war: darken toward black based on fog alpha
    float fogAlpha = texture(fogMask, fragTexCoord).a;
    result = result * (1.0 - fogAlpha);

    outColor = vec4(result, 1.0);
}
