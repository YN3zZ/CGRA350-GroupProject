#version 330 core

uniform vec3 uColor;
uniform sampler2D uAlbedoTexture;
uniform sampler2D uNormalTexture;
uniform sampler2D uRoughnessTexture;
uniform sampler2D uMetallicTexture;
uniform vec3 uLightDir;
uniform vec3 uViewPos;
uniform bool uUseTextures;

in VertexData {
    vec3 worldPos;
    vec3 normal;
    vec2 texCoord;
    mat3 TBN;
} f_in;

out vec4 fb_color;

const float PI = 3.14159265358979f;

// Calculate the microfacet detail made by geometric attenuation.
float microfacet(float NdotH, float VdotH, float NdotV_L) {
    // NdotV_L is the dot product of normDir with either viewDir or lightDir.
    return clamp((2.0 * NdotH * NdotV_L) / VdotH, 0.0f, 1.0f);
}

void main() {
    vec3 color;
    vec3 normal;
    float roughness = 0.8;
    float metallic = 0.0;

    // Sample textures
    vec3 albedo = texture(uAlbedoTexture, f_in.texCoord).rgb;

    // Normal mapping
    vec3 normalMap = texture(uNormalTexture, f_in.texCoord).rgb;
    normalMap = normalize(normalMap * 2.0 - 1.0);  // Convert from [0,1] to [-1,1]
    normal = normalize(f_in.TBN * normalMap);

    roughness = texture(uRoughnessTexture, f_in.texCoord).r;
    metallic = texture(uMetallicTexture, f_in.texCoord).r;

    // PBR lighting (Cook-Torrance BRDF)
    vec3 lightDir = normalize(-uLightDir);
    vec3 viewDir = normalize(uViewPos - f_in.worldPos);
    vec3 halfAngle = normalize(lightDir + viewDir);

    // Calculate dot products and clamp to prevent division by 0
    float NdotL = max(dot(normal, lightDir), 0.001f);
    float NdotV = max(dot(normal, viewDir), 0.001f);
    float NdotH = max(dot(normal, halfAngle), 0.001f);
    float VdotH = max(dot(viewDir, halfAngle), 0.001f);

    // Ambient component
    float ambientStrength = 0.15f;
    vec3 ambient = ambientStrength * lightColor * albedo;

    // D component (Beckmann's facet distribution)
    float alpha2 = roughness * roughness;
    float theta = acos(NdotH);
    float beckman = exp(-pow(tan(theta), 2.0) / alpha2) / (PI * alpha2 * pow(cos(theta), 4.0));

    // G component (masking and shadowing of facets by one another)
    float attenuation = min(1.0, min(microfacet(NdotH, VdotH, NdotV), microfacet(NdotH, VdotH, NdotL)));

    // F component (Fresnel - Schlick's approximation)
    // Materials index of refraction
    float m = 1.5f - metallic; // Metals refract less
    float reflectance = pow(m - 1.0, 2.0) / pow(m + 1.0, 2.0);
    vec3 dielectricF0 = vec3(reflectance);
    vec3 f0 = mix(dielectricF0, albedo, metallic);
    vec3 schlick = f0 + (1.0 - f0) * pow(1.0 - VdotH, 5.0);

    // Specular strength = D * G * F / angle
    vec3 rs = (beckman * attenuation * schlick) / (4.0f * NdotL * NdotV);
    vec3 specular = rs * lightColor;

    // Diffuse component with energy conservation
    vec3 kd = (2.0f - schlick) * (1.0f - metallic);
    vec3 diffuse = kd * (albedo / PI) * NdotL;

    // Combine lighting components
    vec3 finalColor = ambient + diffuse + specular;
    finalColor = clamp(finalColor, vec3(0.0f), vec3(1.0f));

    fb_color = vec4(finalColor, 1.0);
}