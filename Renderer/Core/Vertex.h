
#pragma once

#include "Types.h"
#include <volk.h>

namespace LX {

    struct Vertex {

        f32 x, y, z;
        f32 nx, ny, nz;
        f32 u, v;
        
        // Describes how frequently vertex data is fed to the shader.
        // VERTEX means move to the next vertex for each vertex.
        // INSTANCE means move to the next entry for each instance.
        // We use one binding — binding 0 — for all our vertex data.
        static VkVertexInputBindingDescription GetBindingDescription()
        {
            VkVertexInputBindingDescription binding{};
            binding.binding   = 0;
            binding.stride    = sizeof(Vertex);
            binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
            return binding;
        }

        // Describes where each attribute lives inside the vertex struct.
        // One description per shader input variable.
        static void GetAttributeDescriptions(VkVertexInputAttributeDescription* out, u32* count)
        {
            *count = 3;

            // Attribute 0 — position (vec3)
            out[0].binding  = 0;
            out[0].location = 0;
            out[0].format   = VK_FORMAT_R32G32B32_SFLOAT;
            out[0].offset   = offsetof(Vertex, x);

            // Attribute 1 — normal (vec3)
            out[1].binding  = 0; 
            out[1].location = 1;
            out[1].format   = VK_FORMAT_R32G32B32_SFLOAT;
            out[1].offset   = offsetof(Vertex, nx);

            // Attribute 2 - UV (vec2)
            out[2].binding  = 0;
            out[2].location = 2;
            out[2].format   = VK_FORMAT_R32G32_SFLOAT;
            out[2].offset   = offsetof(Vertex, u);


        }
    };
}