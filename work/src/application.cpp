
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


// Helper function to load cubemap textures
GLuint loadCubemap(const vector<string>& faces) {
    GLuint textureID;
    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_CUBE_MAP, textureID);

    // Dont flip cubemaps vertically
    stbi_set_flip_vertically_on_load(false);

    for (unsigned int i = 0; i < faces.size(); i++) {
        int width, height, nrChannels;
        unsigned char *data = stbi_load(faces[i].c_str(), &width, &height, &nrChannels, 0);
        if (data) {
            GLenum format = (nrChannels == 4) ? GL_RGBA : GL_RGB;
            glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i,
                         0, GL_RGB, width, height, 0, format, GL_UNSIGNED_BYTE, data);
            stbi_image_free(data);
        }
    }

    // Restore flip setting for regular textures
    stbi_set_flip_vertically_on_load(true);

    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

    return textureID;
}


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
    m_water.createMesh();

    // Initialize trees with bark shader
    m_trees.shader = bark_shader;
    m_trees.loadTextures();
    m_trees.setTreeType(3);
    m_trees.generateTreesOnTerrain(&m_terrain);

    // Set terrain and water texture params after trees are generated to prevent visual bugs.
    m_terrain.setShaderParams();
    m_water.setShaderParams();

    // Build skybox shader
    shader_builder sb_skybox;
    sb_skybox.set_shader(GL_VERTEX_SHADER, CGRA_SRCDIR + std::string("//res//shaders//skybox_vert.glsl"));
    sb_skybox.set_shader(GL_FRAGMENT_SHADER, CGRA_SRCDIR + std::string("//res//shaders//skybox_frag.glsl"));
    skyboxShader = sb_skybox.build();

    // Load skybox cubemap textures
    vector<string> skyboxFaces {
        CGRA_SRCDIR + std::string("//res//textures//skybox//Daylight_Box_Right.bmp"),
        CGRA_SRCDIR + std::string("//res//textures//skybox//Daylight_Box_Left.bmp"),
        CGRA_SRCDIR + std::string("//res//textures//skybox//Daylight_Box_Top.bmp"),
        CGRA_SRCDIR + std::string("//res//textures//skybox//Daylight_Box_Bottom.bmp"),
        CGRA_SRCDIR + std::string("//res//textures//skybox//Daylight_Box_Front.bmp"),
        CGRA_SRCDIR + std::string("//res//textures//skybox//Daylight_Box_Back.bmp")
    };
    skyboxTexture = loadCubemap(skyboxFaces);

    // Create skybox cube mesh
    float skyboxVertices[] = {
        // positions
        -1.0f,  1.0f, -1.0f,
        -1.0f, -1.0f, -1.0f,
         1.0f, -1.0f, -1.0f,
         1.0f, -1.0f, -1.0f,
         1.0f,  1.0f, -1.0f,
        -1.0f,  1.0f, -1.0f,

        -1.0f, -1.0f,  1.0f,
        -1.0f, -1.0f, -1.0f,
        -1.0f,  1.0f, -1.0f,
        -1.0f,  1.0f, -1.0f,
        -1.0f,  1.0f,  1.0f,
        -1.0f, -1.0f,  1.0f,

         1.0f, -1.0f, -1.0f,
         1.0f, -1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,
         1.0f,  1.0f, -1.0f,
         1.0f, -1.0f, -1.0f,

        -1.0f, -1.0f,  1.0f,
        -1.0f,  1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,
         1.0f, -1.0f,  1.0f,
        -1.0f, -1.0f,  1.0f,

        -1.0f,  1.0f, -1.0f,
         1.0f,  1.0f, -1.0f,
         1.0f,  1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,
        -1.0f,  1.0f,  1.0f,
        -1.0f,  1.0f, -1.0f,

        -1.0f, -1.0f, -1.0f,
        -1.0f, -1.0f,  1.0f,
         1.0f, -1.0f, -1.0f,
         1.0f, -1.0f, -1.0f,
        -1.0f, -1.0f,  1.0f,
         1.0f, -1.0f,  1.0f
    };

    // Build skybox mesh
    mesh_builder mb_skybox;
    for (int i = 0; i < 36; i++) {
        mesh_vertex v;
        v.pos = vec3(skyboxVertices[i*3], skyboxVertices[i*3+1], skyboxVertices[i*3+2]);
        v.norm = vec3(0.0f);
        v.uv = vec2(0.0f);
        mb_skybox.push_vertex(v);
        mb_skybox.push_index(i);
    }
    skyboxMesh = mb_skybox.build();

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
	mat4 view = translate(mat4(1), vec3(0, 0, -m_distance))
		* rotate(mat4(1), m_pitch, vec3(1, 0, 0))
		* rotate(mat4(1), m_yaw,   vec3(0, 1, 0));


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

	// Draw skybox as last
	glDepthMask(GL_FALSE);
	glDepthFunc(GL_LEQUAL);
	glUseProgram(skyboxShader);
	mat4 skyboxView = mat4(mat3(view));
	glUniformMatrix4fv(glGetUniformLocation(skyboxShader, "view"), 1, false, value_ptr(skyboxView));
	glUniformMatrix4fv(glGetUniformLocation(skyboxShader, "projection"), 1, false, value_ptr(proj));
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_CUBE_MAP, skyboxTexture);
	glUniform1i(glGetUniformLocation(skyboxShader, "skybox"), 0);
	skyboxMesh.draw();
	glDepthMask(GL_TRUE);
	glDepthFunc(GL_LESS);
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
	(void)key, (void)scancode, (void)action, (void)mods; // currently un-used
}


void Application::charCallback(unsigned int c) {
	(void)c; // currently un-used
}
