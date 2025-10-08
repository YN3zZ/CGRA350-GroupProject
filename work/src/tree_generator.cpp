#include "tree_generator.hpp"
#include "perlin_noise.hpp"
#include "cgra/cgra_image.hpp"
#include "cgra/cgra_shader.hpp"
#include "cgra/cgra_wavefront.hpp"
#include <random>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <iostream>

using namespace std;
using namespace glm;
using namespace cgra;

TreeGenerator::~TreeGenerator() {
    // Clean up OpenGL resources
    if (instanceVBO != 0) {
        glDeleteBuffers(1, &instanceVBO);
    }
    if (leafInstanceVBO != 0) {
        glDeleteBuffers(1, &leafInstanceVBO);
    }
    if (leafTexture != 0) {
        glDeleteTextures(1, &leafTexture);
    }
}

void TreeGenerator::loadTextures() {
    const vector<string> barkTexturePaths = {
        "ash-tree-bark_albedo.png",
        "ash-tree-bark_normal-ogl.png",
        "ash-tree-bark_roughness.png",
        "ash-tree-bark_metallic.png"
    };

    for (int i = 0; i < barkTexturePaths.size(); i++) {
        string path = CGRA_SRCDIR + string("/res/textures/") + barkTexturePaths[i];
        rgba_image textureImage = rgba_image(string(path));
        barkTextures.push_back(textureImage.uploadTexture());
    }

    useTextures = true;

    // Load leaf texture
    string leafPath = CGRA_SRCDIR + string("/res/textures/leaves_texture.png");
    rgba_image leafImage = rgba_image(leafPath);
    leafTexture = leafImage.uploadTexture();

    shader_builder sb_leaf;
    sb_leaf.set_shader(GL_VERTEX_SHADER, CGRA_SRCDIR + string("//res//shaders//leaf_vert.glsl"));
    sb_leaf.set_shader(GL_FRAGMENT_SHADER, CGRA_SRCDIR + string("//res//shaders//leaf_frag.glsl"));
    leafShader = sb_leaf.build();
}

void TreeGenerator::regenerateTreeMesh() {
    // Update L-system parameters
    lSystem.cylinderSides = 12;
    lSystem.branchTaper = branchTaper;

    // Generate the tree mesh and collect end nodes and directions for leaves
    string lSystemString = lSystem.generateString();
    baseLeafPositions.clear();
    baseLeafDirections.clear();
    treeMesh = lSystem.generateTreeMesh(lSystemString, baseLeafPositions, baseLeafDirections);

    // Generate leaf mesh
    generateLeafMesh();

    needsMeshRegeneration = false;
}

