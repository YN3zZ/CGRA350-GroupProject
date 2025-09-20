#pragma once

#include "l_system.hpp"
#include "perlin_noise.hpp"
#include <glm/glm.hpp>
#include <vector>

class TreeGenerator {
public:
    GLuint shader = 0;
    glm::vec3 color{0.4f, 0.3f, 0.1f}; // Brown color for trees
    
    // L-System for tree generation
    LSystem lSystem;
    
    // Tree placement parameters
    int treeCount = 50;
    float minHeight = 0.5f; // Minimum terrain height for tree placement
    float maxSlope = 0.8f; // Maximum slope for tree placement
    float minTreeScale = 0.5f;
    float maxTreeScale = 1.5f;
    
    // Generated trees
    std::vector<cgra::gl_mesh> trees;
    std::vector<glm::mat4> treeTransforms;
    
    // Generate trees on terrain
    void generateTreesOnTerrain(const std::vector<cgra::mesh_vertex>& terrainVertices, 
                                int meshResolution, float meshSize);
    void setTreeType(int type);

    // Draw all trees
    void draw(const glm::mat4& view, const glm::mat4& proj);
    
private:
    // Helper function to sample terrain at a position
    glm::vec3 sampleTerrainHeight(const std::vector<cgra::mesh_vertex>& vertices,
                                  int resolution, float size, glm::vec2 position);
};