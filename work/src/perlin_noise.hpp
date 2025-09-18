#pragma once

// glm
#include <glm/glm.hpp>

// project
#include "opengl.hpp"

class PerlinNoise {
private:
	float generateNoise(glm::vec2 pos);
	float generatePerlinNoise(glm::vec2 pos, const std::vector<glm::vec2> &octaveOffsets);
	cgra::gl_mesh createMesh();

public:
	GLuint shader = 0;
	glm::vec3 color{ 0.7f };
	glm::mat4 modelTransform{ 1.0f };
	int noiseSeed = 0; // Used to control what noise is randomly generated for each octave.
	float noisePersistence = 0.3f; // Height loss between octaves.
	float noiseLacunarity = 2.0f; // Frequency increase between octaves.
	float noiseScale = 0.5f; // Spread of noise.
	int noiseOctaves = 4; // Higher octaves add finer details.
	float meshHeight = 2.5f; // Overall height.
	float meshSize = 5.0f; // Overall size.
	int meshResolution = 100; // Square this to get total vertices.
	cgra::gl_mesh terrain;

	// Initially generate the mesh then regenerate it when UI parameters changed.
	PerlinNoise() { generate(); }
	void draw(const glm::mat4& view, const glm::mat4& proj);
	void generate();
};