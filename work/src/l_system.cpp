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
    float currentRadius = 0.1f;

    
    for (char c : lSystemString) {
        switch (c) {
            case 'F': {
                // Draw a branch segment using cylinder
                vec3 startPos = turtle.position;
                vec3 endPos = turtle.position + turtle.direction * stepLength;
                
                addCylinder(mb, startPos, endPos, currentRadius, vertexIndex);
                
                // Move turtle to end position
                turtle.position = endPos;
                // make branches get thinner
                currentRadius *= 0.95f;
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
                currentRadius *= 0.7f; // Make branches thinner
                break;
            }
            case ']': {
                // Pop state
                if (!stateStack.empty()) {
                    turtle = stateStack.top();
                    stateStack.pop();
                    currentRadius *= 1.43f; // Restore thickness (inverse of 0.7)
                }
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
void LSystem::addCylinder(mesh_builder& mb, vec3 start, vec3 end, float radius, 
                 unsigned int& vertexIndex) {
    vec3 direction = normalize(end - start);

    // Create a simple 6-sided cylinder
    int sides = 6;
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
        vec3 v1 = start + radius * (cos(angle1) * right + sin(angle1) * up);
        vec3 v2 = start + radius * (cos(angle2) * right + sin(angle2) * up);
        
        // Top vertices
        vec3 v3 = end + radius * (cos(angle1) * right + sin(angle1) * up);
        vec3 v4 = end + radius * (cos(angle2) * right + sin(angle2) * up);
        
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
