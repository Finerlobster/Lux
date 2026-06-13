
#pragma once
#include "Types.h"
#include <cstring>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

namespace LX
{
    static constexpr u32 MAX_BONES     = 128;
    static constexpr u32 INVALID_BONE  = UINT32_MAX;
    static constexpr u32 MAX_BONE_NAME = 64;

    struct Bone
    {
        char      name[MAX_BONE_NAME] = {};
        u32       parentIndex         = INVALID_BONE;
        glm::mat4 inverseBindMatrix   = glm::mat4(1.0f);
        glm::vec3 bindPosition        = { 0.0f, 0.0f, 0.0f };
        glm::quat bindRotation        = glm::identity<glm::quat>();
        glm::vec3 bindScale           = { 1.0f, 1.0f, 1.0f };
    };

    struct Skeleton
    {
        Bone bones[MAX_BONES] = {};
        u32  boneCount        = 0;

        u32 FindBone(const char* name) const
        {
            for (u32 i = 0; i < boneCount; i++)
                if (::strcmp(bones[i].name, name) == 0)
                    return i;
            return INVALID_BONE;
        }
    };
}