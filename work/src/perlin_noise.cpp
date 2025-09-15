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
			float x = i / noiseSpread;
			float z = j / noiseSpread;
			// Transform height of sphere by noise at position.
			vec2 pos(x, z);
			float height = generatePerlinNoise(pos, noiseOctaves, noisePersistence, noiseAmplitude);
			mat4 transform = translate(mat4(1.0f), vec3(x, height, z)) *
							scale(mat4(1.0f), vec3(0.08f));
			glUniformMatrix4fv(glGetUniformLocation(shader, "uModelViewMatrix"), 1, false, value_ptr(view * transform));
			drawSphere();
		}
	}
}

inline vec2 smoothstep(vec2 t) {
	return t * t * (3.0f - 2.0f * t);
}

inline float random(vec2 v) {
	// Create a hash then bitshift it with XOR to further randomise. Finally 
	int n = int(v.x) + int(v.y) * 57;
	n = (n << 13) ^ n;
	// 2147483647 is max int, so bitwise AND is used with it to force a positive integer by dropping the (positive/negative) sign bit.
	return float((n * (n * n * 255179 + 98712751) + 1576546427) & 2147483647) / 2147483647.0f; // Divided by max int to give [0, 1] range.
}

inline vec2 randGradient(vec2 v) {
	float angle = random(v) * glm::two_pi<float>();
	return vec2(cos(angle), sin(angle));
}

// Inspiration from: https://gist.github.com/patriciogonzalezvivo/670c22f3966e662d2f83
float perlin_noise::generateNoise(vec2 pos) {
	// Use fractional component of position to make it smoother closer to vertices.
	vec2 gridPos = floor(pos);
	vec2 posFrac = pos - gridPos + 1e-3f;
	vec2 smooth = smoothstep(posFrac);

	// Gradients of the four grid corners relative to this position.
	float bl = dot(randGradient(gridPos), posFrac);
	float br = dot(randGradient(gridPos + vec2(1, 0)), posFrac - vec2(1, 0));
	float tl = dot(randGradient(gridPos + vec2(0, 1)), posFrac - vec2(0, 1));
	float tr = dot(randGradient(gridPos + vec2(1, 1)), posFrac - vec2(1, 1));

	// Bilinear interpolation using smooth/fade.
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
		totalHeight += height;
		height *= persistence;
	}
	// Normalise then scale by amplitude.
	float normalizedHeight = overallNoise / totalHeight;
	float finalHeight = (normalizedHeight + 1.0f) * 0.5f * amplitude; // Map back to positive range.
	return finalHeight;
}