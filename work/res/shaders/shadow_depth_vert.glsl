#version 330 core

layout(location = 0) in vec3 aPosition;

// Per-instance attributes (for trees)
layout(location = 3) in vec4 aInstanceMatrix0;
layout(location = 4) in vec4 aInstanceMatrix1;
layout(location = 5) in vec4 aInstanceMatrix2;
layout(location = 6) in vec4 aInstanceMatrix3;

uniform mat4 uLightSpaceMatrix;
uniform bool uUseInstancing;

void main() {
    if (uUseInstancing) {
        // Instanced rendering (trees)
        mat4 instanceMatrix = mat4(
            aInstanceMatrix0,
            aInstanceMatrix1,
            aInstanceMatrix2,
            aInstanceMatrix3
        );
        gl_Position = uLightSpaceMatrix * instanceMatrix * vec4(aPosition, 1.0);
    } else {
        // Non-instanced (terrain)
        gl_Position = uLightSpaceMatrix * vec4(aPosition, 1.0);
    }
}
