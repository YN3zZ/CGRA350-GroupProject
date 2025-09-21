#version 330 core

// uniform data
uniform mat4 uProjectionMatrix;
uniform mat4 uModelViewMatrix;
uniform vec3 uColor;
// User controlled uniforms.
uniform vec3 lightDirection;
uniform vec3 lightColor;
uniform float roughness; // 0=smoothest, 1=roughest. We avoid 0 or 1 (division by 0 error).
uniform float metallic; // 0=normal, 1=most metallic.
uniform bool useOrenNayar;
// Texture mapping.
uniform vec2 heightRange;
uniform sampler2D textureSampler;

// viewspace data (this must match the output of the fragment shader)
in VertexData {
	vec3 globalPos;
	vec3 position;
	vec3 normal;
	vec2 textureCoord;
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


void main() {
	// Getting height proportion to map texture color based on terrain height.
	float minHeight = heightRange.x;
	float maxHeight = heightRange.y;
	float heightProportion = smoothstep(minHeight, maxHeight, f_in.globalPos.y);
	
	//vec3 rockColor = vec3(0.74f, 0.73f, 0.77f);
	//vec3 textureColor = mix(uColor, rockColor, heightProportion);

	vec3 textureColor = texture(textureSampler, f_in.textureCoord).rgb;


	float ambientStrength = 0.1f;
	vec3 ambient = ambientStrength * lightColor * textureColor;

	vec3 normDir = normalize(f_in.normal);
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
	
	// Add ambient light to diffuse and specular.
	vec3 finalColor = ambient + diffuse + specular;
	finalColor = clamp(finalColor, vec3(0.0f), vec3(1.0f)); // Ensure values dont exceed 0 to 1 range.

	fb_color = vec4(finalColor, 1.0f);
}