#version 330 core

// uniform data
uniform mat4 uProjectionMatrix;
uniform mat4 uModelViewMatrix;
// User controlled uniforms.
uniform vec3 lightDirection;
uniform vec3 lightColor;
uniform float roughness;
uniform float metallic;
uniform bool useOrenNayar;
// Texture mapping.
uniform float textureScale;
uniform float meshScale;
uniform float alpha;
// Current time for animating waves texture.
uniform float uTime;
uniform float waterSpeed;
uniform float waterAmplitude;
// A single texture and normal map.
uniform sampler2D uTexture;
uniform sampler2D uNormalMap;
// Shadow mapping
uniform sampler2DShadow uShadowMap;
uniform bool uEnableShadows;
uniform bool uUsePCF;
// Water reflection/refraction
uniform sampler2D uReflectionTexture;
uniform sampler2D uRefractionTexture;
uniform sampler2D uDuDvMap;
uniform bool uEnableReflections;
uniform float uWaveStrength;
uniform float uReflectionBlend;


// viewspace data (this must match the output of the fragment shader)
in VertexData{
	float displacement;
	vec3 position;
	vec3 normal;
	vec2 textureCoord;
	vec3 tangent; // Tangent and bitangent for normal mapping.
	vec3 bitangent;
	vec4 lightSpacePos;
	vec4 clipSpace;
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


float calculateFog() {
	float fogFactor;
	bool linearFog = false; // User controls this.
	float dist = length(f_in.position);
	if (linearFog) {
		float fogMin = 0.1;
		float fogMax = 30.0; // User controls this.
		// Inverse linear min-max scaling so that far away is 0 and close is 1.
		fogFactor = (fogMax - dist) / (fogMax - fogMin);
	}
	else {
		// Expoential scaling.
		float fogDensity = 0.02f; // User controls this.
		fogFactor = exp(-fogDensity * dist);
	}
	return clamp(fogFactor, 0.0f, 1.0f); // Does not exceed [0, 1] range.
}


void main() {
	// Sky color. May make controllable later.
	vec3 skyColor = vec3(0.5f, 0.65f, 0.8f);

	vec2 uv = f_in.textureCoord * textureScale;

	// Projective texture mapping for reflections/refractions
	vec2 ndc = (f_in.clipSpace.xy / f_in.clipSpace.w);
	vec2 refractCoords = ndc * 0.5 + 0.5;
	vec2 reflectCoords = vec2(refractCoords.x, 1.0 - refractCoords.y);

	// Texture wave animation. Might make these controllable in UI.
	float frequency = 1.0f;
	float amplitude = waterAmplitude;
	// Both UV x and y have waves. Some scrolling is added to make it move around. Gives the appeearance of moving water.
	float scrollAmount = uTime * waterSpeed * 0.01f;
	vec2 oldUV = uv.xy;
	uv.y += sin(cos(oldUV.x) * frequency + uTime * waterSpeed) * amplitude + scrollAmount/2;
	uv.x += cos(frequency/3 * uTime * waterSpeed) * amplitude + scrollAmount;

	// DuDv distortion for water waves with symmetric sampling to avoid directional bias
	float moveFactor = uTime * waterSpeed * 0.03f;

	// Use mod to ensure UVs wrap properly
	vec2 distortedUV1 = mod(f_in.textureCoord + vec2(moveFactor, moveFactor * 0.8), 1.0);
	vec2 distortedUV2 = mod(f_in.textureCoord - vec2(moveFactor * 0.7, moveFactor * 0.6), 1.0);

	// Sample DuDv map symmetrically and combine for distortion without directional drift
	vec2 distortion1 = (texture(uDuDvMap, distortedUV1).rg * 2.0 - 1.0);
	vec2 distortion2 = (texture(uDuDvMap, distortedUV2).rg * 2.0 - 1.0);
	vec2 distortion = (distortion1 + distortion2) * 0.5 * uWaveStrength;

	// Scale distortion for screen-space coordinates with less aggressive perspective correction
	float screenSpaceScale = 0.15f / max(f_in.clipSpace.w, 1.0);
	vec2 screenDistortion = distortion * screenSpaceScale;

	// Apply distortion and clamp to prevent edge artifacts
	reflectCoords = clamp(reflectCoords + screenDistortion, 0.001, 0.999);
	refractCoords = clamp(refractCoords + screenDistortion, 0.001, 0.999);

	// Combine the textures/normalMaps to an overall color based on height, smoothly transitioned.
	vec3 textureColor = texture(uTexture, uv).rgb;
	vec3 normalMap = texture(uNormalMap, uv).rgb;

	// Make deeper parts of waves darker.
	float proportion = (f_in.displacement / 4.0f) + 0.5f;
	textureColor = mix(textureColor * 0.7f, textureColor, proportion);

	// Ambient light.
	float ambientStrength = 0.15f;
	vec3 ambient = ambientStrength * lightColor * textureColor * skyColor;

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
	float m = 1.0 - metallic; // Metals refract less.
	float reflectance = pow(m - 1, 2) / pow(m + 1, 2);
	// This is for dieletric (non-metals), so we want to interpolate based on the metallic factor.
	vec3 dielectricF0 = vec3(reflectance);
	vec3 f0 = mix(dielectricF0, textureColor, metallic);

	// F component (schlicks approximation of fresnel).
	float fresnel = pow(1.0f - VdotH, 5.0f);
	vec3 schlick = f0 + (1.0f - f0) * fresnel;

	// Specular stength = D * G * F / angle.
	vec3 rs = (beckman * attenuation * schlick) / (4.0f * NdotL * NdotV);
	vec3 specular = rs * lightColor;

	// Sample reflection and refraction textures
	vec4 reflectionColor = vec4(skyColor, 1.0);
	vec4 refractionColor = vec4(textureColor, 1.0);

	if (uEnableReflections) {
		reflectionColor = texture(uReflectionTexture, reflectCoords);
		refractionColor = texture(uRefractionTexture, refractCoords);
	}

	float fresnelFactor = pow(1.0f - NdotV, 2.0f);
	vec3 environmentColor = mix(refractionColor.rgb, reflectionColor.rgb, fresnelFactor);


	// normDir dot lightDir for how direct the diffuse is to the light.
	float diffuseFactor = NdotL;

	// Or use oren nayar diffuse.
	if (useOrenNayar) {
		diffuseFactor = orenNayarDiffuse(normDir, lightDir, viewDir);
	}

	// Reduce diffuse energy by specular reflectance (conservation of energy). Also metals don't have diffuse (shiny).
	vec3 kd = (2.0f - schlick) * (1.0f - metallic);
	// The diffuse uses the object color evenly scattered in all directions (using PI). Adds sky color.
	vec3 diffuse = kd * (skyColor * ambientStrength + (textureColor / PI) * diffuseFactor);

	// Calculate shadow visibility with slope-based bias
	float shadow = 1.0;
	if (uEnableShadows) {
		shadow = calculateShadow(f_in.lightSpacePos, normDir, lightDir);
	}

	// Calculate fog based on distance to camera.
	float fogFactor = calculateFog();
	// Desaturate light color for fog.
	float desaturated = 0.5f;
	vec3 fogColor = mix(lightColor, vec3(0.5f), desaturated);


	// Add ambient light to diffuse and specular, applying shadow to diffuse and specular only
	vec3 finalColor = ambient + shadow * (diffuse + specular);
	finalColor = mix(fogColor, finalColor, fogFactor); // Add fog.

	if (uEnableReflections) {
		// Mix environment color with PBR lighting, ReflectionBlend controls how much
		finalColor = mix(environmentColor, finalColor, 1.0 - uReflectionBlend) + specular * 0.5;
	}

	finalColor = clamp(finalColor, vec3(0.0f), vec3(1.0f)); // Ensure values dont exceed 0 to 1 range.

	fb_color = vec4(finalColor, alpha);
}