#version 330 core

// uniform data
uniform mat4 uProjectionMatrix;
uniform mat4 uModelViewMatrix;
uniform mat4 uLightSpaceMatrix;
// Current time for animating waves displacement.
uniform float uTime;
uniform float meshScale;
uniform float waterSpeed;
uniform float waterAmplitude;
// Collision with terrain.
uniform float waterHeight;
uniform vec2 terrainHeightRange;
uniform sampler2D uTerrainHeightMap;

// mesh data
layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 uv;

// model data (this must match the input of the fragment shader)
out VertexData{
    float displacement;
	vec3 position;
	vec3 normal;
	vec2 textureCoord;
    vec3 tangent; // "Tangent vectors" required for normal mapping.
    vec3 bitangent; // Part of TBN matrix structure mentioned in lecture.
	vec4 lightSpacePos;
	vec4 clipSpace; // Projective texture mapping
} v_out;

void main() {
    // Calculate terrain height relative to water height for interaction.
    float terrainHeight = texture(uTerrainHeightMap, uv.yx).r;

    // Make the shoreline have less movement.
    float distance = abs(terrainHeight - waterHeight);
    float depthFactor = clamp(distance/2, 0.0, 1.0f); // 0 is shore, 1 is deep.

    float baseHeight = mix(terrainHeightRange.x, terrainHeightRange.y, waterHeight); // Proportion of terrain height.
    // Wave displacement animation. Scales by mesh size.
    float frequency = 120.0f;
    float amplitude = waterAmplitude * depthFactor;
    float speed = waterSpeed * 4.0f * depthFactor;
    float displacement = cos(sin(uv.x) * frequency + uTime * speed) + sin(uv.y * frequency/2.0f + uTime * speed);
    vec3 newPosition = aPosition + vec3(0, baseHeight + displacement * amplitude, 0);
    // Store displacement and for depth coloring.
    v_out.displacement = displacement;

    // Get nearby gradient to recalculate normals (central difference derivative).
    float delta = 0.0001f;
    float gradX = cos(sin(uv.x + delta) * frequency + uTime * speed) - cos(sin(uv.x - delta) * frequency + uTime * speed);
    float gradY = sin((uv.y + delta) * frequency/2.0f + uTime * speed) - sin((uv.y - delta) * frequency/2.0f + uTime * speed);
    vec3 newNormal = normalize(vec3(-gradX, 1.0f, -gradY));

	// transform vertex data to viewspace
	v_out.position = (uModelViewMatrix * vec4(newPosition, 1)).xyz;
	v_out.normal = normalize((uModelViewMatrix * vec4(newNormal, 0)).xyz);
	v_out.textureCoord = uv;

    // Tangent on the x-axis for grid-based heightmap terrain.
    vec3 tangent = vec3(1.0f, 0.0f, 0.0f);

    // Lecture: need proper orthogonal basis for TBN matrix
    tangent = normalize(tangent - dot(tangent, newNormal) * newNormal);
    vec3 bitangent = cross(newNormal, tangent);

    // lecture: "Consider using a (tangent, bitangent and normal) TBN matrix structure for transformations"
    v_out.tangent = normalize((uModelViewMatrix * vec4(tangent, 0.0)).xyz);
    v_out.bitangent = normalize((uModelViewMatrix * vec4(bitangent, 0.0)).xyz);

	// Calculate light space position for shadow mapping
	v_out.lightSpacePos = uLightSpaceMatrix * vec4(newPosition, 1.0);

    // Set the screenspace position (needed for converting to fragment data)
    gl_Position = uProjectionMatrix * uModelViewMatrix * vec4(newPosition, 1.0f);

	// Store clip space position for projective texture mapping in fragment shader
	v_out.clipSpace = gl_Position;
}