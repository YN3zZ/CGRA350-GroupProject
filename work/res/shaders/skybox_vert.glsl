#version 330 core
layout (location = 0) in vec3 aPos;

out vec3 TexCoords;

uniform mat4 projection;
uniform mat4 view;

void main() {
    TexCoords = aPos;
    // Translate the skybox down
    vec3 translatedPos = aPos + vec3(0.0, -0.3, 0.0);
    vec4 pos = projection * view * vec4(translatedPos, 1.0);
    gl_Position = pos.xyww;
}
