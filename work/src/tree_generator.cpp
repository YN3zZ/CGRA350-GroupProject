#include "tree_generator.hpp"
#include "perlin_noise.hpp"
#include "cgra/cgra_image.hpp"
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
}

void TreeGenerator::regenerateTreeMesh() {
    // Update L-system parameters
    lSystem.cylinderSides = cylinderSides;
    lSystem.branchTaper = branchTaper;
    
    // Generate the tree mesh only once
    string lSystemString = lSystem.generateString();
    treeMesh = lSystem.generateTreeMesh(lSystemString);
    
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

void TreeGenerator::generateTreesOnTerrain(PerlinNoise* perlinNoise) {
    // Clear only the transforms, not the mesh
    treeTransforms.clear();

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
    }

    // Set up instancing with the new transforms
    setupInstancing();
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

void TreeGenerator::draw(const mat4& view, const mat4& proj, const vec3& lightDir, const vec3& viewPos) {
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

    // Set lighting uniforms
    glUniform3fv(glGetUniformLocation(shader, "uLightDir"), 1, value_ptr(lightDir));
    glUniform3fv(glGetUniformLocation(shader, "uViewPos"), 1, value_ptr(viewPos));

    glUniform1i(glGetUniformLocation(shader, "uUseTextures"), useTextures ? 1 : 0);

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
}