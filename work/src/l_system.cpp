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
        size_t estimatedSize = current.length() * 3;
        string next;
        next.reserve(estimatedSize);
        
        for (char c : current) {
            auto it = rules.find(c);
            if (it != rules.end()) {
                next.append(it->second);
            } else {
                next.push_back(c);
            }
        }
        current = std::move(next);
    }
    return current;
}

gl_mesh LSystem::generateTreeMesh(const string& lSystemString, vector<vec3>& outEndNodes, vector<vec3>& outEndDirections) {
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
    const float MIN_RADIUS = 0.001f;
    bool nextIsNewBranch = false;

    // Track if this is an end node, meaning no forward movement after this position
    bool isEndNode = false;

    for (size_t i = 0; i < lSystemString.length(); i++) {
        char c = lSystemString[i];

        // Check if next character is 'F'
        bool nextIsForward = (i + 1 < lSystemString.length() && lSystemString[i + 1] == 'F');

        // Check if we're at a branch end (inside brackets with no more F before ])
        if (c == 'F') {
            isEndNode = true;
            for (size_t j = i + 1; j < lSystemString.length(); j++) {
                if (lSystemString[j] == 'F') {
                    isEndNode = false;
                    break;
                } else if (lSystemString[j] == ']') {
                    break;
                }
            }
        }

        switch (c) {
            case 'F': {
                vec3 startPos = turtle.position;
                vec3 endPos = turtle.position + turtle.direction * stepLength;

                if (nextIsNewBranch) {
                    // Add a small tapered section as transition
                    vec3 collarEnd = startPos + turtle.direction * (stepLength * 0.15f);
                    addCylinder(mb, startPos, collarEnd, currentRadius * 1.4f, currentRadius, vertexIndex);
                    startPos = collarEnd; // Start the branch from end of collar
                    nextIsNewBranch = false;
                }

                float endRadius = std::max(MIN_RADIUS, currentRadius * branchTaper);
                if (currentRadius >= MIN_RADIUS) {
                    addCylinder(mb, startPos, endPos, currentRadius, endRadius, vertexIndex);
                }

                turtle.position = endPos;
                currentRadius = endRadius;

                // If this is an end node, record the position and direction for leaf placement
                if (isEndNode) {
                    outEndNodes.push_back(endPos);
                    outEndDirections.push_back(normalize(turtle.direction));
                }

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
                
                currentRadius = std::max(MIN_RADIUS, currentRadius * 0.7f);
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

    int sides = cylinderSides;
    for (int i = 0; i < sides; i++) {
        float angle1 = (2.0f * pi<float>() * i) / sides;
        float angle2 = (2.0f * pi<float>() * (i + 1)) / sides;
        
        vec3 right = normalize(cross(direction, vec3(1, 0, 0)));
        if (length(right) < 0.001f) {
            right = normalize(cross(direction, vec3(0, 0, 1)));
        }
        vec3 up = normalize(cross(direction, right));
        
        // Calculate proper normals (pointing outward from cylinder axis)
        vec3 normal1 = normalize(cos(angle1) * right + sin(angle1) * up);
        vec3 normal2 = normalize(cos(angle2) * right + sin(angle2) * up);
        
        vec3 v1 = start + startRadius * normal1;
        vec3 v2 = start + startRadius * normal2;
        vec3 v3 = end + endRadius * normal1;
        vec3 v4 = end + endRadius * normal2;
        
        // Use the calculated outward-pointing normals
        mesh_vertex mv1{v1, normal1, vec2(float(i)/sides, 0)};
        mesh_vertex mv2{v2, normal2, vec2(float(i+1)/sides, 0)};
        mesh_vertex mv3{v3, normal1, vec2(float(i)/sides, 1)};
        mesh_vertex mv4{v4, normal2, vec2(float(i+1)/sides, 1)};
        
        mb.push_vertex(mv1);
        mb.push_vertex(mv2);
        mb.push_vertex(mv3);
        mb.push_vertex(mv4);
        
        mb.push_indices({vertexIndex, vertexIndex + 2, vertexIndex + 1});
        mb.push_indices({vertexIndex + 1, vertexIndex + 2, vertexIndex + 3});
        
        vertexIndex += 4;
    }
}
