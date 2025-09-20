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
    
    int i = std::clamp(int(u * (resolution - 1)), 0, resolution - 1);
    int j = std::clamp(int(v * (resolution - 1)), 0, resolution - 1);
    
    int index = i * resolution + j;
    return vertices[index].pos;
}

void TreeGenerator::generateTreesOnTerrain(const vector<mesh_vertex>& terrainVertices,
                                           int meshResolution, float meshSize) {
    trees.clear();
    treeTransforms.clear();

    lSystem.cylinderSides = cylinderSides;
    lSystem.branchTaper = branchTaper;
    
    // Generate the tree mesh once
    string lSystemString = lSystem.generateString();
    gl_mesh treeMesh = lSystem.generateTreeMesh(lSystemString);
    
    // Random placement
    mt19937 rng(42);  // Fixed seed for reproducibility
    uniform_real_distribution<float> positionDist(-meshSize * 0.8f, meshSize * 0.8f);
    uniform_real_distribution<float> scaleDist(minTreeScale, maxTreeScale);
    uniform_real_distribution<float> rotationDist(0.0f, 2.0f * pi<float>());
    
    for (int i = 0; i < treeCount; i++) {
        vec2 position(positionDist(rng), positionDist(rng));
        vec3 terrainPoint = sampleTerrainHeight(terrainVertices, meshResolution,
                                                meshSize, position);
        
        trees.push_back(treeMesh);
        
        float scale = scaleDist(rng);
        float rotation = randomRotation ? rotationDist(rng) : 0.0f;
        
        mat4 transform = translate(mat4(1.0f), terrainPoint) *
                        rotate(mat4(1.0f), rotation, vec3(0, 1, 0)) *
                        glm::scale(mat4(1.0f), vec3(scale));
        treeTransforms.push_back(transform);
    }
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
