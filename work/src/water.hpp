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
	GLuint dudvMap;
	float startTime;

public:
	GLuint shader = 0;
	glm::mat4 modelTransform{ 1.0f };
	float waterHeight = -0.4f; // Height of water level.
	float textureScale = 0.80f; // Size of texture.
	float meshScale = 10.0f; // Overall size of mesh. Matches size of terrain.
	int meshResolution = 100; // Square this to get total vertices.
	float waterAlpha = 0.9f;
	float waterSpeed = 0.6f;
	float waterAmplitude = 0.005f;

	float roughness = 0.02f;
	float metallic = 0.25f; // 0 = normal, 1 = metal.
	bool useOrenNayar = true;

	// Constructor and public methods.
	Water();
	void draw(const glm::mat4& view, const glm::mat4& proj,
			  const glm::vec3& lightDirection, const glm::vec3& lightColor,
			  const glm::mat4& lightSpaceMatrix = glm::mat4(1.0f),
			  GLuint shadowMapTexture = 0,
			  bool enableShadows = false,
			  bool usePCF = true,
			  GLuint reflectionTexture = 0,
			  GLuint refractionTexture = 0,
			  bool enableReflections = false,
			  float waveStrength = 0.03f,
			  float reflectionBlend = 0.7f,
			  bool enableLensFlare = false);
	void setShaderParams();
	void createMesh();
};