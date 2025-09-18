// glm
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <random>
#include <chrono>

// project
#include "cgra/cgra_geometry.hpp"
#include "cgra/cgra_mesh.hpp"
#include "perlin_noise.hpp"

using namespace std;
using namespace glm;
using namespace cgra;


void PerlinNoise::draw(const mat4& view, const mat4& proj) {
	// set up the shader for every draw call
	glUseProgram(shader);
	glUniformMatrix4fv(glGetUniformLocation(shader, "uProjectionMatrix"), 1, false, value_ptr(proj));
	glUniformMatrix4fv(glGetUniformLocation(shader, "uModelViewMatrix"), 1, false, value_ptr(view * modelTransform));
	glUniform3fv(glGetUniformLocation(shader, "uColor"), 1, value_ptr(color));
	// Recreate terrain then draw it.
	terrain = createMesh();
	terrain.draw();
}

gl_mesh PerlinNoise::createMesh() {
	vector<mesh_vertex> vertices(meshResolution * meshResolution); // Vector instead of 2D array uses the heap instead of the stack.
	mesh_builder mb;

	// Generate and store every vertex of the plane.
	for (int i = 0; i < meshResolution; ++i) {
        for (int j = 0; j < meshResolution; ++j) {
            // Get u and v offset on mesh and map to range -noiseSize to noiseSize for x and z.
            float u = i / (meshResolution - 1.0f);
            float v = j / (meshResolution - 1.0f);
            float x = (-1.0f + 2.0f * u) * meshSize;
            float z = (-1.0f + 2.0f * v) * meshSize;
            
            // Position, normal and uv of the vertex. Height is based on noise.
            float height = generatePerlinNoise(vec2(x, z));
            vec3 pos(x, height, z);
            vec2 uv(u, v);
            
            // Interpolate normal from slope angle between close neighbours in x and z directions.
            float delta = 1 / (meshResolution - 1.0f) * meshSize; // Distance between neighbouring vertices.
            float heightPosX = generatePerlinNoise(vec2(x + delta, z));
            float heightPosZ = generatePerlinNoise(vec2(x, z + delta));
            float heightNegX = generatePerlinNoise(vec2(x - delta, z));
            float heightNegZ = generatePerlinNoise(vec2(x, z - delta));
            vec3 tangentX = normalize(vec3(x + delta, heightPosX, z) - vec3(x - delta, heightNegX, z));
            vec3 tangentZ = normalize(vec3(x, heightPosZ, z + delta) - vec3(x, heightNegZ, z - delta));
            vec3 norm = normalize(cross(tangentX, tangentZ));
            
            // Each increase in i is a whole loop of j over meshResolution.
            int vertIndex = i * meshResolution + j;
            vertices[vertIndex] = mesh_vertex{pos, norm, uv};
        }
	}

	// Create the triangles. Ignore the final vertex (meshResolution - 1) as the quads/triangles are formed up to it.
	unsigned int index = 0;
	for (int i = 0; i < meshResolution - 1; ++i) {
		for (int j = 0; j < meshResolution - 1; ++j) {
			// Get next index along.
			int i2 = i + 1;
			int j2 = j + 1;
			// Offset i by the rows of j that have been passed already. This is to index the vector properly.
			int iOffset = i * meshResolution;
			int iOffset2 = i2 * meshResolution;

			// Get vertices. 2D array alternative: vertices[i][j].
			mesh_vertex topLeft = vertices[iOffset + j];
			mesh_vertex bottomLeft = vertices[iOffset + j2];
			mesh_vertex topRight = vertices[iOffset2 + j];
			mesh_vertex bottomRight = vertices[iOffset2 + j2];
			// Add the two sets of three vertices for the left triangle and right triangle.
			mesh_vertex triangleVertices[] = { topLeft, topRight, bottomLeft, bottomLeft, topRight, bottomRight };
			for (mesh_vertex v : triangleVertices) {
				mb.push_vertex(v);
			}

			// Add all the indices of the quad in a one liner, instead of doing push_index() 6 times.
			mb.push_indices({ index, index + 1, index + 2, index + 3, index + 4, index + 5 });
			index += 6;
		}
	}
	// Build and return gl_mesh.
	return mb.build();
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


float PerlinNoise::generatePerlinNoise(vec2 pos) {
	float noiseHeight = 0.0f;
	float maxHeight = 0.0f;
	float amplitude = 1.0f;
	float frequency = 1.0f;
	// Create perlin noise by combining noise octaves of doubling frequency and halving amplitude.
	for (int oct = 0; oct < noiseOctaves; oct++) {
		// TODO: Add (initially random) octave offsets onto pos so that each octave is sampled from a different noise area.
		frequency *= pow(2.0f, oct);
		noiseHeight += generateNoise(pos * noiseScale * frequency) * amplitude;
		maxHeight += amplitude;
		// Each octave has lower amplitude and frequency by the persistence nad lacunarity scales.
		amplitude *= noisePersistence;
		frequency *= noiseLacunarity;
	}
	// Normalise then scale by amplitude.
	float normalizedHeight = noiseHeight / maxHeight;
	float finalHeight = (normalizedHeight + 1.0f) * 0.5f * meshHeight; // Also map back to positive range.
	return finalHeight;
}
