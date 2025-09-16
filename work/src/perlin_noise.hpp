#pragma once

// glm
#include <glm/glm.hpp>

// project
#include "opengl.hpp"

class PerlinNoise {
private:
	float generateNoise(glm::vec2 pos);
	float generatePerlinNoise(glm::vec2 pos);
	cgra::gl_mesh createMesh();

public:
	GLuint shader = 0;
	glm::vec3 color{ 0.7f };
	glm::mat4 modelTransform{ 1.0f };
	float noisePersistence = 0.3f; // Height loss between octaves.
	float noiseLacunarity = 0.7f; // Frequency loss between octaves.
	float noiseScale = 0.5f; // Spread of noise.
	int noiseOctaves = 4; // Higher octaves add finer details.
	float meshHeight = 2.5f; // Overall height.
	float meshSize = 5.0f; // Overall size.
	int meshResolution = 100; // Square this to get total vertices.
	cgra::gl_mesh terrain;

	PerlinNoise() {}
	void draw(const glm::mat4& view, const glm::mat4& proj);
};