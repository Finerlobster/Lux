
#version 450

layout(set = 0, binding = 0) uniform GlobalUBO
{
    mat4 model;
    mat4 view;
    mat4 projection;
} ubo;

layout(set = 0, binding = 3) uniform SkinningUBO
{
    mat4 boneMatrices[128];
} skinning;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inUV;
layout(location = 3) in uvec4 inBoneIndices;
layout(location = 4) in vec4 inBoneWeights;

layout(location = 0) out vec3 fragNormal;
layout(location = 1) out vec2 fragUV;
layout(location = 2) out vec3 fragWorldPos;

void main()
{

    //Skinning 
    mat4 skinMatrix = 
        skinning.boneMatrices[inBoneIndices.x] * inBoneWeights.x +
        skinning.boneMatrices[inBoneIndices.y] * inBoneWeights.y +
        skinning.boneMatrices[inBoneIndices.z] * inBoneWeights.z +
        skinning.boneMatrices[inBoneIndices.w] * inBoneWeights.w;

    vec4 skinnedPosition = skinMatrix * vec4(inPosition, 1.0);
    vec4 skinnedNormal = skinMatrix * vec4(inNormal, 0.0);


    vec4 worldPos = ubo.model * skinnedPosition;
    fragWorldPos = worldPos.xyz;

    fragNormal = mat3(transpose(inverse(ubo.model))) * normalize(skinnedNormal.xyz);
    fragUV = inUV;

    gl_Position = ubo.projection
                * ubo.view 
                * worldPos;               
}