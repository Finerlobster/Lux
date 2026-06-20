
#pragma once
#include "Types.h"
#include "Skeleton.h"

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

namespace LX
{
    // Maximum keyframes per channel
    static constexpr u32 MAX_KEYFRAMES = 512;
    static constexpr u32 MAX_CHANNELS = 256;
    static constexpr u32 MAX_CLIP_NAME = 64;

    enum class ChannelType : u32
    {
        Translation,
        Rotation,
        Scale
    };

    // One channel animates one property of one bone
    struct AnimationChannel
    {
        u32 boneIndex = INVALID_BONE; // which bone
        ChannelType type = ChannelType::Rotation;

        // Keyframe times in seconds
        f32 times[MAX_KEYFRAMES];

        // Keyframe values
        glm::vec4 values[MAX_KEYFRAMES];

        u32 keyframeCount = 0;
    };

    struct AnimationClip
    {
        char name[MAX_CLIP_NAME] = {};
        f32 duration = 0.0f; // in seconds
        AnimationChannel channels[MAX_CHANNELS];
        u32 channelCount = 0;
    };
}