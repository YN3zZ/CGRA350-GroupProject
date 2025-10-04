#pragma once

// glm
#include <glm/glm.hpp>

// project
#include "opengl.hpp"

class PerlinNoise {
private:
	float generateNoise(glm::vec2 pos);
	float generatePerlinNoise(glm::vec2 pos, const std::vector<glm::vec2> &octaveOffsets);
	glm::vec2 getHeightRange();
	std::vector<cgra::mesh_vertex> vertices;
	cgra::gl_mesh terrain;
	std::vector<GLuint> textures;
	std::vector<GLuint> normalMaps;


public:
	GLuint shader = 0;
	glm::mat4 modelTransform{ 1.0f };

	int noiseSeed = 0; // Used to control what noise is randomly generated for each octave.
	float noisePersistence = 0.3f; // Height loss between octaves.
	float noiseLacunarity = 2.0f; // Frequency increase between octaves.
	float noiseScale = 0.5f; // Spread of noise.
	int noiseOctaves = 4; // Higher octaves add finer details.
	float meshHeight = 2.5f; // Overall height.
	float meshScale = 5.0f; // Overall size of mesh.
	int meshResolution = 25; // Square this to get total vertices.
	float textureScale = 1.0f; // Size of texture.

	// Lighting params.
	glm::vec3 lightDirection{ 0.2f, -1.0f, -1.0f }; // The light points down and the user controls the x angle.
	glm::vec3 lightColor{ 1.0f };
	float roughness = 0.4f;
	float metallic = 0.05f; // 0 = normal, 1 = metal.
	bool useOrenNayar = false;

	// Constructor and public methods.
	PerlinNoise();
	void draw(const glm::mat4& view, const glm::mat4& proj);
	void generate();
	void setShaderParams();
	void createMesh();
	glm::vec3 sampleVertex(glm::vec2 position);
};