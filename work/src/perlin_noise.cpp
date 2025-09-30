// glm
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <random>
#include <chrono>

// project
#include "cgra/cgra_geometry.hpp"
#include "cgra/cgra_mesh.hpp"
#include "perlin_noise.hpp"
#include "cgra/cgra_image.hpp"

using namespace std;
using namespace glm;
using namespace cgra;


const vector<string> textureNames = { "sandyground1", "patchy-meadow1", "slatecliffrock" };

// Initially generate the mesh and load the textures. Initialise shader and color immediately.
PerlinNoise::PerlinNoise() {
	// Load all the textures using the path strings.
	for (int i = 0; i < textureNames.size(); i++) {
		string pathStart = CGRA_SRCDIR + string("/res/textures/") + textureNames[i];
		rgba_image textureImage = rgba_image(pathStart + string("_albedo.png"));
		rgba_image normalImage = rgba_image(pathStart + string("_normal.png"));
		textures.push_back(textureImage.uploadTexture());
		normalMaps.push_back(normalImage.uploadTexture());
	}
}


// Set textures, normals, scale and height params.
void PerlinNoise::setShaderParams() {
	// Only update uniforms for texture and textureSize when mesh is updated.
	glUseProgram(shader);
	int textureCount = textureNames.size();
	for (int i = 0; i < textureCount; i++) {
		// Textures
		glActiveTexture(GL_TEXTURE0 + i); // GL_TEXTURE0 = 0x84C0 = 33984. Add 1 for each subsequent location.
		glBindTexture(GL_TEXTURE_2D, textures[i]);
		// Access array element of uniform textures shader parameter.
		string name = "uTextures[" + to_string(i) + "]";
		glUniform1i(glGetUniformLocation(shader, name.c_str()), i);

		// Normal Maps
		glActiveTexture(GL_TEXTURE12 + i);
		glBindTexture(GL_TEXTURE_2D, normalMaps[i]);
		// Access array element of uniform textures shader parameter.
		name = "uNormalMaps[" + to_string(i) + "]";
		glUniform1i(glGetUniformLocation(shader, name.c_str()), 12 + i);
	}
	glUniform1i(glGetUniformLocation(shader, "numTextures"), textureCount);
	glUniform1f(glGetUniformLocation(shader, "textureScale"), meshScale / (5.0f * textureScale));

	// Send uniform for height range and model color terrain coloring.
	glUniform2fv(glGetUniformLocation(shader, "heightRange"), 1, value_ptr(getHeightRange()));
}


// Get the min and max height of the terrain for texturing.
vec2 PerlinNoise::getHeightRange() {
	// Min and max height initially are the height of the first vertex.
	vec2 heightRange(vertices[0].pos.y);
	for (int i = 1; i < vertices.size(); i++) {
		float vertHeight = vertices[i].pos.y;
		// Adjust the min/max if any vertex is lower/higher.
		if (heightRange.x > vertHeight) {
			heightRange.x = vertHeight;
		}
		if (heightRange.y < vertHeight) {
			heightRange.y = vertHeight;
		}
	}
	return heightRange;
}


void PerlinNoise::draw(const mat4& view, const mat4& proj) {
	// set up the shader for every draw call
	glUseProgram(shader);
	// Set model, view and projection matrices.
	glUniformMatrix4fv(glGetUniformLocation(shader, "uProjectionMatrix"), 1, false, value_ptr(proj));
	glUniformMatrix4fv(glGetUniformLocation(shader, "uModelViewMatrix"), 1, false, value_ptr(view * modelTransform));

	// Lighting params
	glUniform3fv(glGetUniformLocation(shader, "lightDirection"), 1, value_ptr(lightDirection));
	glUniform3fv(glGetUniformLocation(shader, "lightColor"), 1, value_ptr(lightColor));
	glUniform1f(glGetUniformLocation(shader, "roughness"), roughness);
	glUniform1f(glGetUniformLocation(shader, "metallic"), metallic);
	glUniform1i(glGetUniformLocation(shader, "useOrenNayar"), useOrenNayar ? 1 : 0);

	// Draw the terrain mesh.
	terrain.draw();
}


