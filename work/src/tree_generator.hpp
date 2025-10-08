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

    // Leaf parameters
    GLuint leafTexture = 0;
    float leafSize = 0.4f;
    bool renderLeaves = true;
    float leafTiltAmount = 0.3f;   // Multiplier for tilt rotation (0-1)
    float leafRollAmount = 0.3f;   // Multiplier for roll rotation (0-1)

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
    void draw(const glm::mat4& view, const glm::mat4& proj,
              const glm::vec3& lightDir = glm::vec3(0, -1, -1),
              const glm::vec3& lightColor = glm::vec3(1),
              const glm::mat4& lightSpaceMatrix = glm::mat4(1.0f),
              GLuint shadowMapTexture = 0,
              bool enableShadows = false,
              bool usePCF = true);

    cgra::gl_mesh treeMesh;
    std::vector<glm::mat4> treeTransforms;

private:
    GLuint instanceVBO = 0;
    bool needsMeshRegeneration = true;

    // Leaf data
    std::vector<glm::vec3> baseLeafPositions;   // End nodes from single tree
    std::vector<glm::vec3> baseLeafDirections;  // Branch directions at end nodes
    std::vector<glm::mat4> leafTransforms;      // Transforms for all leaf instances
    cgra::gl_mesh leafMesh;
    GLuint leafInstanceVBO = 0;
    GLuint leafShader = 0;

    void setupInstancing();
    void updateInstanceBuffer();
    void regenerateTreeMesh();
    void generateLeafMesh();
    void setupLeafInstancing();
    void drawLeaves(const glm::mat4& view, const glm::mat4& proj,
                    const glm::vec3& lightDir, const glm::vec3& lightColor,
                    const glm::mat4& lightSpaceMatrix = glm::mat4(1.0f),
                    GLuint shadowMapTexture = 0,
                    bool enableShadows = false,
                    bool usePCF = true);
};
