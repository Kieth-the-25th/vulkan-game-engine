#version 450

layout(set = 1, binding = 0) uniform samplerCube texSampler;

layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec3 fragTexCoord;

layout(location = 0) out vec4 outColor;

void main() {
    outColor = texture(texSampler, fragTexCoord);
}