#version 330 core

// uniform data
uniform vec3 uSunColor;
uniform float uIntensity;

// framebuffer output
out vec4 fb_color;

void main() {
	// always bright
	vec3 emissive = uSunColor * uIntensity;
	fb_color = vec4(emissive, 1.0);
}
