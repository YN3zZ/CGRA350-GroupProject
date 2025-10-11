#version 330 core

// uniform data
uniform vec3 uSunColor;
uniform float uIntensity;

// framebuffer outputs (Multiple Render Targets)
layout (location = 0) out vec4 fb_color;        // Regular scene color
layout (location = 1) out vec4 fb_bright_color; // Bright parts for bloom

void main() {
	// always bright
	vec3 emissive = uSunColor * uIntensity;
	fb_color = vec4(emissive, 1.0);

	// Extract bright parts for bloom
	// Calculate brightness using luminance formula
	float brightness = dot(emissive, vec3(0.2126, 0.7152, 0.0722));

	// If brightness exceeds threshold, output to bright buffer
	if (brightness > 1.0) {
		fb_bright_color = vec4(emissive, 1.0);
	} else {
		fb_bright_color = vec4(0.0, 0.0, 0.0, 1.0);
	}
}
