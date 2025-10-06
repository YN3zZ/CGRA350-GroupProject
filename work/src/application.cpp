
// std
#include <iostream>
#include <string>
#include <chrono>

// glm
#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_transform.hpp>

// project
#include "application.hpp"
#include "cgra/cgra_geometry.hpp"
#include "cgra/cgra_gui.hpp"
#include "cgra/cgra_image.hpp"
#include "cgra/cgra_shader.hpp"
#include "cgra/cgra_wavefront.hpp"
#include "perlin_noise.hpp"


using namespace std;
using namespace cgra;
using namespace glm;


void basic_model::draw(const glm::mat4 &view, const glm::mat4 proj) {
	mat4 modelview = view * modelTransform;
	
	glUseProgram(shader); // load shader and variables
	glUniformMatrix4fv(glGetUniformLocation(shader, "uProjectionMatrix"), 1, false, value_ptr(proj));
	glUniformMatrix4fv(glGetUniformLocation(shader, "uModelViewMatrix"), 1, false, value_ptr(modelview));
	glUniform3fv(glGetUniformLocation(shader, "uColor"), 1, value_ptr(color));

	mesh.draw(); // draw
}


Application::Application(GLFWwindow *window) : m_window(window) {
    // Build terrain shader.
    shader_builder sb;
    sb.set_shader(GL_VERTEX_SHADER, CGRA_SRCDIR + std::string("//res//shaders//terrain_vert.glsl"));
	sb.set_shader(GL_FRAGMENT_SHADER, CGRA_SRCDIR + std::string("//res//shaders//terrain_frag.glsl"));
	GLuint terrainShader = sb.build();
  
    // Build bark shader for trees with textures
    shader_builder sb_bark;
    sb_bark.set_shader(GL_VERTEX_SHADER, CGRA_SRCDIR + std::string("//res//shaders//bark_vert_instanced.glsl"));
    sb_bark.set_shader(GL_FRAGMENT_SHADER, CGRA_SRCDIR + std::string("//res//shaders//bark_frag_instanced.glsl"));
    GLuint bark_shader = sb_bark.build();

    // Build water shader.
    shader_builder sb_water;
    sb_water.set_shader(GL_VERTEX_SHADER, CGRA_SRCDIR + std::string("//res//shaders//water_vert.glsl"));
    sb_water.set_shader(GL_FRAGMENT_SHADER, CGRA_SRCDIR + std::string("//res//shaders//water_frag.glsl"));
    GLuint waterShader = sb_water.build();

    // Initialise terrain model using perlin noise.
	m_terrain = PerlinNoise();
    m_terrain.shader = terrainShader;
    m_terrain.createMesh();

    // Create water model
    m_water = Water();
    m_water.shader = waterShader;
    // Make the terrain and water have the same resolution
    m_water.meshResolution = m_terrain.meshResolution; 
    m_water.createMesh();
   
    // Initialize trees with bark shader
    m_trees.shader = bark_shader;
    m_trees.loadTextures();
    m_trees.setTreeType(3);
    m_trees.generateTreesOnTerrain(&m_terrain);

    // Set terrain and water texture params after trees are generated to prevent visual bugs.
    m_terrain.setShaderParams();
    m_water.setShaderParams();

    // Change UI Style
    ImGuiStyle &style = ImGui::GetStyle();
    // Red button, white text, red background
    style.Colors[ImGuiCol_Button] = ImVec4(0.9f, 0.1f, 0.37f, 1.0f);
    style.Colors[ImGuiCol_FrameBg] = ImVec4(0.65f, 0.25f, 0.37f, 1.0f);
    style.Colors[ImGuiCol_WindowBg] = ImVec4(0.3f, 0.2f, 0.2f, 1.0f);
    // Window title bar
    style.Colors[ImGuiCol_TitleBg] = ImVec4(0.4f, 0.4f, 0.4f, 1.0f);
    style.Colors[ImGuiCol_TitleBgActive] = ImVec4(0.6f, 0.2f, 0.2f, 1.0f);
    style.Colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.6f, 0.2f, 0.2f, 1.0f);
}


