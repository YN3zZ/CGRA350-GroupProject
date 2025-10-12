#version 330 core

// Input from vertex shader
in vec2 vTexCoord;

// Uniforms
uniform sampler2D uTexture;
uniform bool uHorizontal;
uniform float uIntensity;
uniform bool uAnamorphic;
uniform float uAnamorphicRatio;

// Output
out vec4 fragColor;

// Gaussian kernel weights (5-tap)
const float weight[5] = float[] (0.227027, 0.1945946, 0.1216216, 0.054054, 0.016216);

void main() {
    vec2 texelSize = 1.0 / textureSize(uTexture, 0);
    vec3 result = texture(uTexture, vTexCoord).rgb * weight[0];

    if (uHorizontal) {
        for (int i = 1; i < 5; ++i) {
            result += texture(uTexture, vTexCoord + vec2(texelSize.x * float(i) * uIntensity, 0.0)).rgb * weight[i];
            result += texture(uTexture, vTexCoord - vec2(texelSize.x * float(i) * uIntensity, 0.0)).rgb * weight[i];
        }
    } else {
        float verticalIntensity = uAnamorphic ? (uIntensity * uAnamorphicRatio) : uIntensity;
        for (int i = 1; i < 5; ++i) {
            result += texture(uTexture, vTexCoord + vec2(0.0, texelSize.y * float(i) * verticalIntensity)).rgb * weight[i];
            result += texture(uTexture, vTexCoord - vec2(0.0, texelSize.y * float(i) * verticalIntensity)).rgb * weight[i];
        }
    }

    fragColor = vec4(result, 1.0);
}
