#version 450

layout(set = 1, binding = 0) uniform sampler2D texSampler;
layout(set = 0, binding = 2) uniform sampler2D shadowDepthSampler;

layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec2 fragTexCoord;
layout(location = 2) in vec4 lightColor;
layout(location = 3) in vec4 fragShadowCoord;

layout(location = 0) out vec4 outColor;

void main() {
    vec4 lighting = vec4(0, 0, 0, 0);
    if (texture(shadowDepthSampler, fragShadowCoord.xy / fragShadowCoord.w).r < (fragShadowCoord.z / fragShadowCoord.w)) {
        lighting = lightColor * 5 * (fragShadowCoord.z / fragShadowCoord.w);
        //lighting = vec4(0, 0, fragShadowCoord.z * 30.0, 1);
    }
    //outColor = (vec4(texture(shadowDepthSampler, (fragShadowCoord.xy) / fragShadowCoord.w).r, 0.0, 0.0, 1.0) * 5.0) + (vec4(texture(texSampler, fragTexCoord.xy).xyz, 1.0) * 0.1);
    outColor = (lighting) + (vec4(texture(texSampler, fragTexCoord.xy).xyz, 1.0) * 0.5);
}