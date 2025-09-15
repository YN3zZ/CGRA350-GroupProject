#pragma once

// glm
#include <glm/glm.hpp>

// project
#include "opengl.hpp"

class perlin_noise {
private:

public:
	GLuint shader = 0;
	glm::vec3 color{ 0.7f };
	glm::mat4 modelTransform{ 1.0f };

	perlin_noise() {}
	void draw(const glm::mat4& view, const glm::mat4& proj);
	float generateNoise(glm::vec2 pos, int octave);
	float generatePerlinNoise(glm::vec2 pos);
};