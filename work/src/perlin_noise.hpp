#pragma once

// glm
#include <glm/glm.hpp>

// project
#include "opengl.hpp"

class PerlinNoise {
private:

public:
	GLuint shader = 0;
	glm::vec3 color{ 0.7f };
	glm::mat4 modelTransform{ 1.0f };
	float noisePersistence = 0.8f;
	float noiseAmplitude = 2.0f;
	int noiseOctaves = 1;
	float meshSize = 1.0f;
	int meshResolution = 100;

	PerlinNoise() {}
	void draw(const glm::mat4& view, const glm::mat4& proj);
	float generateNoise(glm::vec2 pos);
	float generatePerlinNoise(glm::vec2 pos, int octaves, float persistence, float amplitude);
	cgra::gl_mesh createMesh();
};