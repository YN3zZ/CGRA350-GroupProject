
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


// Bias matrix to transform NDC coordinates [-1,1] to texture coordinates [0,1]
const glm::mat4 biasMatrix(
	0.5f, 0.0f, 0.0f, 0.0f,
	0.0f, 0.5f, 0.0f, 0.0f,
	0.0f, 0.0f, 0.5f, 0.0f,
	0.5f, 0.5f, 0.5f, 1.0f
);


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

    // Build sun shader
    shader_builder sb_sun;
    sb_sun.set_shader(GL_VERTEX_SHADER, CGRA_SRCDIR + std::string("//res//shaders//sun_vert.glsl"));
    sb_sun.set_shader(GL_FRAGMENT_SHADER, CGRA_SRCDIR + std::string("//res//shaders//sun_frag.glsl"));
    m_sunShader = sb_sun.build();

    // Initialize light direction from sun
    updateLightFromSun();

    // Build shadow depth shader (must have both vertex and fragment shader for OpenGL 3.3 core)
    shader_builder sb_shadow;
    sb_shadow.set_shader(GL_VERTEX_SHADER, CGRA_SRCDIR + std::string("//res//shaders//shadow_depth_vert.glsl"));
    sb_shadow.set_shader(GL_FRAGMENT_SHADER, CGRA_SRCDIR + std::string("//res//shaders//shadow_depth_frag.glsl"));
    m_shadow_depth_shader = sb_shadow.build();

    // Create shadow map framebuffer
    glGenFramebuffers(1, &m_shadow_map_fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, m_shadow_map_fbo);

    // Create depth texture
    glGenTextures(1, &m_shadow_map_texture);
    glBindTexture(GL_TEXTURE_2D, m_shadow_map_texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24,
                 m_shadow_map_size, m_shadow_map_size,
                 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    float borderColor[] = { 1.0f, 1.0f, 1.0f, 1.0f };
    glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);

    // Enable shadow comparison mode for sampler2DShadow
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);

    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                           GL_TEXTURE_2D, m_shadow_map_texture, 0);

    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // Create reflection framebuffer
    glGenFramebuffers(1, &m_reflection_fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, m_reflection_fbo);

    // Reflection color texture
    glGenTextures(1, &m_reflection_texture);
    glBindTexture(GL_TEXTURE_2D, m_reflection_texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, m_water_fbo_width, m_water_fbo_height, 0, GL_RGB, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_reflection_texture, 0);

    // Reflection depth renderbuffer
    glGenRenderbuffers(1, &m_reflection_depth_buffer);
    glBindRenderbuffer(GL_RENDERBUFFER, m_reflection_depth_buffer);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, m_water_fbo_width, m_water_fbo_height);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, m_reflection_depth_buffer);

    // Create refraction framebuffer
    glGenFramebuffers(1, &m_refraction_fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, m_refraction_fbo);

    // Refraction color texture
    glGenTextures(1, &m_refraction_texture);
    glBindTexture(GL_TEXTURE_2D, m_refraction_texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, m_water_fbo_width, m_water_fbo_height, 0, GL_RGB, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_refraction_texture, 0);

    // Refraction depth renderbuffer
    glGenRenderbuffers(1, &m_refraction_depth_buffer);
    glBindRenderbuffer(GL_RENDERBUFFER, m_refraction_depth_buffer);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, m_water_fbo_width, m_water_fbo_height);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, m_refraction_depth_buffer);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // Build lens flare shaders
    shader_builder sb_bright_parts;
    sb_bright_parts.set_shader(GL_VERTEX_SHADER, CGRA_SRCDIR + std::string("//res//shaders//bright_parts_vert.glsl"));
    sb_bright_parts.set_shader(GL_FRAGMENT_SHADER, CGRA_SRCDIR + std::string("//res//shaders//bright_parts_frag.glsl"));
    m_bright_parts_shader = sb_bright_parts.build();

    shader_builder sb_gaussian_blur;
    sb_gaussian_blur.set_shader(GL_VERTEX_SHADER, CGRA_SRCDIR + std::string("//res//shaders//gaussian_blur_vert.glsl"));
    sb_gaussian_blur.set_shader(GL_FRAGMENT_SHADER, CGRA_SRCDIR + std::string("//res//shaders//gaussian_blur_frag.glsl"));
    m_gaussian_blur_shader = sb_gaussian_blur.build();

    shader_builder sb_lens_ghost;
    sb_lens_ghost.set_shader(GL_VERTEX_SHADER, CGRA_SRCDIR + std::string("//res//shaders//lens_flare_ghost_vert.glsl"));
    sb_lens_ghost.set_shader(GL_FRAGMENT_SHADER, CGRA_SRCDIR + std::string("//res//shaders//lens_flare_ghost_frag.glsl"));
    m_lens_flare_ghost_shader = sb_lens_ghost.build();

    shader_builder sb_lens_composite;
    sb_lens_composite.set_shader(GL_VERTEX_SHADER, CGRA_SRCDIR + std::string("//res//shaders//lens_flare_composite_vert.glsl"));
    sb_lens_composite.set_shader(GL_FRAGMENT_SHADER, CGRA_SRCDIR + std::string("//res//shaders//lens_flare_composite_frag.glsl"));
    m_lens_flare_composite_shader = sb_lens_composite.build();

    // Load lens flare textures
    rgba_image lens_color_img = rgba_image(CGRA_SRCDIR + std::string("//res//textures//ppfx//lensColor.jpg"));
    glGenTextures(1, &m_lens_color_texture);
    glBindTexture(GL_TEXTURE_2D, m_lens_color_texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, lens_color_img.size.x, lens_color_img.size.y, 0, GL_RGBA, GL_UNSIGNED_BYTE, lens_color_img.data.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    rgba_image lens_texture_img = rgba_image(CGRA_SRCDIR + std::string("//res//textures//ppfx//lensTexture.jpg"));
    glGenTextures(1, &m_lens_texture);
    glBindTexture(GL_TEXTURE_2D, m_lens_texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, lens_texture_img.size.x, lens_texture_img.size.y, 0, GL_RGBA, GL_UNSIGNED_BYTE, lens_texture_img.data.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    rgba_image lens_dirt_img = rgba_image(CGRA_SRCDIR + std::string("//res//textures//ppfx//lensDirt.png"));
    glGenTextures(1, &m_lens_dirt_texture);
    glBindTexture(GL_TEXTURE_2D, m_lens_dirt_texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, lens_dirt_img.size.x, lens_dirt_img.size.y, 0, GL_RGBA, GL_UNSIGNED_BYTE, lens_dirt_img.data.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    rgba_image lens_starburst_img = rgba_image(CGRA_SRCDIR + std::string("//res//textures//ppfx//lensStarburst.png"));
    glGenTextures(1, &m_lens_starburst_texture);
    glBindTexture(GL_TEXTURE_2D, m_lens_starburst_texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, lens_starburst_img.size.x, lens_starburst_img.size.y, 0, GL_RGBA, GL_UNSIGNED_BYTE, lens_starburst_img.data.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    // Create lens flare framebuffers
    int width, height;
    glfwGetFramebufferSize(m_window, &width, &height);

    // Initialize FBO tracking variables and create initial FBOs
    m_lens_flare_fbo_width = 0;  // Set to 0 to force initial creation
    m_lens_flare_fbo_height = 0;
    recreateLensFlareFBOs(width, height);

    // Create screen quad for post-processing
    float quadVertices[] = {
        // positions        // texCoords
        -1.0f,  1.0f, 0.0f,  0.0f, 1.0f,
        -1.0f, -1.0f, 0.0f,  0.0f, 0.0f,
         1.0f, -1.0f, 0.0f,  1.0f, 0.0f,
        -1.0f,  1.0f, 0.0f,  0.0f, 1.0f,
         1.0f, -1.0f, 0.0f,  1.0f, 0.0f,
         1.0f,  1.0f, 0.0f,  1.0f, 1.0f
    };

    glGenVertexArrays(1, &m_screen_quad_vao);
    glGenBuffers(1, &m_screen_quad_vbo);
    glBindVertexArray(m_screen_quad_vao);
    glBindBuffer(GL_ARRAY_BUFFER, m_screen_quad_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), quadVertices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
    glBindVertexArray(0);

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


// Helper function to render the scene (terrain, trees, skybox). Also sets fog parameters.
void Application::renderScene(const mat4& view, const mat4& proj, const mat4& lightSpaceMatrix, bool skipWater, const vec4& clipPlane) {
	// Calculate light color based on sun elevation
	float sunVisibility = glm::smoothstep(-10.0f, 0.0f, m_sunElevation);
	vec3 baseLightColor = m_terrain.lightColor;
	vec3 activeLightColor = m_terrain.lightColor * sunVisibility;

	// Re-bind shadow map texture to prevent conflicts from FBO switches
	// to ensures shadows work correctly during reflection/refraction passes
	glActiveTexture(GL_TEXTURE20);
	glBindTexture(GL_TEXTURE_2D, m_shadow_map_texture);

	// Ensure depth mask is enabled for geometry rendering
	// (skybox disables it
	glDepthMask(GL_TRUE);

	// Set clip plane uniform for terrain
	glUseProgram(m_terrain.shader);
	glUniform1i(glGetUniformLocation(m_terrain.shader, "useFog"), useFog);
	glUniform1i(glGetUniformLocation(m_terrain.shader, "linearFog"), fogType == 0);
	glUniform1f(glGetUniformLocation(m_terrain.shader, "fogDensity"), fogDensity);
	glUniform4fv(glGetUniformLocation(m_terrain.shader, "uClipPlane"), 1, value_ptr(clipPlane));

	// Draw terrain
	m_terrain.lightColor = activeLightColor;
	m_terrain.draw(view, proj, lightSpaceMatrix, m_shadow_map_texture, m_enable_shadows, m_use_pcf);

	// Fog for leaves
	glUseProgram(m_trees.leafShader);
	glUniform1i(glGetUniformLocation(m_trees.leafShader, "useFog"), useFog);
	glUniform1i(glGetUniformLocation(m_trees.leafShader, "linearFog"), fogType == 0);
	glUniform1f(glGetUniformLocation(m_trees.leafShader, "fogDensity"), fogDensity);

	// Set clip plane uniform for trees
	glUseProgram(m_trees.shader);
	glUniform1i(glGetUniformLocation(m_trees.shader, "useFog"), useFog);
	glUniform1i(glGetUniformLocation(m_trees.shader, "linearFog"), fogType == 0);
	glUniform1f(glGetUniformLocation(m_trees.shader, "fogDensity"), fogDensity);
	glUniform4fv(glGetUniformLocation(m_trees.shader, "uClipPlane"), 1, value_ptr(clipPlane));

	// Draw trees
	m_trees.draw(view, proj, m_terrain.lightDirection, activeLightColor, lightSpaceMatrix, m_shadow_map_texture, m_enable_shadows, m_use_pcf);

	m_terrain.lightColor = baseLightColor;

	// Draw skybox
	glDepthMask(GL_FALSE);
	glDepthFunc(GL_LEQUAL);
	glDisable(GL_CULL_FACE);
	glUseProgram(skyboxShader);
	mat4 skyboxView = mat4(mat3(view));
	glUniformMatrix4fv(glGetUniformLocation(skyboxShader, "view"), 1, false, value_ptr(skyboxView));
	glUniformMatrix4fv(glGetUniformLocation(skyboxShader, "projection"), 1, false, value_ptr(proj));

	vec3 skyColor = getSkyColor(m_sunElevation);
	float atmosphereBlend;
	if (m_sunElevation > 0.0f) {
		atmosphereBlend = 0.0f;
	} else if (m_sunElevation > -20.0f) {
		atmosphereBlend = smoothstep(0.0f, -20.0f, m_sunElevation);
	} else {
		atmosphereBlend = 1.0f;
	}

	glUniform3fv(glGetUniformLocation(skyboxShader, "atmosphereColor"), 1, value_ptr(skyColor));
	glUniform1f(glGetUniformLocation(skyboxShader, "atmosphereBlend"), atmosphereBlend);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_CUBE_MAP, skyboxTexture);
	glUniform1i(glGetUniformLocation(skyboxShader, "skybox"), 0);
	skyboxMesh.draw();
	glDepthMask(GL_TRUE);
	glDepthFunc(GL_LESS);
	glEnable(GL_CULL_FACE);

	// Draw sun (if not skipping water pass)
	if (!skipWater) {
		glDepthFunc(GL_LEQUAL);
		glUseProgram(m_sunShader);

		// Calculate sun direction (normalized, infinitely far like skybox)
		float azimuthRad = glm::radians(m_sunAzimuth);
		float elevationRad = glm::radians(m_sunElevation);
		vec3 sunDirection;
		sunDirection.x = cos(elevationRad) * cos(azimuthRad);
		sunDirection.y = sin(elevationRad);
		sunDirection.z = cos(elevationRad) * sin(azimuthRad);

		// Remove translation from view matrix (like skybox) to make sun infinitely far
		mat4 sunView = mat4(mat3(view));
		mat4 sunModel = translate(mat4(1), sunDirection * 100.0f) * scale(mat4(1), vec3(2.0f));
		mat4 sunMV = sunView * sunModel;

		glUniformMatrix4fv(glGetUniformLocation(m_sunShader, "uModelViewMatrix"), 1, false, value_ptr(sunMV));
		glUniformMatrix4fv(glGetUniformLocation(m_sunShader, "uProjectionMatrix"), 1, false, value_ptr(proj));
		glUniform3fv(glGetUniformLocation(m_sunShader, "uSunColor"), 1, value_ptr(activeLightColor));
		glUniform1f(glGetUniformLocation(m_sunShader, "uIntensity"), m_sunIntensity);

		cgra::drawSphere();
		glDepthFunc(GL_LESS);
	}
}

void Application::render() {

	// 1st pass: Shadow map (must be first so reflections/refractions can use it)

	// Only render shadows when sun is above horizon
	if (m_enable_shadows && m_sunElevation > -5.0f) {
		renderShadowMap();
	}

	// 2nd pass: Reflection
	if (m_enable_water_reflections) {
		glBindFramebuffer(GL_FRAMEBUFFER, m_reflection_fbo);
		glViewport(0, 0, m_water_fbo_width, m_water_fbo_height);
		glClearColor(0.3f, 0.3f, 0.4f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		// Re-bind shadow texture after FBO switch
		glActiveTexture(GL_TEXTURE20);
		glBindTexture(GL_TEXTURE_2D, m_shadow_map_texture);

		glEnable(GL_DEPTH_TEST);
		glDepthFunc(GL_LESS);
		glEnable(GL_CULL_FACE);
		glCullFace(GL_BACK);
		glFrontFace(GL_CCW);
		glEnable(GL_CLIP_DISTANCE0);

		// Mirror camera across water plane
		float waterHeight = m_cached_water_height;
		float distance = 2.0f * (cameraPosition.y - waterHeight);
		vec3 reflectedCameraPos = cameraPosition;
		reflectedCameraPos.y -= distance;
		float reflectedPitch = -m_pitch;

		mat4 reflectionView;
		if (firstPersonCamera) {
			reflectionView = rotate(mat4(1), reflectedPitch, vec3(1, 0, 0))
				* rotate(mat4(1), m_yaw, vec3(0, 1, 0))
				* translate(mat4(1), -reflectedCameraPos);
		} else {
			reflectionView = translate(mat4(1), vec3(0, 0, -m_distance))
				* rotate(mat4(1), reflectedPitch, vec3(1, 0, 0))
				* rotate(mat4(1), m_yaw, vec3(0, 1, 0));
		}

		int width, height;
		glfwGetFramebufferSize(m_window, &width, &height);
		mat4 proj = perspective(1.f, float(width) / height, 0.1f, 5000.f);

		mat4 lightSpaceMatrix = getLightSpaceMatrix();
		vec4 clipPlane(0.0f, 1.0f, 0.0f, -waterHeight + 0.1f); // Clip below water.

		renderScene(reflectionView, proj, lightSpaceMatrix, true, clipPlane);

		glDisable(GL_CLIP_DISTANCE0);
		glFrontFace(GL_CW);

		// 3rd pass: Refraction

		glBindFramebuffer(GL_FRAMEBUFFER, m_refraction_fbo);
		glViewport(0, 0, m_water_fbo_width, m_water_fbo_height);
		glClearColor(0.3f, 0.3f, 0.4f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		// Re-bind shadow texture after FBO switch
		glActiveTexture(GL_TEXTURE20);
		glBindTexture(GL_TEXTURE_2D, m_shadow_map_texture);

		glEnable(GL_DEPTH_TEST);
		glDepthFunc(GL_LESS);
		glEnable(GL_CULL_FACE);
		glCullFace(GL_BACK);
		glFrontFace(GL_CW);
		glEnable(GL_CLIP_DISTANCE0);

		glfwGetFramebufferSize(m_window, &width, &height);
		proj = perspective(1.f, float(width) / height, 0.1f, 5000.f);

		mat4 view;
		if (firstPersonCamera) {
			view = rotate(mat4(1), m_pitch, vec3(1, 0, 0))
				* rotate(mat4(1), m_yaw, vec3(0, 1, 0))
				* translate(mat4(1), -cameraPosition);
		} else {
			view = translate(mat4(1), vec3(0, 0, -m_distance))
				* rotate(mat4(1), m_pitch, vec3(1, 0, 0))
				* rotate(mat4(1), m_yaw, vec3(0, 1, 0));
		}

		lightSpaceMatrix = getLightSpaceMatrix();
		clipPlane = vec4(0.0f, -1.0f, 0.0f, waterHeight + 0.1f); // Clip above water

		renderScene(view, proj, lightSpaceMatrix, true, clipPlane);

		glDisable(GL_CLIP_DISTANCE0);
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
	}

	// 4th pass: main rendering

	// retrieve the window height
	int width, height;
	glfwGetFramebufferSize(m_window, &width, &height);

	m_windowsize = vec2(width, height); // update window size

	// Check if post-processing FBOs need to be recreated due to window resize
	if ((m_enable_lens_flare || m_enable_bloom) && (width != m_lens_flare_fbo_width || height != m_lens_flare_fbo_height)) {
		recreateLensFlareFBOs(width, height);
	}

	// Render to scene FBO for post-processing (lens flare and/or bloom)
	if (m_enable_lens_flare || m_enable_bloom) {
		glBindFramebuffer(GL_FRAMEBUFFER, m_scene_fbo);
	} else {
		// Ensure default framebuffer is bound when both effects are disabled
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
	}

	glViewport(0, 0, width, height); // set the viewport to draw to the entire window

	// clear the back-buffer
	glClearColor(0.3f, 0.3f, 0.4f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	// Re-bind shadow texture after returning to default framebuffer
	glActiveTexture(GL_TEXTURE20);
	glBindTexture(GL_TEXTURE_2D, m_shadow_map_texture);

	// enable flags for normal/forward rendering
	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LESS);
	glEnable(GL_CULL_FACE);
	glCullFace(GL_BACK);
	glFrontFace(GL_CW);

	// Update camera position for first person mode
    if (firstPersonCamera) {
        float angle = -m_yaw;
        vec3 forward = vec3(-sin(angle), 0.0f, -cos(angle));
        vec3 up(0.0f, 1.0f, 0.0f);
        vec3 right = vec3(cos(angle), 0.0f, -sin(angle));

        // Add the key movement together to allow diagonal movement.
        vec3 horizontalMove(0.0f);
		vec3 verticalMove(0.0f);
        if (glfwGetKey(m_window, GLFW_KEY_W) == GLFW_PRESS) horizontalMove += forward;
        if (glfwGetKey(m_window, GLFW_KEY_S) == GLFW_PRESS) horizontalMove -= forward;
        if (glfwGetKey(m_window, GLFW_KEY_D) == GLFW_PRESS) horizontalMove += right;
        if (glfwGetKey(m_window, GLFW_KEY_A) == GLFW_PRESS) horizontalMove -= right;
        if (glfwGetKey(m_window, GLFW_KEY_SPACE) == GLFW_PRESS) verticalMove += up;
        if (glfwGetKey(m_window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS) verticalMove -= up;

        // Opposing directions cancel out and normalise makes diagonal movement the same speed as straight.
        if (length(horizontalMove) > 0) {
			horizontalMove = normalize(horizontalMove);
		} 
		cameraPosition += (verticalMove + horizontalMove) * cameraSpeed;
    }

	// projection matrix
	mat4 proj = perspective(1.f, float(width) / height, 0.1f, 5000.f);

	// view matrix
    mat4 view;
    if (firstPersonCamera) {
        view = rotate(mat4(1), m_pitch, vec3(1, 0, 0))
            * rotate(mat4(1), m_yaw, vec3(0, 1, 0))
            * translate(mat4(1), -cameraPosition);
    } else {
        view = translate(mat4(1), vec3(0, 0, -m_distance))
            * rotate(mat4(1), m_pitch, vec3(1, 0, 0))
            * rotate(mat4(1), m_yaw, vec3(0, 1, 0));
    }

	// helpful draw options
	if (m_show_grid) drawGrid(view, proj);
	if (m_show_axis) drawAxis(view, proj);
	glPolygonMode(GL_FRONT_AND_BACK, (m_showWireframe) ? GL_LINE : GL_FILL);

	// Calculate light space matrix for shadow mapping
	mat4 lightSpaceMatrix = getLightSpaceMatrix();

	// Set no clipping for main scene
	vec4 noClipPlane(0.0f, 0.0f, 0.0f, 0.0f);
	renderScene(view, proj, lightSpaceMatrix, false, noClipPlane);

	// Sets fog parameters for water.
	glUseProgram(m_water.shader);
	glUniform1i(glGetUniformLocation(m_water.shader, "useFog"), useFog);
	glUniform1i(glGetUniformLocation(m_water.shader, "linearFog"), fogType == 0);
	glUniform1f(glGetUniformLocation(m_water.shader, "fogDensity"), fogDensity);

    // Draw water with reflection/refraction textures
	float sunVisibility = glm::smoothstep(-10.0f, 0.0f, m_sunElevation);
	vec3 activeLightColor = m_terrain.lightColor * sunVisibility;
    m_water.draw(view, proj, m_terrain.lightDirection, activeLightColor,
				 lightSpaceMatrix, m_shadow_map_texture, m_enable_shadows, m_use_pcf,
				 m_reflection_texture, m_refraction_texture,
				 m_enable_water_reflections, m_water_wave_strength, m_water_reflection_blend,
				 m_enable_lens_flare);

	// Draw sun
	glDepthFunc(GL_LEQUAL);
	glUseProgram(m_sunShader);

	// Calculate sun direction (normalized, infinitely far like skybox)
	float azimuthRad = radians(m_sunAzimuth);
	float elevationRad = radians(m_sunElevation);
	vec3 sunDirection;
	sunDirection.x = cos(elevationRad) * cos(azimuthRad);
	sunDirection.y = sin(elevationRad);
	sunDirection.z = cos(elevationRad) * sin(azimuthRad);

	// Remove translation from view matrix (like skybox) to make sun infinitely far
	mat4 sunView = mat4(mat3(view));
	mat4 sunModel = translate(mat4(1), sunDirection * 100.0f) * scale(mat4(1), vec3(1.5f));
	mat4 sunMV = sunView * sunModel;

	vec4 sunClipSpace = proj * view * vec4(sunDirection, 1.0);
	vec3 sunNDC = vec3(sunClipSpace) / sunClipSpace.w;  // Normalize by w
	m_sun_screen_pos = vec2(sunNDC.x * 0.5f + 0.5f, sunNDC.y * 0.5f + 0.5f);  // Convert from [-1,1] to [0,1]

	glUniformMatrix4fv(glGetUniformLocation(m_sunShader, "uModelViewMatrix"), 1, false, value_ptr(sunMV));
	glUniformMatrix4fv(glGetUniformLocation(m_sunShader, "uProjectionMatrix"), 1, false, value_ptr(proj));
	glUniform3fv(glGetUniformLocation(m_sunShader, "uSunColor"), 1, value_ptr(activeLightColor));
	glUniform1f(glGetUniformLocation(m_sunShader, "uIntensity"), m_sunIntensity);

	cgra::drawSphere();
	glDepthFunc(GL_LESS);

	// Apply post-processing effects
	if (m_enable_lens_flare || m_enable_bloom) {
		// Generate lens flare artifacts from the scene
		if (m_enable_lens_flare) {
			renderLensFlare();
		}

		// Generate bloom from bright parts (extracted via MRT)
		if (m_enable_bloom) {
			renderBloom();
		}

		// Composite lens flare and bloom onto the scene and render to default framebuffer
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		glDrawBuffer(GL_BACK);  // Ensure drawing to back buffer of default framebuffer
		glViewport(0, 0, width, height);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		compositeLensFlare(m_scene_texture);

		// Restore terrain and water texture bindings that were overwritten by post-processing
		m_terrain.setShaderParams();
		m_water.setShaderParams();
	} else {
		// Ensure default framebuffer is bound when both effects are disabled
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
	}
}


void Application::renderGUI() {
    // setup window
    ImGui::SetNextWindowPos(ImVec2(5, 5), ImGuiSetCond_Once);
    ImGui::SetNextWindowSize(ImVec2(400, 950), ImGuiSetCond_Once); // (width, height)
    ImGui::Begin("Options", 0);

    // display current camera parameters
    ImGui::Text("Application %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
    ImGui::SliderFloat("Pitch", &m_pitch, -pi<float>() / 2, pi<float>() / 2, "%.2f");
    ImGui::SliderFloat("Yaw", &m_yaw, -pi<float>(), pi<float>(), "%.2f");
    ImGui::SliderFloat("Distance", &m_distance, 0, 2000, "%.2f", 2.0f);
    ImGui::Checkbox("First person camera", &firstPersonCamera);
    ImGui::SliderFloat("Camera speed", &cameraSpeed, 0.01f, 0.5f, "%.2f");

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
	ImGui::SliderFloat("Mesh Height", &m_terrain.meshHeight, 0.1f, 100.0f, "%.1f", 3.0f);
    // Water is the same size and resolution as the terrain.
    if (ImGui::SliderFloat("Mesh Size", &m_terrain.meshScale, 2.0f, 500.0f, "%.1f", 4.0f)) {
        m_water.meshScale = m_terrain.meshScale; // Water is the same size as the terrain.
    }
    if (ImGui::SliderInt("Mesh Resolution", &m_terrain.meshResolution, 10, 500, "%.0f")) {
        m_water.meshResolution = m_terrain.meshResolution;
    }
	ImGui::SliderFloat("Texture Size", &m_terrain.textureScale, 1.0f, 200.0f, "%.1f");

	bool meshNeedsUpdate = false;
	// Generates the mesh and shaders for terrain and water.
	if (ImGui::Button("Generate")) {
		meshNeedsUpdate = true;
		m_terrain.createMesh();
		m_terrain.setShaderParams();
		m_water.createMesh();
		m_water.setShaderParams();
    m_cached_water_height = m_water.waterHeight;
	}

	ImGui::Separator();
    ImGui::Text("Water Parameters");
    ImGui::SliderFloat("Water Height", &m_water.waterHeight, -5.0f, 2.0f);
    ImGui::SliderFloat("Water Opacity", &m_water.waterAlpha, 0.0f, 1.0f);
    ImGui::SliderFloat("Water Speed", &m_water.waterSpeed, 0.0f, 2.0f);
	ImGui::SliderFloat("Water Amplitude", &m_water.waterAmplitude, 0.0f, 0.5f, "%.3f", 4.0f);
	ImGui::Checkbox("Enable Water Reflections", &m_enable_water_reflections);
	if (m_enable_water_reflections) {
		ImGui::SliderFloat("Wave Distortion Strength", &m_water_wave_strength, 0.0f, 0.3f, "%.3f");
		ImGui::SliderFloat("Reflection Blend", &m_water_reflection_blend, 0.0f, 1.0f, "%.2f");

		const char* resolutions[] = {"960x540 (Half)", "1280x720 (HD)", "1920x1080 (Full HD)", "2560x1440 (2K)"};
		int currentRes = 2;
		if (m_water_fbo_width == 960) currentRes = 0;
		else if (m_water_fbo_width == 1280) currentRes = 1;
		else if (m_water_fbo_width == 1920) currentRes = 2;
		else if (m_water_fbo_width == 2560) currentRes = 3;

		if (ImGui::Combo("Reflection Resolution", &currentRes, resolutions, 4)) {
			if (currentRes == 0) { m_water_fbo_width = 960; m_water_fbo_height = 540; }
			else if (currentRes == 1) { m_water_fbo_width = 1280; m_water_fbo_height = 720; }
			else if (currentRes == 2) { m_water_fbo_width = 1920; m_water_fbo_height = 1080; }
			else if (currentRes == 3) { m_water_fbo_width = 2560; m_water_fbo_height = 1440; }

			// Rebuild framebuffers with new resolution
			// Delete old FBOs
			glDeleteFramebuffers(1, &m_reflection_fbo);
			glDeleteTextures(1, &m_reflection_texture);
			glDeleteRenderbuffers(1, &m_reflection_depth_buffer);
			glDeleteFramebuffers(1, &m_refraction_fbo);
			glDeleteTextures(1, &m_refraction_texture);
			glDeleteRenderbuffers(1, &m_refraction_depth_buffer);

			// Recreate reflection FBO
			glGenFramebuffers(1, &m_reflection_fbo);
			glBindFramebuffer(GL_FRAMEBUFFER, m_reflection_fbo);
			glGenTextures(1, &m_reflection_texture);
			glBindTexture(GL_TEXTURE_2D, m_reflection_texture);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, m_water_fbo_width, m_water_fbo_height, 0, GL_RGB, GL_UNSIGNED_BYTE, nullptr);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_reflection_texture, 0);
			glGenRenderbuffers(1, &m_reflection_depth_buffer);
			glBindRenderbuffer(GL_RENDERBUFFER, m_reflection_depth_buffer);
			glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, m_water_fbo_width, m_water_fbo_height);
			glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, m_reflection_depth_buffer);

			// Recreate refraction FBO
			glGenFramebuffers(1, &m_refraction_fbo);
			glBindFramebuffer(GL_FRAMEBUFFER, m_refraction_fbo);
			glGenTextures(1, &m_refraction_texture);
			glBindTexture(GL_TEXTURE_2D, m_refraction_texture);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, m_water_fbo_width, m_water_fbo_height, 0, GL_RGB, GL_UNSIGNED_BYTE, nullptr);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_refraction_texture, 0);
			glGenRenderbuffers(1, &m_refraction_depth_buffer);
			glBindRenderbuffer(GL_RENDERBUFFER, m_refraction_depth_buffer);
			glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, m_water_fbo_width, m_water_fbo_height);
			glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, m_refraction_depth_buffer);

			glBindFramebuffer(GL_FRAMEBUFFER, 0);
		}
	}

	ImGui::Separator();
	ImGui::Text("Sun, Lighting & Fog");
	if (ImGui::SliderFloat("Sun Azimuth", &m_sunAzimuth, 0.0f, 360.0f, "%.1f°")) {
		updateLightFromSun();
	}
	if (ImGui::SliderFloat("Sun Elevation", &m_sunElevation, -90.0f, 90.0f, "%.1f°")) {
		updateLightFromSun();
	}
	if (ImGui::SliderFloat("Sun Intensity", &m_sunIntensity, 0.5f, 3.0f, "%.2f")) {
		updateLightFromSun();
	}
	ImGui::Checkbox("Use fog", &useFog);
	ImGui::SameLine();
	const char* fogTypes[] = { "Linear", "Exponential" };
	ImGui::Combo("Fog type", &fogType, fogTypes, 2);
	ImGui::SliderFloat("Fog Density", &fogDensity, 0.001f, 0.1f, "%.3f", 2);

	ImGui::Separator();
	ImGui::Text("Shadow Settings");
	ImGui::Checkbox("Enable Shadows", &m_enable_shadows);
	if (m_enable_shadows) {
		ImGui::Checkbox("Use PCF (Soft Shadows)", &m_use_pcf);
		const char* shadowMapSizes[] = {"512", "1024", "2048", "4096"};
		int currentSizeIndex = 0;
		if (m_shadow_map_size == 512) currentSizeIndex = 0;
		else if (m_shadow_map_size == 1024) currentSizeIndex = 1;
		else if (m_shadow_map_size == 2048) currentSizeIndex = 2;
		else if (m_shadow_map_size == 4096) currentSizeIndex = 3;

		if (ImGui::Combo("Shadow Map Size", &currentSizeIndex, shadowMapSizes, 4)) {
			int newSize = 2048;
			if (currentSizeIndex == 0) newSize = 512;
			else if (currentSizeIndex == 1) newSize = 1024;
			else if (currentSizeIndex == 2) newSize = 2048;
			else if (currentSizeIndex == 3) newSize = 4096;

			if (newSize != m_shadow_map_size) {
				m_shadow_map_size = newSize;
				// Recreate shadow map texture with new size
				// to make sure we're not interfering with any active texture units
				GLint currentTexture;
				glGetIntegerv(GL_ACTIVE_TEXTURE, &currentTexture);

				glActiveTexture(GL_TEXTURE11);
				glBindTexture(GL_TEXTURE_2D, m_shadow_map_texture);
				glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, m_shadow_map_size, m_shadow_map_size, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
				float borderColor[] = { 1.0f, 1.0f, 1.0f, 1.0f };
				glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);

				// Enable shadow comparison mode for sampler2DShadow
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);

				// Restore previous active texture unit
				glActiveTexture(currentTexture);
			}
		}
	}

	ImGui::Separator();
	ImGui::Text("Lens Flare Settings");
	ImGui::Checkbox("Enable Lens Flare", &m_enable_lens_flare);
	if (m_enable_lens_flare) {
		ImGui::Text("Bright Parts Extraction");
		ImGui::SliderFloat("Brightness Threshold", &m_bright_threshold, 0.0f, 3.0f, "%.2f");
		ImGui::Checkbox("Smooth Gradient", &m_bright_smooth_gradient);

		ImGui::Text("Blur Settings");
		ImGui::SliderInt("Blur Iterations", &m_blur_iterations, 1, 20);
		ImGui::SliderFloat("Blur Intensity", &m_blur_intensity, 0.1f, 2.0f, "%.2f");

		ImGui::Text("Ghost/Halo Settings");
		const char* lensTypes[] = {"Ghost", "Halo", "Both"};
		ImGui::Combo("Lens Type", &m_lens_flare_type, lensTypes, 3);
		ImGui::Checkbox("Use Lens Texture", &m_lens_use_texture);
		ImGui::SliderInt("Ghost Count", &m_ghost_count, 1, 32);
		ImGui::SliderFloat("Ghost Dispersal", &m_ghost_dispersal, 0.0f, 0.75f, "%.2f");
		ImGui::SliderFloat("Ghost Threshold", &m_ghost_threshold, 0.0f, 30.0f, "%.1f");
		ImGui::SliderFloat("Ghost Distortion", &m_ghost_distortion, 0.0f, 10.0f, "%.1f");
		ImGui::SliderFloat("Halo Radius", &m_halo_radius, 0.0f, 0.65f, "%.2f");
		ImGui::SliderFloat("Halo Threshold", &m_halo_threshold, 0.0f, 30.0f, "%.1f");

		ImGui::Text("Composite Settings");
		ImGui::Checkbox("Use Lens Dirt/Starburst", &m_lens_use_dirt);
		ImGui::SliderFloat("Global Brightness", &m_lens_global_brightness, 0.0f, 0.01f, "%.4f");
	}

	ImGui::Separator();
	ImGui::Text("Bloom Settings");
	ImGui::Checkbox("Enable Bloom", &m_enable_bloom);
	if (m_enable_bloom) {
		ImGui::Text("Bloom creates a glow effect around bright objects like the sun");
		ImGui::SliderInt("Bloom Blur Iterations", &m_bloom_blur_iterations, 1, 20);
		ImGui::SliderFloat("Bloom Blur Intensity", &m_bloom_blur_intensity, 0.1f, 5.0f, "%.2f");
		ImGui::SliderFloat("Bloom Strength", &m_bloom_strength, 0.0f, 0.2f, "%.4f");
		ImGui::Checkbox("Anamorphic Bloom (cinematic horizontal streaks)", &m_bloom_anamorphic);
		if (m_bloom_anamorphic) {
			ImGui::SliderFloat("Anamorphic Ratio", &m_bloom_anamorphic_ratio, 0.1f, 1.0f, "%.2f");
		}
	}

    // L-System parameters that affect mesh generation
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

    ImGui::Separator();
    ImGui::Text("Leaf Parameters");

    if (ImGui::Checkbox("Render Leaves", &m_trees.renderLeaves)) {
        // Just visual toggle
    }
    if (ImGui::SliderFloat("Leaf Size", &m_trees.leafSize, 0.1f, 1.0f, "%.2f")) {
        meshNeedsUpdate = true;
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


vec3 Application::getSunColor(float elevation) {
	// Sun color transitions based on elevation angle
	// Daytime (above 15°): White/yellow (1.0, 1.0, 0.95)
	// Sunrise/Sunset (15° to -5°): Orange to deep red
	// Night (below -5°): Black
    // Maybe add GUI params for controlling these colors?

	if (elevation > 15.0f) {
		// Full daylight
		return vec3(1.0f, 1.0f, 0.95f);
	}
	else if (elevation > 5.0f) {
		// Blend to warm yellow
		float t = smoothstep(5.0f, 15.0f, elevation);
		vec3 warmYellow = vec3(1.0f, 0.95f, 0.8f);
		vec3 dayWhite = vec3(1.0f, 1.0f, 0.95f);
		return mix(warmYellow, dayWhite, t);
	}
	else if (elevation > -2.0f) {
		// Sunset/sunrise
		float t = smoothstep(-2.0f, 5.0f, elevation);
		vec3 deepOrange = vec3(1.0f, 0.4f, 0.1f);
		vec3 warmYellow = vec3(1.0f, 0.95f, 0.8f);
		return mix(deepOrange, warmYellow, t);
	}
	else if (elevation > -10.0f) {
		// Deep red to black
		float t = smoothstep(-10.0f, -2.0f, elevation);
		vec3 black = vec3(0.0f, 0.0f, 0.0f);
		vec3 deepRed = vec3(0.8f, 0.2f, 0.0f);
		return mix(black, deepRed, t);
	}
	else {
		// Night
		return vec3(0.0f, 0.0f, 0.0f);
	}
}

vec3 Application::getSkyColor(float elevation) {
	// Sky color transitions based on sun elevation
	// Simply darken from day blue to night black

	if (elevation > 0.0f) {
		// Daytime
		float brightness = smoothstep(0.0f, 30.0f, elevation);
		vec3 daySky = vec3(0.53f, 0.81f, 0.92f);
		vec3 eveningSky = vec3(0.3f, 0.5f, 0.7f);
		return mix(eveningSky, daySky, brightness);
	}
	else if (elevation > -20.0f) {
		// Sunset to night
		float t = smoothstep(-20.0f, 0.0f, elevation);
		vec3 nightSky = vec3(0.02f, 0.02f, 0.08f);
        vec3 eveningSky = vec3(0.3f, 0.5f, 0.7f);
		return mix(nightSky, eveningSky, t);
	}
	else {
		// Night dark blue
		return vec3(0.02f, 0.02f, 0.08f);
	}
}

void Application::updateLightFromSun() {
	// Convert angles from degrees to radians
	float azimuthRad = radians(m_sunAzimuth);
	float elevationRad = radians(m_sunElevation);

	// Calculate sun position in spherical coordinates (normalized direction from origin to sun)
	vec3 sunDirection;
	sunDirection.x = cos(elevationRad) * cos(azimuthRad);
	sunDirection.y = sin(elevationRad);
	sunDirection.z = cos(elevationRad) * sin(azimuthRad);

	m_terrain.lightDirection = -normalize(sunDirection);

	// Update light color based on sun elevation and apply intensity
	m_terrain.lightColor = getSunColor(m_sunElevation) * m_sunIntensity;
}

glm::mat4 Application::getLightSpaceMatrix() const {
	// Calculate sun direction from angles
	float azimuthRad = radians(m_sunAzimuth);
	float elevationRad = radians(m_sunElevation);

	vec3 sunDirection;
	sunDirection.x = cos(elevationRad) * cos(azimuthRad);
	sunDirection.y = sin(elevationRad);
	sunDirection.z = cos(elevationRad) * sin(azimuthRad);

	vec3 lightPos = normalize(sunDirection) * 250.0f;

	// Orthographic projection covering the terrain
	// Adjust size based on sun elevation (larger when sun is low for longer shadows)
	float elevationFactor = std::max(0.3f, abs(sin(elevationRad)));
	float orthoSize = m_terrain.meshScale * 1.5f / elevationFactor;
	mat4 lightProjection = glm::ortho(
		-orthoSize, orthoSize,
		-orthoSize, orthoSize,
		1.0f, 500.0f  // Near/far planes (adjusted for better coverage)
	);

	// Look from light position toward scene center
	mat4 lightView = glm::lookAt(
		lightPos,
		vec3(0.0f, 0.0f, 0.0f),  // Look at origin
		vec3(0.0f, 1.0f, 0.0f)   // Up vector
	);

	return lightProjection * lightView;
}

void Application::renderShadowMap() {
	// Save viewport state
	int viewport[4];
	glGetIntegerv(GL_VIEWPORT, viewport);

	// Bind shadow framebuffer
	glBindFramebuffer(GL_FRAMEBUFFER, m_shadow_map_fbo);
	glViewport(0, 0, m_shadow_map_size, m_shadow_map_size);

	// Ensure depth writing is enabled (might be disabled by skybox rendering)
	glDepthMask(GL_TRUE);

	// Depth-only rendering
	glDrawBuffer(GL_NONE);
	glReadBuffer(GL_NONE);

	glClear(GL_DEPTH_BUFFER_BIT);

	// Enable depth testing
	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LESS);

	// Cull front faces to prevent shadow acne
	glEnable(GL_CULL_FACE);
	glCullFace(GL_FRONT);

	// Calculate light space matrix
	glm::mat4 lightSpaceMatrix = getLightSpaceMatrix();

	// Use shadow depth shader
	glUseProgram(m_shadow_depth_shader);
	glUniformMatrix4fv(glGetUniformLocation(m_shadow_depth_shader, "uLightSpaceMatrix"),
					   1, GL_FALSE, glm::value_ptr(lightSpaceMatrix));

	// render terrain
	glUniform1i(glGetUniformLocation(m_shadow_depth_shader, "uUseInstancing"), 0);
	m_terrain.terrain.draw();

	// render trees
	if (!m_trees.treeTransforms.empty()) {
		glUniform1i(glGetUniformLocation(m_shadow_depth_shader, "uUseInstancing"), 1);

		// Bind tree mesh VAO (with instance attributes already set up)
		glBindVertexArray(m_trees.treeMesh.vao);
		glDrawElementsInstanced(GL_TRIANGLES,
							   m_trees.treeMesh.index_count,
							   GL_UNSIGNED_INT,
							   0,
							   m_trees.treeTransforms.size());
		glBindVertexArray(0);
	}

	// render leaves
	if (!m_trees.leafTransforms.empty() && m_trees.renderLeaves) {
		glUniform1i(glGetUniformLocation(m_shadow_depth_shader, "uUseInstancing"), 1);

		// Disable culling for leaves
		glDisable(GL_CULL_FACE);

		glBindVertexArray(m_trees.leafMesh.vao);
		glDrawElementsInstanced(GL_TRIANGLES,
							   m_trees.leafMesh.index_count,
							   GL_UNSIGNED_INT,
							   0,
							   m_trees.leafTransforms.size());
		glBindVertexArray(0);

		glEnable(GL_CULL_FACE);
	}

	// Restore culling state
	glCullFace(GL_BACK);
	glDisable(GL_CULL_FACE);

	// Restore viewport and unbind framebuffer
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glViewport(viewport[0], viewport[1], viewport[2], viewport[3]);

	// Ensure shadow map texture remains bound to GL_TEXTURE20
	// to prevent texture unit conflicts during reflection/refraction passes
	glActiveTexture(GL_TEXTURE20);
	glBindTexture(GL_TEXTURE_2D, m_shadow_map_texture);
}

