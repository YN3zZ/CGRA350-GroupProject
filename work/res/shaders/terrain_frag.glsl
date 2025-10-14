#version 330 core

// uniform data
uniform mat4 uProjectionMatrix;
uniform mat4 uModelViewMatrix;
// User controlled uniforms.
uniform vec3 lightDirection;
uniform vec3 lightColor;
uniform float roughness; // 0=smoothest, 1=roughest. We avoid 0 or 1 (division by 0 error).
uniform float metallic; // 0=normal, 1=most metallic.
uniform bool useOrenNayar;
// Texture mapping.
uniform vec2 heightRange;
uniform float textureScale;
uniform sampler2D uTextures[8]; // Up to 8 textures.
uniform sampler2D uNormalMaps[8]; // Up to 8 normals.
uniform int numTextures; // How many have been set out of 8.
// Shadow mapping.
uniform sampler2DShadow uShadowMap;
uniform bool uEnableShadows;
uniform bool uUsePCF;
// Fog
uniform bool useFog;
uniform bool linearFog;
uniform float fogDensity;


// viewspace data (this must match the output of the fragment shader)
in VertexData {
	vec3 globalPos;
	vec3 globalNormal;
	vec3 position;
	vec3 normal;
	vec2 textureCoord;
	vec3 tangent; // Tangent and bitangent for normal mapping.
	vec3 bitangent;
	vec4 lightSpacePos;
} f_in;

// framebuffer output
out vec4 fb_color;


const float PI = 3.14159265358979f;

// Calculate the microfacet detail made by geometric attenuation.
float microfacet(float NdotH, float VdotH, float NdotV_L) {
	// NdotV_L is the dot product of normDir with either viewDir or lightDir.
	return clamp((2 * NdotH * NdotV_L) / VdotH, 0.0f, 1.0f);
}


float orenNayarDiffuse(vec3 normDir, vec3 lightDir, vec3 viewDir) {
	// Roughness angle in radians
	float sigma = roughness * PI / 2.0f;
    float sigma2 = sigma * sigma;

	// Get angles between surface normal and light direction
	float NdotL = max(dot(normDir, lightDir), 0.0);
    float NdotV = max(dot(normDir, viewDir), 0.0);
	float theta_r = acos(NdotV);
    float theta_i = acos(NdotL);

	// Then alpha is the max out of them and beta is the min. This accounts for how off-axis the view or light is.
	float alpha = max(theta_i, theta_r);
    float beta = min(theta_i, theta_r);

	// Oren Naymar constants that depend on the roughness of the surface.
	float A = 1.0f - (0.5f * sigma2 / (sigma2 + 0.33f));
    float B = 0.45f * (sigma2 / (sigma2 + 0.09f));
	
	// Tangent plane projections show how diffuse is affected by the relative rotation of light/view to normal direction.
	vec3 lightTangent = lightDir - normDir * NdotL;
	vec3 viewTangent = viewDir - normDir * NdotV;
	// If light and view are alligned this is 1, if they are perpendicular it is 0, reducing directness.
    float LdotV = max(dot(lightTangent, viewTangent), 0.0f);

	// Lambertian factor x (roughness coefficients x directness x relative view/light axis angles)
    return NdotL * (A + B * LdotV * sin(alpha) * tan(beta));
}


vec3 calculateNormal(vec3 normalMap) {
	// Map normal map to [-1, 1] range in tangent space.
	vec3 normalTangentSpace = normalize(normalMap * 2.0 - 1.0);

	// Build TBN matrix to transform from tangent space to view space.
	vec3 T = normalize(f_in.tangent);
	vec3 B = normalize(f_in.bitangent);
	vec3 N = normalize(f_in.normal);
	mat3 TBN = mat3(T, B, N);

	// Transform the normal from tangent space to view space
	vec3 perturbedNormal = normalize(TBN * normalTangentSpace);
	return perturbedNormal;
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

		float spacing = 0.5;

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

		// Map [0,1] to [0.05,1] for very dark shadows
		return clamp(shadow * 0.95 + 0.05, 0.05, 1.0);
	} else {
		// Hard shadows with hardware depth comparison
		return texture(uShadowMap, projCoords);
	}
}


// Blend textures from 3 axes instead of 1, for consistent texture tiling on stretched slopes.
vec3 triplanarSample(sampler2D tex, vec3 pos, vec3 norm) {
	// Normalize weights (absolute normal for blending)
	vec3 blending = normalize(abs(norm));
	blending /= (blending.x + blending.y + blending.z);

	// Scale texture coordinates for each pair of axes.
	vec2 xProj = pos.yz * textureScale;
	vec2 yProj = pos.xz * textureScale;
	vec2 zProj = pos.xy * textureScale;

	// Sample texture from each axis pair projection.
	vec3 xTex = texture(tex, xProj).rgb;
	vec3 yTex = texture(tex, yProj).rgb;
	vec3 zTex = texture(tex, zProj).rgb;

	// Blend results
	return xTex * blending.x + yTex * blending.y + zTex * blending.z;
}


