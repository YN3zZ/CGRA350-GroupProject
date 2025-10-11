#version 330 core

// uniform data
uniform mat4 uModelViewMatrix;
uniform mat4 uProjectionMatrix;

// mesh data
layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aTexCoord;

void main() {
	vec4 pos = uProjectionMatrix * uModelViewMatrix * vec4(aPosition, 1.0);
	// Set depth to far plane (z = w) to make sun appear infinitely far like skybox
	gl_Position = pos.xyww;
}
