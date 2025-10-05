#pragma once

#include "l_system.hpp"
#include "perlin_noise.hpp"
#include <glm/glm.hpp>
#include <vector>

class TreeGenerator {
public:
    // L-System parameters
    LSystem lSystem;
    int treeCount = 5;
    float minTreeScale = 0.5f;
    float maxTreeScale = 1.0f;
    bool randomRotation = true;
    float branchTaper = 0.65f;
    int cylinderSides = 12;

    // Rendering
    GLuint shader = 0;
    glm::vec3 color{0.4f, 0.3f, 0.2f}; // Brown color for trees

    // Textures
    std::vector<GLuint> barkTextures;
    bool useTextures = false;

    TreeGenerator() = default;
    ~TreeGenerator();  // Need clean up OpenGL resources

    // Load bark textures
    void loadTextures();


    void markMeshDirty() { needsMeshRegeneration = true; }
    // Generate trees on terrain
    void generateTreesOnTerrain(PerlinNoise* perlinNoise);
    void regenerateOnTerrain(PerlinNoise* perlinNoise) {
        generateTreesOnTerrain(perlinNoise);
    }
    void setTreeType(int type);
    // Draw all trees
    void draw(const glm::mat4& view, const glm::mat4& proj, const glm::vec3& lightDir = glm::vec3(0, -1, -1), const glm::vec3& lightColor = glm::vec3(1));
    
private:
    cgra::gl_mesh treeMesh;
    std::vector<glm::mat4> treeTransforms;
    GLuint instanceVBO = 0;
    bool needsMeshRegeneration = true;

    void setupInstancing();
    void updateInstanceBuffer();
    void regenerateTreeMesh();
};