// Create terrain mesh using perlin noise heightmap.
void PerlinNoise::createMesh() {
	// Randomiser based on the user-controlled seed.
	mt19937 randomiser(noiseSeed);
	uniform_int_distribution<int> distribution(0, 10000); // Min to max.
	// Generates 2 random floats to make a vec2 to offset each octave, eliminating repeated patterns.
	vector<vec2> octaveOffsets(noiseOctaves);
	for (int oct = 0; oct < noiseOctaves; oct++) {
		octaveOffsets[oct] = vec2(distribution(randomiser), distribution(randomiser));
	}

	// Calculate the vertex positions using perlin noise.
	// 1 vertex on either side of padding for normals to be smooth around the edge of the terrain.
	int padResolution = meshResolution + 2;
	vector<vec3> vertexPositions(padResolution * padResolution);
	for (int i = 0; i < padResolution; ++i) {
		for (int j = 0; j < padResolution; ++j) {
			// Get u and v offset on mesh and map to range -noiseSize to noiseSize for x and z.
			float u = i / (padResolution - 1.0f);
			float v = j / (padResolution - 1.0f);
			// Rescale the mesh to the original size after padding.
			float rescaling = (padResolution - 1.0f) / (meshResolution - 1.0f);
			float x = (-1.0f + 2.0f * u) * meshScale * rescaling;
			float z = (-1.0f + 2.0f * v) * meshScale * rescaling;

			// Position, normal and uv of the vertex. Height is based on noise.
			float height = generatePerlinNoise(vec2(x, z), octaveOffsets);
			int vertIndex = i * padResolution + j;
			vertexPositions[vertIndex] = vec3(x, height, z);
		}
	}

	// Create the vertices which have positions, normals and UVs.
	vertices = vector<mesh_vertex>(meshResolution * meshResolution);
	for (int i = 0; i < meshResolution; ++i) {
		for (int j = 0; j < meshResolution; ++j) {
			// Get u and v offset on mesh and map to range -noiseSize to noiseSize for x and z.
			float u = i / (meshResolution - 1.0f);
			float v = j / (meshResolution - 1.0f);

			// Each increase in i is a whole loop of j over meshResolution.
			int vertIndex = i * meshResolution + j;
			int padIndex = (i + 1) * padResolution + (j + 1); // Padding so normals don't go off edge of terrain.
			vec3 pos = vertexPositions[padIndex];
			vec2 uv(u, v);

			// Interpolate normal from slope angle between vertex neighbours in x and z directions.
			vec3 tangentX = normalize(vertexPositions[padIndex + 1] - vertexPositions[padIndex - 1]);
			vec3 tangentZ = normalize(vertexPositions[padIndex + padResolution] - vertexPositions[padIndex - padResolution]);
			vec3 norm = normalize(cross(tangentX, tangentZ));

			vertices[vertIndex] = mesh_vertex{ pos, norm, uv };
		}
	}
	
	// Create the triangles. Ignore the final vertex (meshResolution - 1) as the quads/triangles are formed up to it.
	mesh_builder mb;
	unsigned int index = 0;
	for (int i = 0; i < meshResolution - 1; ++i) {
		for (int j = 0; j < meshResolution - 1; ++j) {
			// Offset i by the rows of j that have been passed already. This is to index the vector properly.
			int iOffset = i * meshResolution;

			// Get vertices. 2D array alternative: vertices[i][j] to vertices[i+1][j+1].
			mesh_vertex topLeft = vertices[iOffset + j];
			mesh_vertex bottomLeft = vertices[iOffset + (j + 1)];
			mesh_vertex topRight = vertices[(iOffset + meshResolution) + j];
			mesh_vertex bottomRight = vertices[(iOffset + meshResolution) + (j + 1)];
			// Add the two sets of three vertices for the left triangle and right triangle.
			for (mesh_vertex v : { topLeft, topRight, bottomLeft, bottomLeft, topRight, bottomRight }) {
				mb.push_vertex(v);
			}

			// Add all the indices of the quad in a one liner, instead of doing push_index() 6 times.
			mb.push_indices({ index, index + 1, index + 2, index + 3, index + 4, index + 5 });
			index += 6;
		}
	}
	// Build and set gl_mesh.
	terrain = mb.build();
}


