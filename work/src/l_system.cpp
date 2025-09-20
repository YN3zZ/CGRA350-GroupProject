#include "l_system.hpp"
#include "cgra/cgra_mesh.hpp"
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>

using namespace std;
using namespace glm;
using namespace cgra;

LSystem::LSystem() {
    // Default rules for a simple tree
    rules['F'] = "FF+[+F-F-F]-[-F+F+F]";
}

string LSystem::generateString() {
    string current = axiom;
    
    for (int i = 0; i < iterations; i++) {
        string next = "";
        for (char c : current) {
            if (rules.find(c) != rules.end()) {
                next += rules[c];
            } else {
                next += c;
            }
        }
        current = next;
    }
    
    return current;
}

gl_mesh LSystem::generateTreeMesh(const string& lSystemString) {
    mesh_builder mb;
    
    struct TurtleStateWithRadius {
        TurtleState turtle;
        float radius;
    };
    
    stack<TurtleStateWithRadius> stateStack;
    
    TurtleState turtle;
    turtle.position = vec3(0, 0, 0);
    turtle.direction = vec3(0, 1, 0);
    turtle.rotation = mat4(1.0f);
    
    unsigned int vertexIndex = 0;
    float currentRadius = 0.1f;
    bool nextIsNewBranch = false;
    
    for (char c : lSystemString) {
        switch (c) {
            case 'F': {
                vec3 startPos = turtle.position;
                vec3 endPos = turtle.position + turtle.direction * stepLength;
                
                // If this is the first segment after branching, add a collar
                if (nextIsNewBranch) {
                    // Add a small tapered section as transition
                    vec3 collarEnd = startPos + turtle.direction * (stepLength * 0.15f);
                    addCylinder(mb, startPos, collarEnd, currentRadius * 1.4f, currentRadius, vertexIndex);
                    startPos = collarEnd; // Start the branch from end of collar
                    nextIsNewBranch = false;
                }
                
                float endRadius = currentRadius * branchTaper;
                addCylinder(mb, startPos, endPos, currentRadius, endRadius, vertexIndex);
                
                turtle.position = endPos;
                currentRadius = endRadius;
                break;
            }
            case '+': {
                // Rotate around Z axis (positive)
                mat4 rotation = rotate(mat4(1.0f), radians(angle), vec3(0, 0, 1));
                turtle.rotation = rotation * turtle.rotation;
                turtle.direction = vec3(rotation * vec4(turtle.direction, 0));
                break;
            }
            case '-': {
                // Rotate around Z axis (negative)
                mat4 rotation = rotate(mat4(1.0f), radians(-angle), vec3(0, 0, 1));
                turtle.rotation = rotation * turtle.rotation;
                turtle.direction = vec3(rotation * vec4(turtle.direction, 0));
                break;
            }
            case '[': {
                TurtleStateWithRadius state;
                state.turtle = turtle;
                state.radius = currentRadius;
                stateStack.push(state);
                
                currentRadius *= 0.7f; // Make branches thinner
                nextIsNewBranch = true;
                break;
            }
            case ']': {
                if (!stateStack.empty()) {
                    TurtleStateWithRadius state = stateStack.top();
                    stateStack.pop();
                    turtle = state.turtle;
                    currentRadius = state.radius;
                }
                nextIsNewBranch = false;
                break;
            }
            case '&': { // Pitch down (rotate around X)
                mat4 rotation = rotate(mat4(1.0f), radians(angle), vec3(1, 0, 0));
                turtle.rotation = rotation * turtle.rotation;
                turtle.direction = vec3(rotation * vec4(turtle.direction, 0));
                break;
            }
            case '^': { // Pitch up (rotate around X)
                mat4 rotation = rotate(mat4(1.0f), radians(-angle), vec3(1, 0, 0));
                turtle.rotation = rotation * turtle.rotation;
                turtle.direction = vec3(rotation * vec4(turtle.direction, 0));
                break;
            }
            case '\\': { // Roll left (rotate around Y)
                mat4 rotation = rotate(mat4(1.0f), radians(angle), vec3(0, 1, 0));
                turtle.rotation = rotation * turtle.rotation;
                turtle.direction = vec3(rotation * vec4(turtle.direction, 0));
                break;
            }
            case '/': { // Roll right (rotate around Y)
                mat4 rotation = rotate(mat4(1.0f), radians(-angle), vec3(0, 1, 0));
                turtle.rotation = rotation * turtle.rotation;
                turtle.direction = vec3(rotation * vec4(turtle.direction, 0));
                break;
            }
        }
    }
    
    return mb.build();
}

// Add helper function to create a cylinder between two points
void LSystem::addCylinder(mesh_builder& mb, vec3 start, vec3 end, float startRadius, 
    float endRadius, unsigned int& vertexIndex) {
    vec3 direction = normalize(end - start);

    // Create a simple x-sided cylinder
    int sides = cylinderSides;
    for (int i = 0; i < sides; i++) {
        float angle1 = (2.0f * pi<float>() * i) / sides;
        float angle2 = (2.0f * pi<float>() * (i + 1)) / sides;
        
        // Calculate vertex positions
        vec3 right = normalize(cross(direction, vec3(1, 0, 0)));
        if (length(right) < 0.001f) {
            right = normalize(cross(direction, vec3(0, 0, 1)));
        }
        vec3 up = normalize(cross(direction, right));
        
        // Bottom vertices
        vec3 v1 = start + startRadius * (cos(angle1) * right + sin(angle1) * up);
        vec3 v2 = start + startRadius * (cos(angle2) * right + sin(angle2) * up);
        
        // Top vertices
        vec3 v3 = end + endRadius * (cos(angle1) * right + sin(angle1) * up);
        vec3 v4 = end + endRadius * (cos(angle2) * right + sin(angle2) * up);
        
        // Create triangles for cylinder side
        mesh_vertex mv1{v1, normalize(v1 - start), vec2(0, 0)};
        mesh_vertex mv2{v2, normalize(v2 - start), vec2(1, 0)};
        mesh_vertex mv3{v3, normalize(v3 - end), vec2(0, 1)};
        mesh_vertex mv4{v4, normalize(v4 - end), vec2(1, 1)};
        
        mb.push_vertex(mv1);
        mb.push_vertex(mv2);
        mb.push_vertex(mv3);
        mb.push_vertex(mv4);
        
        mb.push_indices({vertexIndex, vertexIndex + 1, vertexIndex + 2});
        mb.push_indices({vertexIndex + 1, vertexIndex + 3, vertexIndex + 2});
        vertexIndex += 4;
    }
}
