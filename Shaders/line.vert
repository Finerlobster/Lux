#version 450

layout(set = 0, binding = 0) uniform GlobalUBO
{
    mat4 model;
    mat4 view;
    mat4 projection;
    vec4 cameraPos;
} ubo;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;

layout(location = 0) out vec3 fragColor;

void main()
{
    // Lines live in world space directly
    // model matrix is identity for debug lines
    gl_Position = ubo.projection * ubo.view * vec4(inPosition, 1.0);
    fragColor   = inColor;
}