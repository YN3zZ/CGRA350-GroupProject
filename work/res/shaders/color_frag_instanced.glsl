#version 330 core

uniform vec3 uColor;

in VertexData {
    vec3 position;
    vec3 normal;
    vec2 textureCoord;
} f_in;

out vec4 fb_color;

void main() {
    // Calculate lighting
    vec3 eye = normalize(-f_in.position);
    float light = abs(dot(normalize(f_in.normal), eye));
    vec3 color = mix(uColor / 4, uColor, light);
    
    fb_color = vec4(color, 1);
}