#version 450

layout(set = 1, binding = 0) uniform sampler2D texSampler;
layout(set = 0, binding = 2) uniform sampler2D shadowDepthSampler;

layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec2 fragTexCoord;
layout(location = 2) in vec4 perVertexLighting;
layout(location = 3) in vec4 fragShadowCoord;

layout(location = 0) out vec4 outColor;

LightingBufferObject {
    mat4 view;
    mat4 proj;
    vec4 lightColor;
}

layout(set = 0, binding = 1) buffer DirectionalLights {
    vec4 count;
    LightingBufferObject[] lbos;
} lights;

void main() {
    vec4 lighting = vec4(0, 0, 0, 0);
    if (fragShadowCoord.z / fragShadowCoord.w < 1.0 && texture(shadowDepthSampler, fragShadowCoord.xy / fragShadowCoord.w).r < (fragShadowCoord.z / fragShadowCoord.w)) {
        lighting = lbo.lightColor * 1; //* (fragShadowCoord.z / fragShadowCoord.w);
        lighting = vec4(texture(shadowDepthSampler, fragShadowCoord.xy / fragShadowCoord.w).r, fragShadowCoord.w, fragShadowCoord.z, 1);
    }
    //outColor = (vec4(texture(shadowDepthSampler, (fragShadowCoord.xy) / fragShadowCoord.w).r, 0.0, 0.0, 1.0) * 5.0) + (vec4(texture(texSampler, fragTexCoord.xy).xyz, 1.0) * 0.1);
    outColor = (lighting) + perVertexLighting * 1 + (vec4(texture(texSampler, fragTexCoord.xy).xyz, 1.0) * 0.5);
}