void TreeGenerator::setupInstancing() {
    // Ensure we have a tree mesh
    if (needsMeshRegeneration) {
        regenerateTreeMesh();
    }
    
    // Create instance buffer if it doesn't exist
    if (instanceVBO == 0) {
        glGenBuffers(1, &instanceVBO);
    }
    
    // Upload instance data (transform matrices)
    glBindBuffer(GL_ARRAY_BUFFER, instanceVBO);
    glBufferData(GL_ARRAY_BUFFER, 
                 treeTransforms.size() * sizeof(mat4),
                 treeTransforms.data(), 
                 GL_DYNAMIC_DRAW);
    
    // Bind the tree mesh VAO and add instance attributes to it
    glBindVertexArray(treeMesh.vao);
    
    // Bind instance buffer
    glBindBuffer(GL_ARRAY_BUFFER, instanceVBO);
    
    // Set up instance attributes
    size_t vec4Size = sizeof(vec4);
    for (unsigned int i = 0; i < 4; i++) {
        unsigned int attribLocation = 3 + i;
        glEnableVertexAttribArray(attribLocation);
        glVertexAttribPointer(attribLocation, 4, GL_FLOAT, GL_FALSE, 
                             sizeof(mat4), 
                             (void*)(i * vec4Size));
        glVertexAttribDivisor(attribLocation, 1); // This tells OpenGL this is per-instance data
    }
    
    // Unbind
    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void TreeGenerator::updateInstanceBuffer() {
    if (instanceVBO == 0 || treeTransforms.empty()) return;
    
    // Update only the buffer data without recreating it
    glBindBuffer(GL_ARRAY_BUFFER, instanceVBO);
    glBufferSubData(GL_ARRAY_BUFFER, 0, 
                    treeTransforms.size() * sizeof(mat4),
                    treeTransforms.data());
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void TreeGenerator::generateLeafMesh() {
    // Load OBJ mesh
    string objPath = CGRA_SRCDIR + string("/res/assets/leaves.obj");
    mesh_builder mb = load_wavefront_data(objPath);
    leafMesh = mb.build();
}

void TreeGenerator::setupLeafInstancing() {
    if (leafTransforms.empty()) return;

    // Create instance buffer if it doesn't exist
    if (leafInstanceVBO == 0) {
        glGenBuffers(1, &leafInstanceVBO);
    }

    // Upload instance data
    glBindBuffer(GL_ARRAY_BUFFER, leafInstanceVBO);
    glBufferData(GL_ARRAY_BUFFER,
                 leafTransforms.size() * sizeof(mat4),
                 leafTransforms.data(),
                 GL_DYNAMIC_DRAW);

    // Setup instance attributes for leaf mesh
    glBindVertexArray(leafMesh.vao);
    glBindBuffer(GL_ARRAY_BUFFER, leafInstanceVBO);

    size_t vec4Size = sizeof(vec4);
    for (unsigned int i = 0; i < 4; i++) {
        unsigned int attribLocation = 3 + i;
        glEnableVertexAttribArray(attribLocation);
        glVertexAttribPointer(attribLocation, 4, GL_FLOAT, GL_FALSE,
                             sizeof(mat4),
                             (void*)(i * vec4Size));
        glVertexAttribDivisor(attribLocation, 1);
    }

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void TreeGenerator::generateTreesOnTerrain(PerlinNoise* perlinNoise) {
    // Clear only the transforms, not the mesh
    treeTransforms.clear();
    leafTransforms.clear();

    // Mark that we need to regenerate the mesh if L-system parameters changed
    if (needsMeshRegeneration) {
        regenerateTreeMesh();
    }

    // Random placement
    mt19937 rng(42);
    float meshSize = perlinNoise->meshScale;
    uniform_real_distribution<float> positionDist(-meshSize * 0.8f, meshSize * 0.8f);
    uniform_real_distribution<float> scaleDist(minTreeScale, maxTreeScale);
    uniform_real_distribution<float> rotationDist(0.0f, 2.0f * pi<float>());

    for (int i = 0; i < treeCount; i++) {
        vec2 position(positionDist(rng), positionDist(rng));
        vec3 terrainPoint = perlinNoise->sampleVertex(position);

        float scale = scaleDist(rng);
        float rotation = randomRotation ? rotationDist(rng) : 0.0f;

        mat4 transform = translate(mat4(1.0f), terrainPoint) *
                        rotate(mat4(1.0f), rotation, vec3(0, 1, 0)) *
                        glm::scale(mat4(1.0f), vec3(scale));

        treeTransforms.push_back(transform);

        // Create leaf transforms for this tree instance aligned with branch direction
        for (size_t j = 0; j < baseLeafPositions.size(); j++) {
            vec3 leafPos = baseLeafPositions[j];
            vec3 branchDir = baseLeafDirections[j];

            // Transform position and direction to world space
            vec3 worldLeafPos = vec3(transform * vec4(leafPos, 1.0f));
            vec3 worldBranchDir = normalize(vec3(transform * vec4(branchDir, 0.0f)));

            // Create rotation matrix to align leaf with branch direction
            // The leaf mesh grows along the branch direction (Y-axis in local space)
            vec3 up = worldBranchDir;
            vec3 right = normalize(cross(vec3(0, 1, 0), up));
            if (length(right) < 0.001f) {
                right = normalize(cross(vec3(1, 0, 0), up));
            }
            vec3 forward = normalize(cross(up, right));

            // Construct orientation matrix
            mat4 orientation = mat4(
                vec4(right, 0),
                vec4(up, 0),
                vec4(forward, 0),
                vec4(0, 0, 0, 1)
            );

            // Combine position, orientation, and scale
            mat4 leafTransform = translate(mat4(1.0f), worldLeafPos) *
                                orientation *
                                glm::scale(mat4(1.0f), vec3(scale * leafSize));

            leafTransforms.push_back(leafTransform);
        }
    }

    // Set up instancing with the new transforms
    setupInstancing();
    setupLeafInstancing();
}

void TreeGenerator::setTreeType(int type) {
    lSystem.rules.clear();
    
    switch(type) {
        case 0: // Simple tree
            lSystem.axiom = "F";
            lSystem.rules['F'] = "FF+[+F-F-F]-[-F+F+F]";
            break;
        case 1: // Bushy tree
            lSystem.axiom = "X";
            lSystem.rules['X'] = "F[+X]F[-X]+X";
            lSystem.rules['F'] = "FF";
            break;
        case 2: // Willow-like
            lSystem.axiom = "F";
            lSystem.rules['F'] = "F[+F]F[-F][F]";
            break;
        case 3: // 3D tree
            lSystem.axiom = "F";
            lSystem.rules['F'] = "F[+&F][-&F][^F][/F]";
            break;
    }
    
    needsMeshRegeneration = true;
}

void TreeGenerator::drawLeaves(const mat4& view, const mat4& proj,
                               const vec3& lightDir, const vec3& lightColor,
                               const mat4& lightSpaceMatrix,
                               GLuint shadowMapTexture,
                               bool enableShadows,
                               bool usePCF) {
    if (leafTransforms.empty() || !renderLeaves || leafShader == 0) return;

    glUseProgram(leafShader);

    glUniformMatrix4fv(glGetUniformLocation(leafShader, "uProjectionMatrix"),
                        1, false, value_ptr(proj));
    glUniformMatrix4fv(glGetUniformLocation(leafShader, "uViewMatrix"),
                        1, false, value_ptr(view));

    // Compute camera position from view matrix
    mat4 invView = inverse(view);
    vec3 viewPos = vec3(invView[3]);

    // Set lighting uniforms
    glUniform3fv(glGetUniformLocation(leafShader, "uLightDir"), 1, value_ptr(lightDir));
    glUniform3fv(glGetUniformLocation(leafShader, "lightColor"), 1, value_ptr(lightColor));
    glUniform3fv(glGetUniformLocation(leafShader, "uViewPos"), 1, value_ptr(viewPos));

    // Shadow params
    glUniformMatrix4fv(glGetUniformLocation(leafShader, "uLightSpaceMatrix"), 1, false, value_ptr(lightSpaceMatrix));
    glUniform1i(glGetUniformLocation(leafShader, "uShadowMap"), 11);
    glUniform1i(glGetUniformLocation(leafShader, "uEnableShadows"), enableShadows ? 1 : 0);
    glUniform1i(glGetUniformLocation(leafShader, "uUsePCF"), usePCF ? 1 : 0);

    glActiveTexture(GL_TEXTURE16);
    glBindTexture(GL_TEXTURE_2D, leafTexture);
    glUniform1i(glGetUniformLocation(leafShader, "uLeafTexture"), 16);

    // Enable blending and disable backface culling for leaves and store previous state
    GLboolean blendEnabled;
    GLboolean cullFaceEnabled;
    glGetBooleanv(GL_BLEND, &blendEnabled);
    glGetBooleanv(GL_CULL_FACE, &cullFaceEnabled);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // Disable backface culling for double-sided leaves
    glDisable(GL_CULL_FACE);

    // Draw all leaf instances
    glBindVertexArray(leafMesh.vao);
    glDrawElementsInstanced(GL_TRIANGLES,
                           leafMesh.index_count,
                           GL_UNSIGNED_INT,
                           0,
                           leafTransforms.size());
    glBindVertexArray(0);

    // Restore previous state
    if (!blendEnabled) glDisable(GL_BLEND);
    if (cullFaceEnabled) glEnable(GL_CULL_FACE);
}

void TreeGenerator::draw(const mat4& view, const mat4& proj,
                         const vec3& lightDir, const vec3& lightColor,
                         const mat4& lightSpaceMatrix,
                         GLuint shadowMapTexture,
                         bool enableShadows,
                         bool usePCF) {
    if (treeTransforms.empty()) return;

    // Ensure mesh is generated
    if (needsMeshRegeneration) {
        regenerateTreeMesh();
        setupInstancing();
    }

    glUseProgram(shader);

    glUniformMatrix4fv(glGetUniformLocation(shader, "uProjectionMatrix"),
                        1, false, value_ptr(proj));
    glUniformMatrix4fv(glGetUniformLocation(shader, "uViewMatrix"),
                        1, false, value_ptr(view));

    // Set color uniform
    glUniform3fv(glGetUniformLocation(shader, "uColor"), 1, value_ptr(color));

    // compute camera position from view matrix
    mat4 invView = inverse(view);
    vec3 viewPos = vec3(invView[3]);

    // Set lighting uniforms
    glUniform3fv(glGetUniformLocation(shader, "lightColor"), 1, value_ptr(lightColor));
    glUniform3fv(glGetUniformLocation(shader, "uLightDir"), 1, value_ptr(lightDir));
    glUniform3fv(glGetUniformLocation(shader, "uViewPos"), 1, value_ptr(viewPos));

    glUniform1i(glGetUniformLocation(shader, "uUseTextures"), useTextures ? 1 : 0);

    // Shadow params
    glUniformMatrix4fv(glGetUniformLocation(shader, "uLightSpaceMatrix"), 1, false, value_ptr(lightSpaceMatrix));
    glUniform1i(glGetUniformLocation(shader, "uShadowMap"), 11);
    glUniform1i(glGetUniformLocation(shader, "uEnableShadows"), enableShadows ? 1 : 0);
    glUniform1i(glGetUniformLocation(shader, "uUsePCF"), usePCF ? 1 : 0);

    if (useTextures) {
        // Start from GL_TEXTURE4 to avoid conflicts with terrain textures
        const vector<string> textureUniforms = {
            "uAlbedoTexture",
            "uNormalTexture",
            "uRoughnessTexture",
            "uMetallicTexture"
        };

        for (int i = 0; i < barkTextures.size(); i++) {
            glActiveTexture(GL_TEXTURE8 + i);
            glBindTexture(GL_TEXTURE_2D, barkTextures[i]);
            glUniform1i(glGetUniformLocation(shader, textureUniforms[i].c_str()), 8 + i);
        }
    }

    // Bind VAO and draw all instances with one call
    glBindVertexArray(treeMesh.vao);
    glDrawElementsInstanced(GL_TRIANGLES,
                           treeMesh.index_count,
                           GL_UNSIGNED_INT,
                           0,
                           treeTransforms.size());
    glBindVertexArray(0);

    // Draw leaves after trees
    drawLeaves(view, proj, lightDir, lightColor, lightSpaceMatrix, shadowMapTexture, enableShadows, usePCF);
}