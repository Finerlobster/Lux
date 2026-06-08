
#pragma once

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>

namespace LX 
{
    static constexpr u32 MAX_BONES = 128;

    //std140 alignment rules apply - each mat4 is 64 bytes.
    struct GlobalUBO
    {
        glm::mat4 model;
        glm::mat4 view;
        glm::mat4 projection;
        glm::vec4 cameraPos;
    };

    struct LightUBO
    {
        glm::vec4 direction;
        glm::vec4 color; //xyz = color, w = intensity
        glm::vec4 ambient; //xyz = color, w = unused
    };

    struct SkinningUBO
    {
        glm::mat4 boneMatrices[MAX_BONES];
    };
}