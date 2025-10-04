#version 330 core

// uniform data
uniform mat4 uProjectionMatrix;
uniform mat4 uModelViewMatrix;
// Current time for animating waves displacement.
uniform float uTime;

// mesh data
layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 uv;

// model data (this must match the input of the fragment shader)
out VertexData {
	float globalHeight;
	vec3 position;
	vec3 normal;
	vec2 textureCoord;
    vec3 tangent; // "Tangent vectors" required for normal mapping.
    vec3 bitangent; // Part of TBN matrix structure mentioned in lecture.
} v_out;

void main() {
	// Send untransformed global y position for texture mapping based on height proportion.
	v_out.globalHeight = aPosition.y;

    // Wave displacement animation
    float frequency = 60.0f;
    float amplitude = 0.03f;
    float speed = 0.5f;
    float displacement = (sin(uv.x * frequency + uTime * speed) + cos(uv.y * frequency/2.0f + uTime * speed)) * amplitude;
    vec3 newPosition = aPosition + vec3(0, displacement, 0);

	// transform vertex data to viewspace
	v_out.position = (uModelViewMatrix * vec4(newPosition, 1)).xyz;
	v_out.normal = normalize((uModelViewMatrix * vec4(aNormal, 0)).xyz);
	v_out.textureCoord = uv;

    // Tangent on the x-axis for grid-based heightmap terrain.
    vec3 tangent = vec3(1.0f, 0.0f, 0.0f);

    // Lecture: need proper orthogonal basis for TBN matrix
    tangent = normalize(tangent - dot(tangent, aNormal) * aNormal);
    vec3 bitangent = cross(aNormal, tangent);

    // lecture: "Consider using a (tangent, bitangent and normal) TBN matrix structure for transformations"
    v_out.tangent = normalize((uModelViewMatrix * vec4(tangent, 0.0)).xyz);
    v_out.bitangent = normalize((uModelViewMatrix * vec4(bitangent, 0.0)).xyz);

    // Set the screenspace position (needed for converting to fragment data)
    gl_Position = uProjectionMatrix * uModelViewMatrix * vec4(newPosition, 1.0f);
}