void Application::renderScreenQuad() {
	glBindVertexArray(m_screen_quad_vao);
	glDrawArrays(GL_TRIANGLES, 0, 6);
	glBindVertexArray(0);
}

void Application::renderLensFlare() {
	if (!m_enable_lens_flare) return;

	// Save current state
	GLint currentViewport[4];
	glGetIntegerv(GL_VIEWPORT, currentViewport);
	int width = currentViewport[2];
	int height = currentViewport[3];

	// Disable depth testing and face culling for post-processing
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_CULL_FACE);

	// Pass 1: Extract bright parts from the scene
	glBindFramebuffer(GL_FRAMEBUFFER, m_bright_parts_fbo);
	glViewport(0, 0, width, height);
	glClear(GL_COLOR_BUFFER_BIT);

	glUseProgram(m_bright_parts_shader);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, m_scene_texture);  // Use the captured scene texture
	glUniform1i(glGetUniformLocation(m_bright_parts_shader, "uSceneTexture"), 0);
	glUniform1f(glGetUniformLocation(m_bright_parts_shader, "uThreshold"), m_bright_threshold);
	glUniform1i(glGetUniformLocation(m_bright_parts_shader, "uSmoothGradient"), m_bright_smooth_gradient);

	renderScreenQuad();

	// Pass 2: Blur bright parts using ping-pong technique
	bool horizontal = true;
	bool first_iteration = true;

	glUseProgram(m_gaussian_blur_shader);
	glUniform1f(glGetUniformLocation(m_gaussian_blur_shader, "uIntensity"), m_blur_intensity);

	for (int i = 0; i < m_blur_iterations; i++) {
		glBindFramebuffer(GL_FRAMEBUFFER, m_pingpong_fbo[horizontal]);
		glViewport(0, 0, width / 2, height / 2);
		glClear(GL_COLOR_BUFFER_BIT);

		glUniform1i(glGetUniformLocation(m_gaussian_blur_shader, "uHorizontal"), horizontal);

		glActiveTexture(GL_TEXTURE0);
		if (first_iteration) {
			glBindTexture(GL_TEXTURE_2D, m_bright_parts_texture);
		} else {
			glBindTexture(GL_TEXTURE_2D, m_pingpong_texture[!horizontal]);
		}
		glUniform1i(glGetUniformLocation(m_gaussian_blur_shader, "uTexture"), 0);

		renderScreenQuad();

		horizontal = !horizontal;
		if (first_iteration) first_iteration = false;
	}

	// Pass 3: Generate ghost/halo artifacts
	glBindFramebuffer(GL_FRAMEBUFFER, m_lens_flare_fbo);
	glViewport(0, 0, width, height);
	glClear(GL_COLOR_BUFFER_BIT);

	glUseProgram(m_lens_flare_ghost_shader);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, m_pingpong_texture[!horizontal]);
	glUniform1i(glGetUniformLocation(m_lens_flare_ghost_shader, "uBrightTexture"), 0);

	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, m_lens_color_texture);
	glUniform1i(glGetUniformLocation(m_lens_flare_ghost_shader, "uLensColorTexture"), 1);

	glActiveTexture(GL_TEXTURE2);
	glBindTexture(GL_TEXTURE_2D, m_lens_texture);
	glUniform1i(glGetUniformLocation(m_lens_flare_ghost_shader, "uLensMaskTexture"), 2);

	glUniform1i(glGetUniformLocation(m_lens_flare_ghost_shader, "uLensType"), m_lens_flare_type);
	glUniform1i(glGetUniformLocation(m_lens_flare_ghost_shader, "uUseLensTexture"), m_lens_use_texture);
	glUniform1i(glGetUniformLocation(m_lens_flare_ghost_shader, "uGhostCount"), m_ghost_count);
	glUniform1f(glGetUniformLocation(m_lens_flare_ghost_shader, "uGhostDispersal"), m_ghost_dispersal);
	glUniform1f(glGetUniformLocation(m_lens_flare_ghost_shader, "uGhostThreshold"), m_ghost_threshold);
	glUniform1f(glGetUniformLocation(m_lens_flare_ghost_shader, "uGhostDistortion"), m_ghost_distortion);
	glUniform1f(glGetUniformLocation(m_lens_flare_ghost_shader, "uHaloRadius"), m_halo_radius);
	glUniform1f(glGetUniformLocation(m_lens_flare_ghost_shader, "uHaloThreshold"), m_halo_threshold);

	renderScreenQuad();

	// Restore default framebuffer
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glViewport(currentViewport[0], currentViewport[1], currentViewport[2], currentViewport[3]);

	// Re-enable depth testing
	glEnable(GL_DEPTH_TEST);
}

