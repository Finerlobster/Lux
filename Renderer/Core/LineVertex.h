
#pragma once
#include "Types.h"
#include <volk.h>

namespace LX
{
    struct LineVertex
    {
        f32 x, y, z;    // position
        f32 r, g, b;    // color

        static VkVertexInputBindingDescription GetBindingDescription()
        {
            VkVertexInputBindingDescription binding{};
            binding.binding   = 0;
            binding.stride    = sizeof(LineVertex);
            binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
            return binding;
        }

        static void GetAttributeDescriptions(
            VkVertexInputAttributeDescription* out,
            u32* count)
        {
            *count = 2;

            // Position
            out[0].binding  = 0;
            out[0].location = 0;
            out[0].format   = VK_FORMAT_R32G32B32_SFLOAT;
            out[0].offset   = offsetof(LineVertex, x);

            // Color
            out[1].binding  = 0;
            out[1].location = 1;
            out[1].format   = VK_FORMAT_R32G32B32_SFLOAT;
            out[1].offset   = offsetof(LineVertex, r);
        }
    };
}