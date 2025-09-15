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

void perlin_noise::generateNoise() {

}

void perlin_noise::generatePerlinNoise() {

}