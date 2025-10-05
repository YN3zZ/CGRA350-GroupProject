#version 330 core

uniform vec3 uColor;
uniform sampler2D uAlbedoTexture;
uniform sampler2D uNormalTexture;
uniform sampler2D uRoughnessTexture;
uniform sampler2D uMetallicTexture;
uniform vec3 uLightDir;
uniform vec3 lightColor;
uniform vec3 uViewPos;
uniform bool uUseTextures;

in VertexData {
    vec3 worldPos;
    vec3 normal;
    vec2 texCoord;
    mat3 TBN;
} f_in;

out vec4 fb_color;

void main() {
    // Sample textures
    vec3 color = texture(uAlbedoTexture, f_in.texCoord).rgb;

    // Normal mapping
    vec3 normalMap = texture(uNormalTexture, f_in.texCoord).rgb;
    normalMap = normalize(normalMap * 2.0 - 1.0);  // Convert from [0,1] to [-1,1]
    vec3 normal = normalize(f_in.TBN * normalMap);

    float roughness = texture(uRoughnessTexture, f_in.texCoord).r;
    float metallic = texture(uMetallicTexture, f_in.texCoord).r;

    // Blinn-Phong lighting with roughness
    vec3 lightDir = normalize(uLightDir);
    vec3 viewDir = normalize(uViewPos - f_in.worldPos);

    // Ambient
    float ambientStrength = 0.2f;
    vec3 ambient = lightColor * ambientStrength * color;

    // Diffuse
    float diff = max(dot(normal, lightDir), 0.0);
    vec3 diffuse = color * diff;

    // Specular (affected by roughness)
    vec3 halfwayDir = normalize(lightDir + viewDir);
    float spec = pow(max(dot(normal, halfwayDir), 0.0), mix(64.0, 4.0, roughness));
    vec3 specular = vec3(1.0) * spec * (1.0 - roughness) * 0.3;

    // Realistic bark rendering. Bark is not metallic, so its specular highlights should be white
    if (metallic > 0.5) {
        diffuse *= color;  // Metals tint their diffuse
        specular *= color; // And specular
    }

    // Combine lighting components
    vec3 result = ambient + diffuse + specular;
    fb_color = vec4(result, 1.0);
}