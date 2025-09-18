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

	vec3 stoneColor = mix(vec3(0.64f, 0.63f, 0.65f), vec3(0.24f, 0.23f, 0.25f), 1 - heightProportion);

	// output to the frambuffer
	fb_color = vec4(mix(color, stoneColor, heightProportion), 1);
}