void Application::render() {
	
	// retrieve the window hieght
	int width, height;
	glfwGetFramebufferSize(m_window, &width, &height); 

	m_windowsize = vec2(width, height); // update window size
	glViewport(0, 0, width, height); // set the viewport to draw to the entire window

	// clear the back-buffer
	glClearColor(0.3f, 0.3f, 0.4f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT); 

	// enable flags for normal/forward rendering
	glEnable(GL_DEPTH_TEST); 
	glDepthFunc(GL_LESS);

	// projection matrix
	mat4 proj = perspective(1.f, float(width) / height, 0.1f, 5000.f);

	// view matrix
    mat4 view;
    if (firstPersonCamera) {
        view = rotate(mat4(1), m_pitch, vec3(1, 0, 0))
            * rotate(mat4(1), m_yaw, vec3(0, 1, 0))
            * translate(mat4(1), -cameraPosition);

        float angle = -m_yaw;
        vec3 forward = vec3(-sin(angle), 0.0f, -cos(angle));
        vec3 up(0.0f, 1.0f, 0.0f);
        vec3 right = vec3(cos(angle), 0.0f, -sin(angle));

        // Add the key movement together to allow diagonal movement.
        vec3 cameraMove(0.0f);
        if (wPressed) cameraMove += forward;
        if (sPressed) cameraMove -= forward;
        if (dPressed) cameraMove += right;
        if (aPressed) cameraMove -= right;
        if (spacePressed) cameraMove += up;
        if (shiftPressed) cameraMove -= up;

        // Opposing directions cancel out and normalise makes diagonal movement the same speed as straight.
        if (length(cameraMove) > 0) {
            cameraPosition += normalize(cameraMove) * cameraSpeed;
        }
    }
    else { // Old camera focused on a single point.
        view = translate(mat4(1), vec3(0, 0, -m_distance))
            * rotate(mat4(1), m_pitch, vec3(1, 0, 0))
            * rotate(mat4(1), m_yaw, vec3(0, 1, 0));
    }

	// helpful draw options
	if (m_show_grid) drawGrid(view, proj);
	if (m_show_axis) drawAxis(view, proj);
	glPolygonMode(GL_FRONT_AND_BACK, (m_showWireframe) ? GL_LINE : GL_FILL);

	// draw the model
	m_terrain.draw(view, proj);

	// Draw trees with lighting from terrain.
	m_trees.draw(view, proj, m_terrain.lightDirection, m_terrain.lightColor);
    
    // Draw water with lighting from terrain.
    m_water.draw(view, proj, m_terrain.lightDirection, m_terrain.lightColor);
}


