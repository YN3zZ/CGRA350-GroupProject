#version 330 core

uniform sampler2D uLeafTexture;

in vec2 vTexCoord;

out vec4 fragColor;

void main() {
    vec4 texColor = texture(uLeafTexture, vTexCoord);

    // Discard fully transparent pixels
    if (texColor.a < 0.1) {
        discard;
    }

    fragColor = texColor;
}
