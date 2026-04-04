#version 410 core

in vec3 fragColor;
layout(location = 0) out vec4 outColor;

uniform float alpha;

void main() {
	outColor = vec4(fragColor, alpha);
}
