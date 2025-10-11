#version 330 core

// Based on: http://john-chapman-graphics.blogspot.com/2013/02/pseudo-lens-flare.html
// Reference: https://github.com/geoo993/ComputerGraphicsWithOpenGL

// Input from vertex shader
in vec2 vTexCoord;

// Uniforms
uniform sampler2D uBrightTexture;
uniform sampler2D uLensColorTexture;
uniform sampler2D uLensMaskTexture;
uniform int uLensType;            // 0 = Ghost, 1 = Halo, 2 = Both
uniform bool uUseLensTexture;
uniform int uGhostCount;          // Number of ghost samples (4-32)
uniform float uGhostDispersal;    // Dispersion factor (0.0-2.0)
uniform float uGhostThreshold;    // Ghost brightness threshold (0.0-20.0)
uniform float uGhostDistortion;   // Chromatic distortion amount
uniform float uHaloRadius;        // Halo radius (0.0-2.0)
uniform float uHaloThreshold;     // Halo brightness threshold (0.0-20.0)

// Output
out vec4 fragColor;

// Sample texture with chromatic aberration distortion
vec4 textureDistorted(in sampler2D tex, in vec2 texcoord, in vec2 direction, in vec3 distortion) {
    return vec4(
        texture(tex, texcoord + direction * distortion.r).r,
        texture(tex, texcoord + direction * distortion.g).g,
        texture(tex, texcoord + direction * distortion.b).b,
        1.0
    );
}

// Apply halo effect
vec4 applyHalo(in sampler2D tex, in vec2 texcoord, in vec2 ghostVec, in vec2 direction, in vec3 distortion) {
    vec2 haloVec = normalize(ghostVec) * uHaloRadius;
    vec2 offset = texcoord + haloVec;

    // Weight to restrict contribution to a ring
    float weight = length(vec2(0.5) - fract(offset)) / length(vec2(0.5));
    weight = pow(1.0 - weight, uHaloThreshold);

    return textureDistorted(tex, offset, direction, distortion) * weight;
}

// Apply ghost effect
vec4 applyGhost(in sampler2D tex, in vec2 texcoord, in int index, in vec2 ghostVec, in vec2 direction, in vec3 distortion) {
    vec2 offset = fract(texcoord + ghostVec * float(index));

    // Weight samples by falloff from image center
    // Bright spots in center can cast ghosts to edges, but not vice versa
    float weight = length(vec2(0.5) - offset) / length(vec2(0.5));
    weight = pow(1.0 - weight, uGhostThreshold);

    return textureDistorted(tex, offset, direction, distortion) * weight;
}

void main() {
    vec2 texcoord = vec2(1.0) - vTexCoord;  // Flip texture coordinates
    vec2 texelSize = 1.0 / textureSize(uBrightTexture, 0);

    // Chromatic distortion vector
    vec3 distortion = vec3(-texelSize.x * uGhostDistortion, 0.0, texelSize.x * uGhostDistortion);

    // Ghost vector to screen center (simulates internal lens reflections)
    // The algorithm creates artifacts symmetric to screen center, not sun position
    vec2 ghostVec = (vec2(0.5) - texcoord) * uGhostDispersal;
    vec2 direction = normalize(ghostVec);

    // Accumulate ghost/halo contributions
    vec4 result = vec4(0.0);

    for (int i = 0; i < uGhostCount; ++i) {
        if (uLensType == 0) {
            // Ghost only
            result += applyGhost(uBrightTexture, texcoord, i, ghostVec, direction, distortion);
        } else if (uLensType == 1) {
            // Halo only
            result += applyHalo(uBrightTexture, texcoord, ghostVec, direction, distortion);
        } else if (uLensType == 2) {
            // Both ghost and halo
            result += applyGhost(uBrightTexture, texcoord, i, ghostVec, direction, distortion);
            result += applyHalo(uBrightTexture, texcoord, ghostVec, direction, distortion);
        } else {
            // No effect
            result += texture(uBrightTexture, texcoord);
        }
    }

    // Apply lens texture modulation
    if (uUseLensTexture) {
        float lengthResult = length(vec2(0.5) - texcoord) / length(vec2(0.5));
        if (uLensType == 1) {
            result *= texture(uLensColorTexture, vec2(lengthResult));
        } else {
            result *= texture(uLensMaskTexture, vec2(lengthResult));
        }
    }

    fragColor = result;
}
