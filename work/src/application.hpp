
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
	float cameraSpeed = 0.2f;
	bool firstPersonCamera = true;
	
	// skybox
	GLuint skyboxShader = 0;
	GLuint skyboxTexture = 0;
	cgra::gl_mesh skyboxMesh;

	// sun
	GLuint m_sunShader = 0;
	float m_sunIntensity = 1.5f;
	float m_sunAzimuth = 0.0f;
	float m_sunElevation = 50.0f;
	float m_sunDistance = 500.0f;
	glm::vec2 m_sun_screen_pos = glm::vec2(0.5f, 0.5f);  // Sun position in screen space [0-1]

	// Shadow mapping
	GLuint m_shadow_map_fbo = 0;
	GLuint m_shadow_map_texture = 0;
	GLuint m_shadow_depth_shader = 0;
	int m_shadow_map_size = 2048;
	bool m_enable_shadows = true;
	bool m_use_pcf = true;

	// Fog
	bool useFog = true;
	int fogType = 0;
	float fogDensity = 0.01f;

	// Water reflection/refraction
	GLuint m_reflection_fbo = 0;
	GLuint m_reflection_texture = 0;
	GLuint m_reflection_depth_buffer = 0;
	GLuint m_refraction_fbo = 0;
	GLuint m_refraction_texture = 0;
	GLuint m_refraction_depth_buffer = 0;
	int m_water_fbo_width = 1920;
	int m_water_fbo_height = 1080;
	bool m_enable_water_reflections = true;
	float m_water_wave_strength = 0.03f;
	float m_water_reflection_blend = 0.7f;
	float m_cached_water_height = -0.4f;

	// Lens flare post-processing
	GLuint m_scene_fbo = 0;
	GLuint m_scene_texture = 0;
	GLuint m_scene_depth_buffer = 0;
	GLuint m_lens_flare_fbo = 0;
	GLuint m_lens_flare_texture = 0;
	GLuint m_bright_parts_fbo = 0;
	GLuint m_bright_parts_texture = 0;
	GLuint m_pingpong_fbo[2] = {0, 0};
	GLuint m_pingpong_texture[2] = {0, 0};
	GLuint m_bright_parts_shader = 0;
	GLuint m_gaussian_blur_shader = 0;
	GLuint m_lens_flare_ghost_shader = 0;
	GLuint m_lens_flare_composite_shader = 0;
	GLuint m_lens_color_texture = 0;
	GLuint m_lens_texture = 0;
	GLuint m_lens_dirt_texture = 0;
	GLuint m_lens_starburst_texture = 0;
	GLuint m_screen_quad_vao = 0;
	GLuint m_screen_quad_vbo = 0;

	// Lens flare parameters
	bool m_enable_lens_flare = true;
	float m_bright_threshold = 1.0f;
	bool m_bright_smooth_gradient = true;
	int m_lens_flare_type = 2;  // 0=Ghost, 1=Halo, 2=Both
	bool m_lens_use_texture = true;
	int m_ghost_count = 5;
	float m_ghost_dispersal = 0.7f;
	float m_ghost_threshold = 20.0f;
	float m_ghost_distortion = 7.5f;
	float m_halo_radius = 0.4f;
	float m_halo_threshold = 20.0f;
	bool m_lens_use_dirt = false;
	float m_lens_global_brightness = 0.0015f;
	int m_blur_iterations = 20;
	float m_blur_intensity = 0.5f;
	int m_lens_flare_fbo_width = 0;
	int m_lens_flare_fbo_height = 0;

	// Helper functions
	void updateLightFromSun();
	glm::vec3 getSunColor(float elevation);
	glm::vec3 getSkyColor(float elevation);
	void renderShadowMap();
	glm::mat4 getLightSpaceMatrix() const;
	void renderScene(const glm::mat4& view, const glm::mat4& proj, const glm::mat4& lightSpaceMatrix, bool skipWater = false, const glm::vec4& clipPlane = glm::vec4(0.0f));
	void renderScreenQuad();
	void renderLensFlare();
	void compositeLensFlare(GLuint sceneTexture);
	void recreateLensFlareFBOs(int width, int height);

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
