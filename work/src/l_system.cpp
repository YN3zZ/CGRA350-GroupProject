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
    
    // Stack for push/pop operations
    stack<TurtleState> stateStack;
    
    // Initial turtle state
    TurtleState turtle;
    turtle.position = vec3(0, 0, 0);
    turtle.direction = vec3(0, 1, 0); // Growing upward
    turtle.rotation = mat4(1.0f);
    
    unsigned int vertexIndex = 0;
    
    for (char c : lSystemString) {
        switch (c) {
            case 'F': {
                // Draw a branch segment
                vec3 startPos = turtle.position;
                vec3 endPos = turtle.position + turtle.direction * stepLength;
                
                // Create a simple cylinder for the branch
                mesh_vertex v1, v2;
                v1.pos = startPos;
                v1.norm = normalize(turtle.direction);
                v1.uv = vec2(0, 0);
                
                v2.pos = endPos;
                v2.norm = normalize(turtle.direction);
                v2.uv = vec2(1, 0);
                
                mb.push_vertex(v1);
                mb.push_vertex(v2);
                mb.push_indices({vertexIndex, vertexIndex + 1});
                vertexIndex += 2;
                
                // Move turtle to end position
                turtle.position = endPos;
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
                // Push current state
                stateStack.push(turtle);
                break;
            }
            case ']': {
                // Pop state
                if (!stateStack.empty()) {
                    turtle = stateStack.top();
                    stateStack.pop();
                }
                break;
            }
        }
    }
    
    return mb.build();
}