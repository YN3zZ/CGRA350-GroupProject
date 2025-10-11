#version 330 core

// Input from vertex shader
in vec2 vTexCoord;

// Uniforms
uniform sampler2D uSceneTexture;
uniform float uThreshold;
uniform bool uSmoothGradient;

// Output
out vec4 fragColor;

void main() {
    vec4 color = texture(uSceneTexture, vTexCoord);

    // Calculate luminance using standard weights
    float brightness = dot(color.rgb, vec3(0.2126, 0.7152, 0.0722));

    if (uSmoothGradient) {
        // Smooth gradient falloff (cubic)
        fragColor = color * brightness * brightness * brightness;
    } else {
        // Hard threshold
        if (brightness > uThreshold) {
            fragColor = color;
        } else {
            fragColor = vec4(0.0);
        }
    }
}
