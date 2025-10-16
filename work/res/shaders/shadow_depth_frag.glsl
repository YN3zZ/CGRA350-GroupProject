#version 330 core

uniform sampler2D uLeafTexture;
uniform bool uRenderingLeaves;

in vec2 vTexCoord;

void main() {
    if (uRenderingLeaves) {
        vec4 texColor = texture(uLeafTexture, vTexCoord);
        if (texColor.a < 0.1) {
            discard;
        }
    }

    // Depth is automatically written to GL_DEPTH_ATTACHMENT
}
