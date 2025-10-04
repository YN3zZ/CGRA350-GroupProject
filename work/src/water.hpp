#pragma once

// glm
#include <glm/glm.hpp>

// project
#include "opengl.hpp"

class Water {
private:
	cgra::gl_mesh waterMesh;
	GLuint texture;
	GLuint normalMap;

public:
	GLuint shader = 0;
	glm::mat4 modelTransform{ 1.0f };
	float waterHeight = 0.05f; // Height of water level.
	float textureScale = 0.02f; // Size of texture.
	float meshScale = 5.0f; // Overall size of mesh. Matches size of terrain.
	int meshResolution = 25; // Square this to get total vertices.

	float roughness = 0.04f;
	float metallic = 0.05f; // 0 = normal, 1 = metal.
	bool useOrenNayar = false;

	// Constructor and public methods.
	Water();
	void draw(const glm::mat4& view, const glm::mat4& proj, glm::vec3 lightDirection, glm::vec3 lightColor);
	void setShaderParams();
	void createMesh();
};