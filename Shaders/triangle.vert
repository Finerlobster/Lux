#version 450

layout(push_constant) uniform PushConstants
{
    mat4 model;
} push;

layout(set = 0, binding = 0) uniform GlobalUBO
{
    mat4 view;
    mat4 projection;
    vec4 cameraPos;
} ubo;

layout(set = 0, binding = 3) uniform SkinningUBO
{
    mat4 boneMatrices[128];
} skinning;

layout(location = 0) in vec3  inPosition;
layout(location = 1) in vec3  inNormal;
layout(location = 2) in vec2  inUV;
layout(location = 3) in uvec4 inBoneIndices;
layout(location = 4) in vec4  inBoneWeights;

layout(location = 0) out vec3 fragNormal;
layout(location = 1) out vec2 fragUV;
layout(location = 2) out vec3 fragWorldPos;

void main()
{
    float totalWeight = inBoneWeights.x + inBoneWeights.y +
                        inBoneWeights.z + inBoneWeights.w;

    vec4 skinnedPosition;
    vec4 skinnedNormal;

    if (totalWeight < 0.001)
    {
        skinnedPosition = vec4(inPosition, 1.0);
        skinnedNormal   = vec4(inNormal, 0.0);
    }
    else
    {
        mat4 skinMatrix =
            skinning.boneMatrices[inBoneIndices.x] * inBoneWeights.x +
            skinning.boneMatrices[inBoneIndices.y] * inBoneWeights.y +
            skinning.boneMatrices[inBoneIndices.z] * inBoneWeights.z +
            skinning.boneMatrices[inBoneIndices.w] * inBoneWeights.w;

        skinnedPosition = skinMatrix * vec4(inPosition, 1.0);
        skinnedNormal   = skinMatrix * vec4(inNormal, 0.0);
    }

    // Use push constant model matrix — correct per draw call
    vec4 worldPos = push.model * skinnedPosition;
    fragWorldPos  = worldPos.xyz;
    fragNormal    = mat3(transpose(inverse(push.model))) *
                    normalize(skinnedNormal.xyz);
    fragUV        = inUV;

    gl_Position = ubo.projection * ubo.view * worldPos;
}