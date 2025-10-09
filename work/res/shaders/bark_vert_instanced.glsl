#version 330 core

// Per-vertex attributes
layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aTexCoord;

// Per-instance attributes (4 vec4s = 1 mat4)
layout(location = 3) in vec4 aInstanceMatrix0;
layout(location = 4) in vec4 aInstanceMatrix1;
layout(location = 5) in vec4 aInstanceMatrix2;
layout(location = 6) in vec4 aInstanceMatrix3;

// Uniforms
uniform mat4 uProjectionMatrix;
uniform mat4 uViewMatrix;
uniform mat4 uLightSpaceMatrix;
uniform vec4 uClipPlane;

// Output to fragment shader
out VertexData {
    vec3 worldPos;
    vec3 normal;
    vec2 texCoord;
    mat3 TBN;
	vec4 lightSpacePos;
} v_out;

out float gl_ClipDistance[1];

void main() {
    // Reconstruct instance matrix from 4 vec4s
    mat4 instanceMatrix = mat4(
        aInstanceMatrix0,
        aInstanceMatrix1,
        aInstanceMatrix2,
        aInstanceMatrix3
    );

    // Transform to world space using instance matrix
    vec4 worldPos = instanceMatrix * vec4(aPosition, 1.0);
    v_out.worldPos = worldPos.xyz;

    // Transform normal
    mat3 normalMatrix = mat3(transpose(inverse(instanceMatrix)));
    vec3 normal = normalize(normalMatrix * aNormal);
    v_out.normal = normal;

    // Calculate tangent and bitangent for normal mapping
    // For cylindrical geometry, tangent follows the circumference
    vec3 tangent;
    if (abs(normal.y) < 0.999) {
        // Most cases: tangent perpendicular to both normal and up vector
        vec3 up = vec3(0, 1, 0);
        tangent = normalize(cross(up, normal));
    } else {
        // When normal points up/down, use a different reference
        tangent = normalize(cross(vec3(1, 0, 0), normal));
    }
    vec3 bitangent = normalize(cross(normal, tangent));

    // Construct TBN matrix for normal mapping
    v_out.TBN = mat3(tangent, bitangent, normal);

    v_out.texCoord = aTexCoord;

	// Calculate light space position for shadow mapping
	v_out.lightSpacePos = uLightSpaceMatrix * worldPos;

	gl_ClipDistance[0] = dot(worldPos, uClipPlane);

    // Final position
    gl_Position = uProjectionMatrix * uViewMatrix * worldPos;
}