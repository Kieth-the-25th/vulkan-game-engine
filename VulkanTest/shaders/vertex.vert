#version 450

layout(location = 0) in vec3 position;
layout(location = 1) in vec3 color;
layout(location = 2) in vec2 texCoord;

layout(location = 0) out vec3 fragColor;
layout(location = 1) out vec2 fragTexCoord;
layout(location = 2) out vec4 perVertexLighting;
layout(location = 3) out vec4 fragShadowCoord;

layout(set = 0, binding = 0) uniform UniformBufferObject {
    mat4 view;
    mat4 proj;
} ubo;

layout(set = 0, binding = 1) uniform LightingBufferObject {
    mat4 view;
    mat4 proj;
    vec4 lightColor;
} lbo;

struct VertexLight {
    vec4 pos;
    vec4 color;
};

layout(std430, set = 0, binding = 3) buffer VertexLights {
    vec4 count;
    VertexLight lights[];
} vertexLights;

layout(push_constant) uniform constants {
    mat4 model;
    vec4 misc;
} pushConstants;

mat4 shadowCoordBias = mat4(0.5, 0.0, 0.0, 0.0,
                            0.0, 0.5, 0.0, 0.0,
                            0.0, 0.0, 1.0, 0.0,
                            0.5, 0.5, 0.0, 1.0);

void main() {
    fragShadowCoord = shadowCoordBias * lbo.proj * lbo.view * pushConstants.model * vec4(position, 1.0);
    gl_Position = pushConstants.model * vec4(position, 1.0);
    perVertexLighting = vec4(0.0);
    for (int i = 0; i < vertexLights.count.x; i++) {
        perVertexLighting = (vertexLights.lights[i].color / (distance(gl_Position.xyz, vertexLights.lights[i].pos.xyz) + 1)) + perVertexLighting;
    }
    gl_Position = ubo.proj * ubo.view * gl_Position;
    fragColor = color;
    fragTexCoord = texCoord;
}