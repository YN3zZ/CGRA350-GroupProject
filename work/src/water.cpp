// glm
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <random>
#include <chrono>

// project
#include "cgra/cgra_geometry.hpp"
#include "cgra/cgra_mesh.hpp"
#include "water.hpp"
#include "cgra/cgra_image.hpp"

using namespace std;
using namespace glm;
using namespace cgra;

// Initially generate the mesh and load the textures. Initialise shader and color separately.
Water::Water() {
	// Enable tranparency, so the water can use alpha channel.
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	string pathStart = CGRA_SRCDIR + string("/res/textures/") + "water";
	rgba_image textureImage = rgba_image(pathStart + string("_albedo.png"));
	rgba_image normalImage = rgba_image(pathStart + string("_normal.png"));
	texture = textureImage.uploadTexture();
	normalMap = normalImage.uploadTexture();

	// Get starting time for animations to base off.
	startTime = (float)glfwGetTime();
}


// Set textures, normals, scale and height params.
void Water::setShaderParams() {
	// Only update uniforms for texture and textureSize when mesh is updated.
	glUseProgram(shader);
	// Texture
	int i = 22; // Location
	glActiveTexture(GL_TEXTURE0 + i);
	glBindTexture(GL_TEXTURE_2D, texture);
	glUniform1i(glGetUniformLocation(shader, "uTexture"), i++);

	// Normal Map
	glActiveTexture(GL_TEXTURE0 + i);
	glBindTexture(GL_TEXTURE_2D, normalMap);
	glUniform1i(glGetUniformLocation(shader, "uNormalMap"), i);

	glUniform1f(glGetUniformLocation(shader, "textureScale"), sqrt(meshScale) / textureScale);
	glUniform1f(glGetUniformLocation(shader, "meshScale"), meshScale);
}


void Water::draw(const mat4& view, const mat4& proj, const vec3& lightDirection, const vec3& lightColor, 
				const mat4& lightSpaceMatrix, GLuint shadowMapTexture, bool enableShadows, bool usePCF) {
	// set up the shader for every draw call
	glUseProgram(shader);
	// Set model, view and projection matrices.
	glUniformMatrix4fv(glGetUniformLocation(shader, "uProjectionMatrix"), 1, false, value_ptr(proj));
	glUniformMatrix4fv(glGetUniformLocation(shader, "uModelViewMatrix"), 1, false, value_ptr(view * modelTransform));

	// Lighting params
	vec3 lightDirViewSpace = mat3(view) * lightDirection;
	glUniform3fv(glGetUniformLocation(shader, "lightDirection"), 1, value_ptr(lightDirViewSpace));
	glUniform3fv(glGetUniformLocation(shader, "lightColor"), 1, value_ptr(lightColor));
	glUniform1f(glGetUniformLocation(shader, "roughness"), roughness);
	glUniform1f(glGetUniformLocation(shader, "metallic"), metallic);
	glUniform1i(glGetUniformLocation(shader, "useOrenNayar"), useOrenNayar ? 1 : 0);
	glUniform1f(glGetUniformLocation(shader, "alpha"), waterAlpha);

	float currentTime = (float)glfwGetTime() - startTime;
	glUniform1f(glGetUniformLocation(shader, "uTime"), currentTime);

	// Send uniform for water speed and height.
	glUniform1f(glGetUniformLocation(shader, "waterSpeed"), waterSpeed);
	//glUniform1f(glGetUniformLocation(shader, "waterHeight"), waterHeight); // For terrain transitions
	glUniform1f(glGetUniformLocation(shader, "waterAmplitude"), waterAmplitude);

	// Re-bind water textures (fix for water texture disappering)
	glActiveTexture(GL_TEXTURE22);
	glBindTexture(GL_TEXTURE_2D, texture);
	glActiveTexture(GL_TEXTURE23);
	glBindTexture(GL_TEXTURE_2D, normalMap);

	// Shadow params
	glActiveTexture(GL_TEXTURE20);
	glBindTexture(GL_TEXTURE_2D, shadowMapTexture);

	glUniformMatrix4fv(glGetUniformLocation(shader, "uLightSpaceMatrix"), 1, false, value_ptr(lightSpaceMatrix));
	glUniform1i(glGetUniformLocation(shader, "uShadowMap"), 20);
	glUniform1i(glGetUniformLocation(shader, "uEnableShadows"), enableShadows ? 1 : 0);
	glUniform1i(glGetUniformLocation(shader, "uUsePCF"), usePCF ? 1 : 0);

	// Draw the terrain mesh.
	waterMesh.draw();
}


// Create terrain mesh using perlin noise heightmap.
void Water::createMesh() {

	// Create the vertices which have positions, normals and UVs.
	vector<mesh_vertex> vertices(meshResolution * meshResolution);
	for (int i = 0; i < meshResolution; ++i) {
		for (int j = 0; j < meshResolution; ++j) {
			// Get u and v offset on mesh and map to range -noiseSize to noiseSize for x and z.
			float u = i / (meshResolution - 1.0f);
			float v = j / (meshResolution - 1.0f);
			float x = (-1.0f + 2.0f * u) * meshScale;
			float z = (-1.0f + 2.0f * v) * meshScale;

			// Position, normal, and uv. Simple upwards normal because the waves are generated in the shader.
			vec3 pos = vec3(x, waterHeight, z);
			vec2 uv(u, v);
			vec3 norm = vec3(0, 1, 0);

			// Each increase in i is a whole loop of j over meshResolution.
			int vertIndex = i * meshResolution + j;
			vertices[vertIndex] = mesh_vertex{ pos, norm, uv };
		}
	}

	// Create the triangles. Ignore the final vertex (meshResolution - 1) as the quads/triangles are formed up to it.
	mesh_builder mb;
	unsigned int index = 0;
	for (int i = 0; i < meshResolution - 1; ++i) {
		for (int j = 0; j < meshResolution - 1; ++j) {
			// Offset i by the rows of j that have been passed already. This is to index the vector properly.
			int iOffset = i * meshResolution;

			// Get vertices. 2D array alternative: vertices[i][j] to vertices[i+1][j+1].
			mesh_vertex topLeft = vertices[iOffset + j];
			mesh_vertex bottomLeft = vertices[iOffset + (j + 1)];
			mesh_vertex topRight = vertices[(iOffset + meshResolution) + j];
			mesh_vertex bottomRight = vertices[(iOffset + meshResolution) + (j + 1)];
			// Add the two sets of three vertices for the left triangle and right triangle.
			for (mesh_vertex v : { topLeft, topRight, bottomLeft, bottomLeft, topRight, bottomRight }) {
				mb.push_vertex(v);
			}

			// Add all the indices of the quad in a one liner, instead of doing push_index() 6 times.
			mb.push_indices({ index, index + 1, index + 2, index + 3, index + 4, index + 5 });
			index += 6;
		}
	}
	// Build and set gl_mesh.
	waterMesh = mb.build();
}