void Application::renderGUI() {
    // setup window
    ImGui::SetNextWindowPos(ImVec2(5, 5), ImGuiSetCond_Once);
    ImGui::SetNextWindowSize(ImVec2(350, 800), ImGuiSetCond_Once); // (width, height)
    ImGui::Begin("Options", 0);

    // display current camera parameters
    ImGui::Text("Application %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
    ImGui::SliderFloat("Pitch", &m_pitch, -pi<float>() / 2, pi<float>() / 2, "%.2f");
    ImGui::SliderFloat("Yaw", &m_yaw, -pi<float>(), pi<float>(), "%.2f");
    ImGui::SliderFloat("Distance", &m_distance, 0, 2000, "%.2f", 2.0f);

    // helpful drawing options
    ImGui::Checkbox("Show axis", &m_show_axis);
    ImGui::SameLine();
    ImGui::Checkbox("Show grid", &m_show_grid);
    ImGui::Checkbox("Wireframe", &m_showWireframe);
    ImGui::SameLine();
    if (ImGui::Button("Screenshot")) rgba_image::screenshot(true);

    ImGui::Separator();
    ImGui::Text("Terrain Generation");
    
	// Temporary UI control of noise to be replaced with the node-based UI. Regenerates model when parameters changed.
	ImGui::SliderInt("Seed", &m_terrain.noiseSeed, 0, 100, "%.0f");
	ImGui::SliderFloat("Persistence", &m_terrain.noisePersistence, 0.01f, 0.8f, "%.2f", 0.5f);
	ImGui::SliderFloat("Lacunarity", &m_terrain.noiseLacunarity, 1.0f, 4.0f, "%.2f", 2.0f);
	ImGui::SliderFloat("Noise Scale", &m_terrain.noiseScale, 0.01f, 2.0f, "%.2f", 3.0f);
	ImGui::SliderInt("Octaves", &m_terrain.noiseOctaves, 1, 10, "%.0f");
	ImGui::Separator();
	ImGui::SliderFloat("Mesh Height", &m_terrain.meshHeight, 0.1f, 100.0f, "%.1f", 3.0f);
    // Water is the same size and resolution as the terrain.
    if (ImGui::SliderFloat("Mesh Size", &m_terrain.meshScale, 2.0f, 500.0f, "%.1f", 4.0f)) {
        m_water.meshScale = m_terrain.meshScale; // Water is the same size as the terrain.
    }
    if (ImGui::SliderInt("Mesh Resolution", &m_terrain.meshResolution, 10, 500, "%.0f")) {
        m_water.meshResolution = m_terrain.meshResolution;
    }
	ImGui::SliderFloat("Texture Size", &m_terrain.textureScale, 0.1f, 5.0f, "%.1f");
	ImGui::Separator();
	ImGui::SliderFloat3("Light Color", value_ptr(m_terrain.lightColor), 0.0f, 1.0f);
	ImGui::SliderFloat("Light Angle", &m_terrain.lightDirection.x, -1.0f, 1.0f);
    // L-System parameters that affect mesh generation
    bool meshNeedsUpdate = false;

    ImGui::Text("Water Parameters");
    ImGui::SliderFloat("Water Height", &m_water.waterHeight, -5.0f, 2.0f);
    ImGui::SliderFloat("Water Opacity", &m_water.waterAlpha, 0.0f, 1.0f);
    ImGui::SliderFloat("Water Speed", &m_water.waterSpeed, 0.0f, 2.0f);
    
    // Generates the mesh and shaders for terrain and water.
    if (ImGui::Button("Generate")) {
        meshNeedsUpdate = true;
        m_terrain.createMesh();
        m_terrain.setShaderParams();
        m_water.createMesh();
        m_water.setShaderParams();
    }

    ImGui::Separator();
    ImGui::Text("L-System Parameters");
    
    // Tree placement parameters
    if (ImGui::SliderInt("Tree Count", &m_trees.treeCount, 0, 200)) {
        m_trees.regenerateOnTerrain(&m_terrain);
    }
    
    if (ImGui::SliderFloat("Branch Angle", &m_trees.lSystem.angle, 10.0f, 45.0f, "%.1f")) {
        meshNeedsUpdate = true;
    }
    if (ImGui::SliderInt("Iterations", &m_trees.lSystem.iterations, 1, 5)) {
        meshNeedsUpdate = true;
    }
    if (ImGui::SliderFloat("Step Length", &m_trees.lSystem.stepLength, 0.1f, 2.0f, "%.2f")) {
        meshNeedsUpdate = true;
    }
    
    // Tree type selection
    const char* treeTypes[] = {"Simple", "Bushy", "Willow", "3D Tree"};
    if (ImGui::Combo("Tree Type", &m_treeType, treeTypes, 4)) {
        m_trees.setTreeType(m_treeType);
        meshNeedsUpdate = true;
    }

    if (ImGui::SliderFloat("Branch Taper", &m_trees.branchTaper, 0.5f, 1.0f, "%.2f")) {
        meshNeedsUpdate = true;
    }
    if (ImGui::SliderInt("Cylinder Sides", &m_trees.cylinderSides, 4, 12)) {
        meshNeedsUpdate = true;
    }
    
    // Placement parameters that don't affect mesh
    bool placementNeedsUpdate = false;
    
    if (ImGui::SliderFloat("Min Scale", &m_trees.minTreeScale, 0.2f, 1.0f, "%.2f")) {
        placementNeedsUpdate = true;
    }
    if (ImGui::SliderFloat("Max Scale", &m_trees.maxTreeScale, 1.0f, 6.0f, "%.2f")) {
        placementNeedsUpdate = true;
    }
    if (ImGui::Checkbox("Random Rotation", &m_trees.randomRotation)) {
        placementNeedsUpdate = true;
    }
    
    // Apply updates
    if (meshNeedsUpdate) {
        m_trees.markMeshDirty();
        m_trees.regenerateOnTerrain(&m_terrain);
    } else if (placementNeedsUpdate) {
        m_trees.regenerateOnTerrain(&m_terrain);
    }

	// finish creating window
	ImGui::End();
}


void Application::cursorPosCallback(double xpos, double ypos) {
	if (m_leftMouseDown) {
		vec2 whsize = m_windowsize / 2.0f;

		// clamp the pitch to [-pi/2, pi/2]
		m_pitch += float(acos(glm::clamp((m_mousePosition.y - whsize.y) / whsize.y, -1.0f, 1.0f))
			- acos(glm::clamp((float(ypos) - whsize.y) / whsize.y, -1.0f, 1.0f)));
		m_pitch = float(glm::clamp(m_pitch, -pi<float>() / 2, pi<float>() / 2));

		// wrap the yaw to [-pi, pi]
		m_yaw += float(acos(glm::clamp((m_mousePosition.x - whsize.x) / whsize.x, -1.0f, 1.0f))
			- acos(glm::clamp((float(xpos) - whsize.x) / whsize.x, -1.0f, 1.0f)));
		if (m_yaw > pi<float>()) m_yaw -= float(2 * pi<float>());
		else if (m_yaw < -pi<float>()) m_yaw += float(2 * pi<float>());
	}

	// updated mouse position
	m_mousePosition = vec2(xpos, ypos);
}


void Application::mouseButtonCallback(int button, int action, int mods) {
	(void)mods; // currently un-used

	// capture is left-mouse down
	if (button == GLFW_MOUSE_BUTTON_LEFT)
		m_leftMouseDown = (action == GLFW_PRESS); // only other option is GLFW_RELEASE
}


void Application::scrollCallback(double xoffset, double yoffset) {
	(void)xoffset; // currently un-used
	m_distance *= pow(1.1f, -yoffset);
}


void Application::keyCallback(int key, int scancode, int action, int mods) {
    // Pressed is 1, released is 0. Cast to bool for moving each frame.
    if (key == GLFW_KEY_W) {
        wPressed = (bool)action; 
    }
    else if (key == GLFW_KEY_A) {
        aPressed = (bool)action;
    }
    else if (key == GLFW_KEY_S) {
        sPressed = (bool)action;
    }
    else if (key == GLFW_KEY_D) {
        dPressed = (bool)action;
    }
    else if (key == GLFW_KEY_LEFT_SHIFT) {
        shiftPressed = (bool)action;
    }
    else if (key == GLFW_KEY_SPACE) {
        spacePressed = (bool)action;
    }
}


void Application::charCallback(unsigned int c) {
	(void)c; // currently un-used
}
