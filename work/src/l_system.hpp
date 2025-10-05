#pragma once

// glm
#include <glm/glm.hpp>

// std
#include <string>
#include <vector>
#include <map>
#include <stack>

// project
#include "opengl.hpp"
#include "cgra/cgra_mesh.hpp"

class LSystem {
public:
    // L-System parameters
    std::string axiom = "F";
    std::map<char, std::string> rules;
    int iterations = 3;
    float angle = 25.0f; // degrees
    float stepLength = 0.86f;
    float branchTaper = 0.98f;
    int cylinderSides = 8;
    
    // Generate the L-System string
    std::string generateString();

    // Convert L-System string to 3D mesh and collect end node positions and directions
    cgra::gl_mesh generateTreeMesh(const std::string& lSystemString, std::vector<glm::vec3>& outEndNodes, std::vector<glm::vec3>& outEndDirections);
    
    // Constructor
    LSystem();
    
private:
    // Helper structure for turtle graphics state
    struct TurtleState {
        glm::vec3 position;
        glm::vec3 direction;
        glm::mat4 rotation;
    };

    // Helper function to add cylinder mesh
    void addCylinder(cgra::mesh_builder& mb, glm::vec3 start, glm::vec3 end, float startRadius, float endRadius, unsigned int& vertexIndex);
};
