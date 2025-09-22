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
	glm::vec2 getHeightRange();

public:
	GLuint shader = 0;
	glm::vec3 color{ 1.0f };
	glm::mat4 modelTransform{ 1.0f };

	int noiseSeed = 0; // Used to control what noise is randomly generated for each octave.
	float noisePersistence = 0.3f; // Height loss between octaves.
	float noiseLacunarity = 2.0f; // Frequency increase between octaves.
	float noiseScale = 0.5f; // Spread of noise.
	int noiseOctaves = 4; // Higher octaves add finer details.
	float meshHeight = 2.5f; // Overall height.
	float meshSize = 5.0f; // Overall size.
	int meshResolution = 100; // Square this to get total vertices.
	float textureSize = 1.0f;
	cgra::gl_mesh terrain;

	// Vertices vector exposed for other objects such as trees to generate on them.
	std::vector<cgra::mesh_vertex> vertices; // May make this private and expose it through getters or as a parameter.

	// Lighting params.
	glm::vec3 lightDirection{ 0.276f, -0.276f, -0.921f };
	glm::vec3 lightColor{ 1.0f };
	float roughness = 0.6f;
	float metallic = 0.05f; // 0 = normal, 1 = metal.
	bool useOrenNayar = false;
	GLuint texture;

	// Constructor and public methods.
	PerlinNoise(GLuint shader, glm::vec3 color);
	PerlinNoise() {}
	void draw(const glm::mat4& view, const glm::mat4& proj);
	void generate();
};