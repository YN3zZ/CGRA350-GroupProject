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
// Shadow mapping
uniform sampler2D uShadowMap;
uniform bool uEnableShadows;
uniform bool uUsePCF;

in VertexData {
    vec3 worldPos;
    vec3 normal;
    vec2 texCoord;
    mat3 TBN;
	vec4 lightSpacePos;
} f_in;

out vec4 fb_color;

const float PI = 3.14159265358979f;

// Calculate the microfacet detail made by geometric attenuation.
float microfacet(float NdotH, float VdotH, float NdotV_L) {
    // NdotV_L is the dot product of normDir with either viewDir or lightDir.
    return clamp((2.0 * NdotH * NdotV_L) / VdotH, 0.0f, 1.0f);
}

float calculateShadow(vec4 lightSpacePos, vec3 normal, vec3 lightDir) {
	// Perform perspective divide
	vec3 projCoords = lightSpacePos.xyz / lightSpacePos.w;

	// Transform to [0,1] range
	projCoords = projCoords * 0.5 + 0.5;

	// Outside shadow map bounds = no shadow
	if (projCoords.z > 1.0 || projCoords.x < 0.0 || projCoords.x > 1.0 || projCoords.y < 0.0 || projCoords.y > 1.0) {
		return 1.0;
	}

	// Get depth from light's perspective
	float currentDepth = projCoords.z;

	// Hardcoded bias to 0
	float bias = 0.0;

	if (uUsePCF) {
		// PCF: 3x3 kernel soft shadows
		float shadow = 0.0;
		vec2 texelSize = 1.0 / textureSize(uShadowMap, 0);
		for (int x = -1; x <= 1; ++x) {
			for (int y = -1; y <= 1; ++y) {
				float pcfDepth = texture(uShadowMap, projCoords.xy + vec2(x, y) * texelSize).r;
				shadow += currentDepth - bias > pcfDepth ? 0.0 : 1.0;
			}
		}
		shadow /= 9.0;
		return shadow;
	} else {
		// Hard shadows
		float closestDepth = texture(uShadowMap, projCoords.xy).r;
		return currentDepth - bias > closestDepth ? 0.0 : 1.0;
	}
}

void main() {
    // Sample textures
    vec3 albedo = texture(uAlbedoTexture, f_in.texCoord).rgb;

    // Normal mapping
    vec3 normalMap = texture(uNormalTexture, f_in.texCoord).rgb;
    normalMap = normalize(normalMap * 2.0 - 1.0);  // Convert from [0,1] to [-1,1]
    vec3 normal = normalize(f_in.TBN * normalMap);

    float roughness = texture(uRoughnessTexture, f_in.texCoord).r;
    float metallic = texture(uMetallicTexture, f_in.texCoord).r;

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

	// Calculate shadow visibility with slope-based bias
	float shadow = 1.0;
	if (uEnableShadows) {
		shadow = calculateShadow(f_in.lightSpacePos, normal, lightDir);
	}

    // Combine lighting components, applying shadow to diffuse and specular only
    vec3 finalColor = ambient + shadow * (diffuse + specular);
    finalColor = clamp(finalColor, vec3(0.0f), vec3(1.0f));

    fb_color = vec4(finalColor, 1.0);
}