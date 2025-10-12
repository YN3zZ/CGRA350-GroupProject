#version 330 core

// Based on: http://john-chapman-graphics.blogspot.com/2013/02/pseudo-lens-flare.html

// Input from vertex shader
in vec2 vTexCoord;

// Uniforms
uniform sampler2D uSceneTexture;      // Original scene
uniform sampler2D uFlareTexture;      // Ghost/halo artifacts
uniform sampler2D uBloomTexture;      // Bloom glow
uniform sampler2D uLensDirtTexture;   // Lens dirt overlay
uniform sampler2D uLensStarTexture;   // Starburst texture
uniform bool uUseDirt;
uniform float uGlobalBrightness;
uniform mat3 uLensStarMatrix;         // Matrix to rotate starburst based on camera
uniform bool uEnableBloom;
uniform float uBloomStrength;

// Output
out vec4 fragColor;

void main() {
    vec2 uv = vTexCoord;

    // Sample textures
    vec4 sceneColor = texture(uSceneTexture, uv);
    vec4 lensFlare = texture(uFlareTexture, uv);
    vec4 bloom = texture(uBloomTexture, uv);

    if (uUseDirt) {
        // Add lens dirt and starburst effects
        vec4 lensDirt = texture(uLensDirtTexture, uv);

        // Transform UV coordinates for rotating starburst
        vec2 lensStarTexcoord = (uLensStarMatrix * vec3(uv, 1.0)).xy;
        vec4 lensStarburst = texture(uLensStarTexture, lensStarTexcoord);

        // Modulate flare with dirt and starburst
        lensFlare *= (lensDirt + lensStarburst);
    }

    // Combine scene with lens flare and bloom
    vec4 finalColor = sceneColor + lensFlare * uGlobalBrightness;

    // Add bloom effect if enabled
    if (uEnableBloom) {
        finalColor += bloom * uBloomStrength;
    }

    fragColor = finalColor;
}
