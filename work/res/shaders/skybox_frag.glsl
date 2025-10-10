#version 330 core
out vec4 FragColor;

in vec3 TexCoords;

uniform samplerCube skybox;
uniform vec3 atmosphereColor;  // Sky tint color based on sun position
uniform float atmosphereBlend; // How much to blend

void main() {
    vec4 skyboxColor = texture(skybox, TexCoords);

    // Blend skybox texture
    vec3 finalColor = mix(skyboxColor.rgb, atmosphereColor, atmosphereBlend);

    FragColor = vec4(finalColor, 1.0);
}
