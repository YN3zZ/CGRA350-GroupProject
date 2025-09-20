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
    float stepLength = 1.0f;
    
    // Generate the L-System string
    std::string generateString();
    
    // Convert L-System string to 3D mesh
    cgra::gl_mesh generateTreeMesh(const std::string& lSystemString);
    
    // Constructor
    LSystem();
    
private:
    // Helper structure for turtle graphics state
    struct TurtleState {
        glm::vec3 position;
        glm::vec3 direction;
        glm::mat4 rotation;
    };
};