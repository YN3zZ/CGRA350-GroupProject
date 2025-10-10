#version 330 core

// uniform data
uniform mat4 uProjectionMatrix;
uniform mat4 uModelViewMatrix;
uniform mat4 uLightSpaceMatrix;
uniform vec4 uClipPlane;

// mesh data
layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aTexCoord;

// model data (this must match the input of the fragment shader)
out VertexData {
	float globalHeight;
	vec3 position;
	vec3 normal;
	vec2 textureCoord;
    vec3 tangent; // "Tangent vectors" required for normal mapping.
    vec3 bitangent; // Part of TBN matrix structure mentioned in lecture.
	vec4 lightSpacePos;
} v_out;

out float gl_ClipDistance[1];

void main() {
	// Send untransformed global y position for texture mapping based on height proportion.
	v_out.globalHeight = aPosition.y;

	// transform vertex data to viewspace
	v_out.position = (uModelViewMatrix * vec4(aPosition, 1)).xyz;
	v_out.normal = normalize((uModelViewMatrix * vec4(aNormal, 0)).xyz);
	v_out.textureCoord = aTexCoord;

    // Tangent on the x-axis for grid-based heightmap terrain.
    vec3 tangent = vec3(1.0f, 0.0f, 0.0f);

    // Lecture: need proper orthogonal basis for TBN matrix
    tangent = normalize(tangent - dot(tangent, aNormal) * aNormal);
    vec3 bitangent = cross(aNormal, tangent);

    // lecture: "Consider using a (tangent, bitangent and normal) TBN matrix structure for transformations"
    v_out.tangent = normalize((uModelViewMatrix * vec4(tangent, 0.0)).xyz);
    v_out.bitangent = normalize((uModelViewMatrix * vec4(bitangent, 0.0)).xyz);

	// Calculate light space position for shadow mapping
	v_out.lightSpacePos = uLightSpaceMatrix * vec4(aPosition, 1.0);

	gl_ClipDistance[0] = dot(vec4(aPosition, 1.0), uClipPlane);

    // Set the screenspace position (needed for converting to fragment data)
    gl_Position = uProjectionMatrix * uModelViewMatrix * vec4(aPosition, 1.0f);
}