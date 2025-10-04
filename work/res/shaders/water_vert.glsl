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
    float frequency = 80.0f;
    float amplitude = 0.03f;
    float speed = 0.6f;
    float displacement = (sin(uv.x * frequency + uTime * speed) + cos(uv.y * frequency/2.0f + uTime * speed)) * amplitude;
    vec3 newPosition = aPosition + vec3(0, displacement, 0);

    // Get nearby gradient to recalculate normals.
    float delta = 0.0001f;
    float gradX = sin((uv.x + delta) * frequency + uTime * speed) - sin((uv.x - delta) * frequency + uTime * speed);
    float gradY = cos((uv.y + delta) * frequency / 2.0f + uTime * speed) - cos((uv.y - delta) * frequency / 2.0f + uTime * speed);
    vec3 newNormal = normalize(vec3(-gradX, 1.0f, -gradY));

	// transform vertex data to viewspace
	v_out.position = (uModelViewMatrix * vec4(newPosition, 1)).xyz;
	v_out.normal = normalize((uModelViewMatrix * vec4(newNormal, 0)).xyz);
	v_out.textureCoord = uv;

    // Tangent on the x-axis for grid-based heightmap terrain.
    vec3 tangent = vec3(1.0f, 0.0f, 0.0f);

    // Lecture: need proper orthogonal basis for TBN matrix
    tangent = normalize(tangent - dot(tangent, newNormal) * newNormal);
    vec3 bitangent = cross(newNormal, tangent);

    // lecture: "Consider using a (tangent, bitangent and normal) TBN matrix structure for transformations"
    v_out.tangent = normalize((uModelViewMatrix * vec4(tangent, 0.0)).xyz);
    v_out.bitangent = normalize((uModelViewMatrix * vec4(bitangent, 0.0)).xyz);

    // Set the screenspace position (needed for converting to fragment data)
    gl_Position = uProjectionMatrix * uModelViewMatrix * vec4(newPosition, 1.0f);
}