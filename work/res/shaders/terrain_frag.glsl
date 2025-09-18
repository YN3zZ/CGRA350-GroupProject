#version 330 core

// uniform data
uniform mat4 uProjectionMatrix;
uniform mat4 uModelViewMatrix;
uniform vec3 uColor;
uniform vec2 heightRange;

// viewspace data (this must match the output of the fragment shader)
in VertexData{
	vec3 globalPos;
	vec3 position;
	vec3 normal;
	vec2 textureCoord;
} f_in;

// framebuffer output
out vec4 fb_color;

void main() {
	// calculate lighting (hack)
	vec3 eye = normalize(-f_in.position);
	float light = abs(dot(normalize(f_in.normal), eye));
	vec3 color = mix(uColor / 4, uColor, light);

	float minHeight = heightRange.x;
	float maxHeight = heightRange.y;
	float heightProportion = smoothstep(minHeight, maxHeight, f_in.globalPos.y);

	// Temporarily hardcoded.
	vec3 color1 = vec3(0.58f, 0.43f, 0.55f);
	vec3 color2 = vec3(0.64f, 0.63f, 0.57f);
	vec3 stoneColor = mix(color1, color2, clamp(heightProportion - 0.3f, 0.0f, 1.0f));

	// output to the frambuffer
	fb_color = vec4(mix(color, stoneColor, heightProportion), 1);
}