void Application::renderBloom() {
	if (!m_enable_bloom) return;

	// Save current state
	GLint currentViewport[4];
	glGetIntegerv(GL_VIEWPORT, currentViewport);
	int width = currentViewport[2];
	int height = currentViewport[3];

	// Disable depth testing and face culling for post-processing
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_CULL_FACE);

	// Blur bright parts using ping-pong technique

	bool horizontal = true;
	bool first_iteration = true;

	glUseProgram(m_gaussian_blur_shader);
	glUniform1f(glGetUniformLocation(m_gaussian_blur_shader, "uIntensity"), m_bloom_blur_intensity);
	glUniform1i(glGetUniformLocation(m_gaussian_blur_shader, "uAnamorphic"), m_bloom_anamorphic);
	glUniform1f(glGetUniformLocation(m_gaussian_blur_shader, "uAnamorphicRatio"), m_bloom_anamorphic_ratio);

	for (int i = 0; i < m_bloom_blur_iterations; i++) {
		glBindFramebuffer(GL_FRAMEBUFFER, m_bloom_pingpong_fbo[horizontal]);
		glViewport(0, 0, width, height);
		glClear(GL_COLOR_BUFFER_BIT);

		glUniform1i(glGetUniformLocation(m_gaussian_blur_shader, "uHorizontal"), horizontal);

		glActiveTexture(GL_TEXTURE0);
		if (first_iteration) {
			// First iteration: blur the bright parts extracted via MRT
			glBindTexture(GL_TEXTURE_2D, m_bloom_texture);
		} else {
			// Subsequent iterations: blur the result from previous pass
			glBindTexture(GL_TEXTURE_2D, m_bloom_pingpong_texture[!horizontal]);
		}
		glUniform1i(glGetUniformLocation(m_gaussian_blur_shader, "uTexture"), 0);

		renderScreenQuad();

		horizontal = !horizontal;
		if (first_iteration) first_iteration = false;
	}

	// Restore state
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glViewport(currentViewport[0], currentViewport[1], currentViewport[2], currentViewport[3]);
	glEnable(GL_DEPTH_TEST);
}

