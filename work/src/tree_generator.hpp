#pragma once

#include "l_system.hpp"
#include "perlin_noise.hpp"
#include <glm/glm.hpp>
#include <vector>

class TreeGenerator {
public:
    // L-System parameters
    LSystem lSystem;
    int treeCount = 50;
    float minTreeScale = 0.5f;
    float maxTreeScale = 2.0f;
    bool randomRotation = true;
    float branchTaper = 0.8f;
    int cylinderSides = 12;

    // Rendering
    GLuint shader = 0;
    glm::vec3 color{0.4f, 0.3f, 0.2f}; // Brown color for trees

    TreeGenerator() = default;
    ~TreeGenerator();  // Need clean up OpenGL resources


    void markMeshDirty() { needsMeshRegeneration = true; }
    // Generate trees on terrain
    void generateTreesOnTerrain(const std::vector<cgra::mesh_vertex>& terrainVertices, 
        int meshResolution, float meshSize);
    void regenerateOnTerrain(const std::vector<cgra::mesh_vertex>& terrainVertices,
        int meshResolution, float meshSize) {
        generateTreesOnTerrain(terrainVertices, meshResolution, meshSize);
    }
    void setTreeType(int type);
    // Draw all trees
    void draw(const glm::mat4& view, const glm::mat4& proj);
    
private:
    cgra::gl_mesh treeMesh;
    std::vector<glm::mat4> treeTransforms;
    GLuint instanceVBO = 0;
    bool needsMeshRegeneration = true;

    glm::vec3 sampleTerrainHeight(const std::vector<cgra::mesh_vertex>& vertices,
                                  int resolution, float size, glm::vec2 position);
    void setupInstancing();
    void updateInstanceBuffer();
    void regenerateTreeMesh();
};