inline vec2 randGradient(vec2 v) {
	// Create a hash then bitshift it with XOR to further randomise.
	int n = int(v.x) * 17 + int(v.y) * 57;
	n = (n << 13) ^ n;
	// 2147483647 is max int, so bitwise AND is used with it to force a positive integer by dropping the (positive/negative) sign bit.
	float num = ((n * (n * n * 255179 + 98712751) + 1576546427) & 2147483647) / 2147483647.0f; // Divided by max int (as float) to give [0.0, 1.0] range.

	// Next convert the random number from [0.0, 1.0] into an angle from [0.0, 2PI] for a circular gradient.
	float angle = num * glm::two_pi<float>();
	return vec2(cos(angle), sin(angle));
}


// Inspiration from: https://gist.github.com/patriciogonzalezvivo/670c22f3966e662d2f83
float PerlinNoise::generateNoise(vec2 pos) {
	// Use fractional component of position to make it smoother closer to vertices.
	vec2 gridPos = floor(pos);
	vec2 posFrac = pos - gridPos;
	vec2 smooth = posFrac * posFrac * (3.0f - 2.0f * posFrac);

	// Gradients of the four grid corners relative to this position.
	float bl = dot(randGradient(gridPos), posFrac);
	float br = dot(randGradient(gridPos + vec2(1, 0)), posFrac - vec2(1, 0));
	float tl = dot(randGradient(gridPos + vec2(0, 1)), posFrac - vec2(0, 1));
	float tr = dot(randGradient(gridPos + vec2(1, 1)), posFrac - vec2(1, 1));

	// Bilinear interpolation using smooth/fade.
    return mix(mix(bl, br, smooth.x), mix(tl, tr, smooth.x), smooth.y);
}


float PerlinNoise::generatePerlinNoise(vec2 pos, const vector<vec2> &octaveOffsets) {
	float noiseHeight = 0.0f;
	float maxHeight = 0.0f;
	float amplitude = 1.0f;
	float frequency = 1.0f;
	// Create perlin noise by combining noise octaves of doubling frequency and halving amplitude.
	for (int oct = 0; oct < noiseOctaves; oct++) {
		// Uses seeded octave offsets, noise scale and diminishing frequency and amplitude per octave.
		noiseHeight += generateNoise((pos + octaveOffsets[oct]) * noiseScale * frequency) * amplitude;
		maxHeight += amplitude;
		// Each octave has lower amplitude and higher frequency by the persistence and lacunarity scales.
		amplitude *= noisePersistence;
		frequency *= noiseLacunarity;
	}
	// Normalise then scale by amplitude.
	float normalizedHeight = noiseHeight / maxHeight;
	float finalHeight = (normalizedHeight + 1.0f) * 0.5f * meshHeight; // Also map back to positive range.
	return finalHeight;
}


// For drawing trees on the terrain.
vec3 PerlinNoise::sampleVertex(vec2 position) {
	int vertCount = meshResolution - 1;
	float u = ((position.x + 0.5f) / meshScale + 1.0f) * 0.5f;
	float v = ((position.y + 0.5f) / meshScale + 1.0f) * 0.5f;

	int i = std::clamp(int(u * (meshResolution - 1)), 0, meshResolution - 1);
	int j = std::clamp(int(v * (meshResolution - 1)), 0, meshResolution - 1);

	int index = i * meshResolution + j;
	return vertices[index].pos;
}