void Application::compositeLensFlare(GLuint sceneTexture) {
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_CULL_FACE);

	glUseProgram(m_lens_flare_composite_shader);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, sceneTexture);
	glUniform1i(glGetUniformLocation(m_lens_flare_composite_shader, "uSceneTexture"), 0);

	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, m_lens_flare_texture);
	glUniform1i(glGetUniformLocation(m_lens_flare_composite_shader, "uFlareTexture"), 1);

	glActiveTexture(GL_TEXTURE2);
	glBindTexture(GL_TEXTURE_2D, m_lens_dirt_texture);
	glUniform1i(glGetUniformLocation(m_lens_flare_composite_shader, "uLensDirtTexture"), 2);

	glActiveTexture(GL_TEXTURE3);
	glBindTexture(GL_TEXTURE_2D, m_lens_starburst_texture);
	glUniform1i(glGetUniformLocation(m_lens_flare_composite_shader, "uLensStarTexture"), 3);

	// Bind bloom texture
	glActiveTexture(GL_TEXTURE4);
	// Use the final blurred bloom from ping-pong buffer
	// After bloom blur, the result is in m_bloom_pingpong_texture[!horizontal]
	// Since we did m_bloom_blur_iterations, calculate which buffer has the final result
	bool horizontal = (m_bloom_blur_iterations % 2 == 0);
	glBindTexture(GL_TEXTURE_2D, m_bloom_pingpong_texture[!horizontal]);
	glUniform1i(glGetUniformLocation(m_lens_flare_composite_shader, "uBloomTexture"), 4);

	glUniform1i(glGetUniformLocation(m_lens_flare_composite_shader, "uUseDirt"), m_lens_use_dirt);
	glUniform1f(glGetUniformLocation(m_lens_flare_composite_shader, "uGlobalBrightness"), m_lens_global_brightness);
	glUniform1i(glGetUniformLocation(m_lens_flare_composite_shader, "uEnableBloom"), m_enable_bloom);
	glUniform1f(glGetUniformLocation(m_lens_flare_composite_shader, "uBloomStrength"), m_bloom_strength);

	// Calculate lens star matrix based on camera rotation (makes the starburst rotate with camera movement)
	mat4 view;
	if (firstPersonCamera) {
		view = rotate(mat4(1), m_pitch, vec3(1, 0, 0))
			* rotate(mat4(1), m_yaw, vec3(0, 1, 0))
			* translate(mat4(1), -cameraPosition);
	} else {
		view = translate(mat4(1), vec3(0, 0, -m_distance))
			* rotate(mat4(1), m_pitch, vec3(1, 0, 0))
			* rotate(mat4(1), m_yaw, vec3(0, 1, 0));
	}

	vec4 camx = view[0];
	vec4 camz = view[1];
	float camrot = dot(vec3(camx), vec3(0.0f, 0.0f, 1.0f)) + dot(vec3(camz), vec3(0.0f, 1.0f, 0.0f));

	mat3 scaleBias1 = mat3(
		2.0f,  0.0f, -1.0f,
		0.0f,  2.0f, -1.0f,
		0.0f,  0.0f,  1.0f
	);
	mat3 rotation = mat3(
		cos(camrot), -sin(camrot), 0.0f,
		sin(camrot),  cos(camrot), 0.0f,
		0.0f,         0.0f,        1.0f
	);
	mat3 scaleBias2 = mat3(
		0.5f, 0.0f, 0.5f,
		0.0f, 0.5f, 0.5f,
		0.0f, 0.0f, 1.0f
	);

	mat3 lensStarMatrix = scaleBias2 * rotation * scaleBias1;
	glUniformMatrix3fv(glGetUniformLocation(m_lens_flare_composite_shader, "uLensStarMatrix"), 1, GL_FALSE, value_ptr(lensStarMatrix));

	renderScreenQuad();

	glEnable(GL_DEPTH_TEST);
	glEnable(GL_CULL_FACE);
}