float calculateFog() {
	float fogFactor;
	float dist = length(f_in.position);
	if (linearFog) {
		float fogMin = 0.1f;
		float fogMax = 1.5f / fogDensity;
		// Inverse linear min-max scaling so that far away is 0 and close is 1.
		fogFactor = (fogMax - dist) / (fogMax - fogMin);
	}
	else {
		// Expoential scaling.
		fogFactor = exp(-fogDensity * dist);
	}
	return clamp(fogFactor, 0.0f, 1.0f); // Does not exceed [0, 1] range.
}


void main() {
	// Getting height proportion to map texture color based on terrain height.
	float minHeight = heightRange.x;
	float maxHeight = heightRange.y;
	float heightProportion = smoothstep(minHeight, maxHeight, f_in.globalPos.y);
	
	// Scale height to the texture array.
	float scaledHeight = heightProportion * numTextures;
	// Combine the textures/normalMaps to an overall color based on height, smoothly transitioned.
	vec3 textureColor = vec3(0.0f);
	vec3 normalMap = vec3(0.0f);
	float totalWeight = 0.0f;
	for (int i = 0; i < numTextures; i++) {
		// Weight for how close the current height is to the middle of the textures band.
		float weight = max(1.0 - abs(scaledHeight - i - 0.5f), 0.0f);
		textureColor += triplanarSample(uTextures[i], f_in.globalPos, f_in.globalNormal) * weight;
		normalMap += triplanarSample(uNormalMaps[i], f_in.globalPos, f_in.globalNormal) * weight;
		totalWeight += weight;
	}
	// Normalize so the sum of contributions is 1 (solid texture to avoid light/dark patches).
	textureColor /= totalWeight;
	normalMap /= totalWeight;


	float ambientStrength = 0.15f;
	vec3 ambient = ambientStrength * lightColor * textureColor;

	vec3 normDir = calculateNormal(normalMap);
	vec3 viewDir = normalize(-f_in.position);
	vec3 lightDir = normalize(-lightDirection);
	vec3 halfAngle = normalize(lightDir + viewDir);

	// Calculuate dot products only once and clamp to 0.001, preventing division by 0.
	float NdotL = max(dot(normDir, lightDir), 0.001f);
	float NdotV = max(dot(normDir, viewDir), 0.001f);
	float NdotH = max(dot(normDir, halfAngle), 0.001f);
	float VdotH = max(dot(viewDir, halfAngle), 0.001f);

	float alpha2 = roughness * roughness;
	float theta = acos(NdotH);


	// D component (beckmans facet distribution).
	float beckman = exp(-pow(tan(theta), 2) / alpha2) / (PI * alpha2 * pow(cos(theta), 4));

	// G component (masking and shadowing of facets by one another).
	float attenuation = min(1, min(microfacet(NdotH, VdotH, NdotV), microfacet(NdotH, VdotH, NdotL)));

	// Materials index of refraction.
	float m = 1.5f - metallic; // Metals refract less.
	float reflectance = pow(m - 1, 2) / pow(m + 1, 2);
	// This is for dieletric (non-metals), so we want to interpolate based on the metallic factor.
	vec3 dielectricF0 = vec3(reflectance);
	vec3 f0 = mix(dielectricF0, textureColor, metallic);

	// F component (schlicks approximation of fresnel).
	vec3 schlick = f0 + (1 - f0) * pow(1 - VdotH, 5);

	// Specular stength = D * G * F / angle.
	vec3 rs = (beckman * attenuation * schlick) / (4.0f * NdotL * NdotV);
	vec3 specular = rs * lightColor;


	// normDir dot lightDir for how direct the diffuse is to the light.
	float diffuseFactor = NdotL;

	// Or use oren nayar diffuse.
	if (useOrenNayar) {
		diffuseFactor = orenNayarDiffuse(normDir, lightDir, viewDir);
	}

	// Reduce diffuse energy by specular reflectance (conservation of energy). Also metals don't have diffuse (shiny).
	vec3 kd = (2.0f - schlick) * (1.0f - metallic);
	// The diffuse uses the object color evenly scattered in all directions (using PI).
	vec3 diffuse = kd * (textureColor / PI) * diffuseFactor;

	// Calculate shadow visibility with slope-based bias
	float shadow = 1.0;
	if (uEnableShadows) {
		shadow = calculateShadow(f_in.lightSpacePos, normDir, lightDir);
	}

	// Calculate fog based on distance to camera.
	float fogFactor = useFog ? calculateFog() : 1.0f;
	// Desaturate light color for fog.
	float desaturated = 0.5f;
	vec3 fogColor = mix(lightColor, vec3(0.4f), desaturated);

	// Add ambient light to diffuse and specular, applying shadow to diffuse and specular only
	vec3 finalColor = ambient + shadow * (diffuse + specular);
	finalColor = mix(fogColor, finalColor, fogFactor); // Add fog.
	finalColor = clamp(finalColor, vec3(0.0f), vec3(1.0f)); // Ensure values dont exceed 0 to 1 range.

	fb_color = vec4(finalColor, 1.0f);
}