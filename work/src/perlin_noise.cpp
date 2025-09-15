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
	glUniformMatrix4fv(glGetUniformLocation(shader, "uModelViewMatrix"), 1, false, value_ptr(view));
	drawSphere();
}

inline vec2 smoothstep(vec2 t) {
	return t * t * (vec2(3.0f) - 2.0f * t);
}

inline float random(vec2 seed) {
	// Random numbers from 0 to 1. Make deterministic with seed/coordinate based pseduorandom. I opted for seed.
	default_random_engine generator;
	uniform_real_distribution<float> distribution(0.0f, 1.0f);
	generator.seed(cos(seed.x * 1236718) * sin(seed.y * 2168429) * 139.3716f);
	return distribution(generator);
}

// Inspiration from: https://gist.github.com/patriciogonzalezvivo/670c22f3966e662d2f83
float perlin_noise::generateNoise(vec2 pos) {
	// Use fractional component of position to make it smoother closer to vertices.
	vec2 gridPos = floor(pos);
	vec2 posFrac = pos - gridPos;
	vec2 smooth = smoothstep(posFrac);

	// The four corners relative to this position.
	float bl = random(gridPos);
	float br = random(gridPos + vec2(1, 0));
	float tl = random(gridPos + vec2(0, 1));
	float tr = random(gridPos + vec2(1, 1));
	
	return lerp(lerp(bl, br, smooth.x), lerp(tl, tr, smooth.x), smooth.y);
}

float perlin_noise::generatePerlinNoise(vec2 pos) {

}