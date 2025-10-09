#version 330 core

uniform sampler2D uLeafTexture;
uniform vec3 uLightDir;
uniform vec3 lightColor;
uniform vec3 uViewPos;
// Shadow mapping
uniform sampler2DShadow uShadowMap;
uniform bool uEnableShadows;
uniform bool uUsePCF;

in vec2 vTexCoord;
in vec3 vNormal;
in vec3 vWorldPos;
in vec4 vLightSpacePos;

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

float calculateShadow(vec4 lightSpacePos, vec3 normal, vec3 lightDir) {
	// Perform perspective divide
	vec3 projCoords = lightSpacePos.xyz / lightSpacePos.w;

	// Transform from NDC [-1,1] to texture coordinates [0,1]
	projCoords = projCoords * 0.5 + 0.5;

	// Outside shadow map bounds = no shadow
	if (projCoords.z > 1.0 || projCoords.x < 0.0 || projCoords.x > 1.0 || projCoords.y < 0.0 || projCoords.y > 1.0) {
		return 1.0;
	}

	if (uUsePCF) {
		// Improved PCF with adaptive spacing and hardware depth comparison
		float shadow = 0.0;
		vec2 texelSize = 1.0 / vec2(textureSize(uShadowMap, 0));

		// Adaptive spacing: tighter at high res, wider at low res
		float resolution = float(textureSize(uShadowMap, 0).x);
		float spacing = 10.0 - resolution / 512.0;

		// Variable kernel size (using 1 for 3x3)
		int pcfSize = 1;

		for (int x = -pcfSize; x <= pcfSize; ++x) {
			for (int y = -pcfSize; y <= pcfSize; ++y) {
				vec2 offset = projCoords.xy + vec2(x, y) * spacing * texelSize;
				// Hardware depth comparison: returns 1.0 if lit, 0.0 if shadowed
				shadow += texture(uShadowMap, vec3(offset, projCoords.z));
			}
		}

		int kernelSize = pcfSize * 2 + 1;
		shadow /= float(kernelSize * kernelSize);

		// Map [0,1] to [0.5,1], shadows never fully black
		return clamp(shadow * 0.5 + 0.5, 0.5, 1.0);
	} else {
		// Hard shadows with hardware depth comparison
		return texture(uShadowMap, projCoords);
	}
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

	// Calculate shadow visibility with slope-based bias
	float shadow = 1.0;
	if (uEnableShadows) {
		shadow = calculateShadow(vLightSpacePos, normal, lightDir);
	}

    // Combine lighting components, applying shadow to diffuse and specular only
    vec3 finalColor = ambient + shadow * (diffuse + specular);
    finalColor = clamp(finalColor, vec3(0.0f), vec3(1.0f));

    fragColor = vec4(finalColor, texColor.a);
}
