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
    // We'll implement this in the next step
    cout << "Generated L-System string: " << lSystemString.substr(0, 50) << "..." << endl;
    return mb.build();
}