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

// Output to fragment shader
out vec2 vTexCoord;
out vec3 vNormal;
out vec3 vWorldPos;

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
    vWorldPos = worldPos.xyz;

    // Transform normal to world space
    mat3 normalMatrix = mat3(transpose(inverse(instanceMatrix)));
    vNormal = normalize(normalMatrix * aNormal);

    vTexCoord = aTexCoord;

    // Final position
    gl_Position = uProjectionMatrix * uViewMatrix * worldPos;
}
