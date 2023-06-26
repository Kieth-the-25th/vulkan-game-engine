#version 450

layout(location = 0) in vec3 position;
layout(location = 1) in vec3 color;
layout(location = 2) in vec2 texCoord;

layout(set = 0, binding = 0) uniform UniformBufferObject {
    mat4 view;
    mat4 proj;
} ubo;

layout(push_constant) uniform constants {
    mat4 model;
    vec4 misc;
} pushConstants;

void main() {
    gl_Position = ubo.proj * ubo.view * pushConstants.model * vec4(position, 1.0);
}