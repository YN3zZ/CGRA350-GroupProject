#include "tree_generator.hpp"
#include <random>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

using namespace std;
using namespace glm;
using namespace cgra;

vec3 TreeGenerator::sampleTerrainHeight(const vector<mesh_vertex>& vertices,
                                        int resolution, float size, vec2 position) {
    // Map world position to grid indices
    float u = (position.x / size + 1.0f) * 0.5f;
    float v = (position.y / size + 1.0f) * 0.5f;
    
    int i = clamp(int(u * (resolution - 1)), 0, resolution - 1);
    int j = clamp(int(v * (resolution - 1)), 0, resolution - 1);
    
    int index = i * resolution + j;
    return vertices[index].pos;
}

void TreeGenerator::generateTreesOnTerrain(const vector<mesh_vertex>& terrainVertices,
                                           int meshResolution, float meshSize) {
    trees.clear();
    treeTransforms.clear();
    
    // Generate the tree mesh once
    string lSystemString = lSystem.generateString();
    gl_mesh treeMesh = lSystem.generateTreeMesh(lSystemString);
    
    // Random placement
    mt19937 rng(42);  // Fixed seed for reproducibility
    uniform_real_distribution<float> positionDist(-meshSize * 0.8f, meshSize * 0.8f);
    
    for (int i = 0; i < treeCount; i++) {
        vec2 position(positionDist(rng), positionDist(rng));
        vec3 terrainPoint = sampleTerrainHeight(terrainVertices, meshResolution, 
                                                meshSize, position);
        
        // Check if this is a valid position for a tree
        if (terrainPoint.y > minHeight) {
            trees.push_back(treeMesh);
            
            // Create transform matrix for this tree
            mat4 transform = translate(mat4(1.0f), terrainPoint);
            treeTransforms.push_back(transform);
        }
    }
}

void TreeGenerator::draw(const mat4& view, const mat4& proj) {
    glUseProgram(shader);
    glUniformMatrix4fv(glGetUniformLocation(shader, "uProjectionMatrix"), 1, false, value_ptr(proj));
    glUniform3fv(glGetUniformLocation(shader, "uColor"), 1, value_ptr(color));
    
    for (size_t i = 0; i < trees.size(); i++) {
        mat4 modelview = view * treeTransforms[i];
        glUniformMatrix4fv(glGetUniformLocation(shader, "uModelViewMatrix"), 1, false, value_ptr(modelview));
        trees[i].draw();
    }
}