void Application::recreateLensFlareFBOs(int width, int height) {
	// Delete old FBOs and textures
	if (m_scene_fbo) glDeleteFramebuffers(1, &m_scene_fbo);
	if (m_scene_texture) glDeleteTextures(1, &m_scene_texture);
	if (m_scene_depth_buffer) glDeleteRenderbuffers(1, &m_scene_depth_buffer);
	if (m_bright_parts_fbo) glDeleteFramebuffers(1, &m_bright_parts_fbo);
	if (m_bright_parts_texture) glDeleteTextures(1, &m_bright_parts_texture);
	if (m_pingpong_fbo[0]) glDeleteFramebuffers(1, &m_pingpong_fbo[0]);
	if (m_pingpong_fbo[1]) glDeleteFramebuffers(1, &m_pingpong_fbo[1]);
	if (m_pingpong_texture[0]) glDeleteTextures(1, &m_pingpong_texture[0]);
	if (m_pingpong_texture[1]) glDeleteTextures(1, &m_pingpong_texture[1]);
	if (m_lens_flare_fbo) glDeleteFramebuffers(1, &m_lens_flare_fbo);
	if (m_lens_flare_texture) glDeleteTextures(1, &m_lens_flare_texture);
	if (m_bloom_texture) glDeleteTextures(1, &m_bloom_texture);
	if (m_bloom_pingpong_fbo[0]) glDeleteFramebuffers(1, &m_bloom_pingpong_fbo[0]);
	if (m_bloom_pingpong_fbo[1]) glDeleteFramebuffers(1, &m_bloom_pingpong_fbo[1]);
	if (m_bloom_pingpong_texture[0]) glDeleteTextures(1, &m_bloom_pingpong_texture[0]);
	if (m_bloom_pingpong_texture[1]) glDeleteTextures(1, &m_bloom_pingpong_texture[1]);

	// Scene capture FBO (full resolution) with MRT support for bloom
	glGenFramebuffers(1, &m_scene_fbo);
	glBindFramebuffer(GL_FRAMEBUFFER, m_scene_fbo);

	// Color attachment 0: regular scene
	glGenTextures(1, &m_scene_texture);
	glBindTexture(GL_TEXTURE_2D, m_scene_texture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, width, height, 0, GL_RGB, GL_FLOAT, nullptr);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_scene_texture, 0);

	// Color attachment 1: bright parts (for bloom)
	glGenTextures(1, &m_bloom_texture);
	glBindTexture(GL_TEXTURE_2D, m_bloom_texture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, width, height, 0, GL_RGB, GL_FLOAT, nullptr);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, m_bloom_texture, 0);

	// Tell OpenGL we're rendering to both color attachments
	unsigned int attachments[2] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1 };
	glDrawBuffers(2, attachments);

	// Depth buffer
	glGenRenderbuffers(1, &m_scene_depth_buffer);
	glBindRenderbuffer(GL_RENDERBUFFER, m_scene_depth_buffer);
	glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, width, height);
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, m_scene_depth_buffer);

	// Bright parts FBO
	glGenFramebuffers(1, &m_bright_parts_fbo);
	glBindFramebuffer(GL_FRAMEBUFFER, m_bright_parts_fbo);
	glGenTextures(1, &m_bright_parts_texture);
	glBindTexture(GL_TEXTURE_2D, m_bright_parts_texture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, width, height, 0, GL_RGB, GL_FLOAT, nullptr);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_bright_parts_texture, 0);

	// Ping-pong FBOs
	for (int i = 0; i < 2; i++) {
		glGenFramebuffers(1, &m_pingpong_fbo[i]);
		glBindFramebuffer(GL_FRAMEBUFFER, m_pingpong_fbo[i]);
		glGenTextures(1, &m_pingpong_texture[i]);
		glBindTexture(GL_TEXTURE_2D, m_pingpong_texture[i]);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, width / 2, height / 2, 0, GL_RGB, GL_FLOAT, nullptr);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_pingpong_texture[i], 0);
	}

	// Lens flare output FBO
	glGenFramebuffers(1, &m_lens_flare_fbo);
	glBindFramebuffer(GL_FRAMEBUFFER, m_lens_flare_fbo);
	glGenTextures(1, &m_lens_flare_texture);
	glBindTexture(GL_TEXTURE_2D, m_lens_flare_texture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, width, height, 0, GL_RGB, GL_FLOAT, nullptr);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_lens_flare_texture, 0);

	// Bloom ping-pong FBOs (full resolution for better quality)
	for (int i = 0; i < 2; i++) {
		glGenFramebuffers(1, &m_bloom_pingpong_fbo[i]);
		glBindFramebuffer(GL_FRAMEBUFFER, m_bloom_pingpong_fbo[i]);
		glGenTextures(1, &m_bloom_pingpong_texture[i]);
		glBindTexture(GL_TEXTURE_2D, m_bloom_pingpong_texture[i]);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, width, height, 0, GL_RGB, GL_FLOAT, nullptr);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_bloom_pingpong_texture[i], 0);
	}

	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	// Update stored size
	m_lens_flare_fbo_width = width;
	m_lens_flare_fbo_height = height;
}
