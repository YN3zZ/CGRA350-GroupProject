
#pragma once

// glm
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

// project
#include "opengl.hpp"
#include "cgra/cgra_mesh.hpp"
#include "perlin_noise.hpp"
#include "tree_generator.hpp"
#include "water.hpp"


// Basic model that holds the shader, mesh and transform for drawing.
// Can be copied and modified for adding in extra information for drawing
// including textures for texture mapping etc.
struct basic_model {
	GLuint shader = 0;
	cgra::gl_mesh mesh;
	glm::vec3 color{ 0.7f };
	glm::mat4 modelTransform{ 1.0f };
	GLuint texture;

	void draw(const glm::mat4 &view, const glm::mat4 proj);
};


// Main application class
//
class Application {
private:
	// window
	glm::vec2 m_windowsize;
	GLFWwindow *m_window;

	// oribital camera
	float m_pitch = .86;
	float m_yaw = -.86;
	float m_distance = 20;

	// last input
	bool m_leftMouseDown = false;
	glm::vec2 m_mousePosition;

	// drawing flags
	bool m_show_axis = false;
	bool m_show_grid = false;
	bool m_showWireframe = false;

	// geometry
	PerlinNoise m_terrain;
	TreeGenerator m_trees;
	Water m_water;
	int m_treeType = 3;

	// First person camera movement.
	glm::vec3 cameraPosition{ 0.0f, 20.0f, 0.0f };
	float cameraSpeed = 0.05f;
	bool firstPersonCamera = true;
	
	// skybox
	GLuint skyboxShader = 0;
	GLuint skyboxTexture = 0;
	cgra::gl_mesh skyboxMesh;

	// sun
	GLuint m_sunShader = 0;
	float m_sunIntensity = 1.5f;
	float m_sunAzimuth = 0.0f;
	float m_sunElevation = 30.0f;
	float m_sunDistance = 500.0f;

	// Shadow mapping
	GLuint m_shadow_map_fbo = 0;
	GLuint m_shadow_map_texture = 0;
	GLuint m_shadow_depth_shader = 0;
	int m_shadow_map_size = 2048;
	bool m_enable_shadows = true;
	bool m_use_pcf = true;

	// Helper functions
	void updateLightFromSun();
	glm::vec3 getSunColor(float elevation);
	glm::vec3 getSkyColor(float elevation);
	void renderShadowMap();
	glm::mat4 getLightSpaceMatrix() const;

public:
	// setup
	Application(GLFWwindow *);

	// disable copy constructors (for safety)
	Application(const Application&) = delete;
	Application& operator=(const Application&) = delete;

	// rendering callbacks (every frame)
	void render();
	void renderGUI();

	// input callbacks
	void cursorPosCallback(double xpos, double ypos);
	void mouseButtonCallback(int button, int action, int mods);
	void scrollCallback(double xoffset, double yoffset);
	void keyCallback(int key, int scancode, int action, int mods);
	void charCallback(unsigned int c);
};
