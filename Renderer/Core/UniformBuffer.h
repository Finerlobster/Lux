
#pragma once

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>

namespace LX 
{
    //std140 alignment rules apply - each mat4 is 64 bytes.
    struct GlobalUBO
    {
        glm::mat4 model;
        glm::mat4 view;
        glm::mat4 projection;
    };
}