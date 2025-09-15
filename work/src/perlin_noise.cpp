// glm
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <random>
#include <chrono>

// project
#include "cgra/cgra_geometry.hpp"
#include "perlin_noise.hpp"

using namespace std;
using namespace glm;
using namespace cgra;


void perlin_noise::draw(const mat4& view, const mat4& proj) {
	// set up the shader for every draw call
	glUseProgram(shader);
	glUniformMatrix4fv(glGetUniformLocation(shader, "uProjectionMatrix"), 1, false, value_ptr(proj));
	glUniform3fv(glGetUniformLocation(shader, "uColor"), 1, value_ptr(color));

	// Visualise noise temporarily with spheres.
	for (int i = 0; i < 100; i++) {
		for (int j = 0; j < 100; j++) {
			float x = i / 6.0f;
			float z = j / 6.0f;
			// Transform height of sphere by noise at position.
			vec2 pos(x, z);
			float height = generatePerlinNoise(pos, 6, 0.8f, 4.0f);
			mat4 transform = translate(mat4(1.0f), vec3(x, height * 4, z)) *
							scale(mat4(1.0f), vec3(0.08f));
			glUniformMatrix4fv(glGetUniformLocation(shader, "uModelViewMatrix"), 1, false, value_ptr(view * transform));
			drawSphere();
		}
	}
	
}

inline vec2 smoothstep(vec2 t) {
	return t * t * (3.0f - 2.0f * t);
}

inline float random(vec2 p) {
	// Create a hash then bitshift it with XOR to further randomise. Finally 
	int n = int(p.x) + int(p.y) * 57;
	n = (n << 13) ^ n;
	// 2147483647 is max int, so bitwise AND is used with it to force a positive integer by dropping the (positive/negative) sign bit.
	return float((n * (n * n * 255179 + 98712751) + 1576546427) & 2147483647) / 2147483647.0f; // Divided by max int to give [0, 1] range.
}

// Inspiration from: https://gist.github.com/patriciogonzalezvivo/670c22f3966e662d2f83
float perlin_noise::generateNoise(vec2 pos) {
	// Use fractional component of position to make it smoother closer to vertices.
	vec2 gridPos = floor(pos);
	vec2 posFrac = pos - gridPos;
	vec2 smooth = smoothstep(posFrac);

	// The four grid corners relative to this position.
	float bl = random(gridPos);
	float br = random(gridPos + vec2(1, 0));
	float tl = random(gridPos + vec2(0, 1));
	float tr = random(gridPos + vec2(1, 1));
	
	return lerp(lerp(bl, br, smooth.x), lerp(tl, tr, smooth.x), smooth.y);
}

float perlin_noise::generatePerlinNoise(vec2 pos, int octaves, float persistence, float amplitude) {
	// Create perlin noise by combining noise octaves of doubling frequency and halving amplitude.
	float overallNoise = 0.0f;
	float height = 1.0f;
	float totalHeight = 0;
	for (int oct = 0; oct < octaves; oct++) {
		float frequency = pow(2, oct);
		overallNoise += generateNoise(pos * frequency) * height;
		height *= persistence;
		totalHeight += height;
	}
	// Normalise then scale by amplitude.
	return overallNoise / totalHeight * amplitude;
}