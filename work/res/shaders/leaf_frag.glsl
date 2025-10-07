#version 330 core

uniform sampler2D uLeafTexture;
uniform vec3 uLightDir;
uniform vec3 lightColor;
uniform vec3 uViewPos;

in vec2 vTexCoord;
in vec3 vNormal;
in vec3 vWorldPos;

out vec4 fragColor;

const float PI = 3.14159265358979f;

// Leaf material properties
const float roughness = 0.6f;
const float metallic = 0.0f;

// Calculate the microfacet detail made by geometric attenuation.
float microfacet(float NdotH, float VdotH, float NdotV_L) {
    // NdotV_L is the dot product of normDir with either viewDir or lightDir.
    return clamp((2.0 * NdotH * NdotV_L) / VdotH, 0.0f, 1.0f);
}

void main() {
    vec4 texColor = texture(uLeafTexture, vTexCoord);

    // Discard fully transparent pixels
    if (texColor.a < 0.1) {
        discard;
    }

    vec3 albedo = texColor.rgb;

    // PBR lighting (Cook-Torrance BRDF)
    vec3 normal = normalize(vNormal);
    vec3 lightDir = normalize(-uLightDir);
    vec3 viewDir = normalize(uViewPos - vWorldPos);
    vec3 halfAngle = normalize(lightDir + viewDir);

    // Calculate dot products and clamp to prevent division by 0
    float NdotL = max(dot(normal, lightDir), 0.001f);
    float NdotV = max(dot(normal, viewDir), 0.001f);
    float NdotH = max(dot(normal, halfAngle), 0.001f);
    float VdotH = max(dot(viewDir, halfAngle), 0.001f);

    // Ambient component
    float ambientStrength = 0.2f;
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

    fragColor = vec4(finalColor, texColor.a);
}
