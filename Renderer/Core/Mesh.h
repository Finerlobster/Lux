
#pragma once
#include "Types.h"
#include "Handle.h"

// Forward declare Vulkan handle types to avoid pulling in volk
// VkDescriptorSet is a non-dispatchable handle — pointer sized integer
#if defined(_WIN32)
    #define VK_DEFINE_NON_DISPATCHABLE_HANDLE(object) typedef uint64_t object;
#else
    #define VK_DEFINE_NON_DISPATCHABLE_HANDLE(object) typedef uint64_t object;
#endif

VK_DEFINE_NON_DISPATCHABLE_HANDLE(VkDescriptorSet)

namespace LX {
    struct MeshPrimitive {
        BufferHandle vertexBuffer;
        BufferHandle indexBuffer;
        TextureHandle texture;
        u32 indexCount = 0;
        VkDescriptorSet descriptorSets[3] = {}; //one per frame in flight
    };

    static constexpr u32 MAX_PRIMITIVES = 16;

    struct Mesh {
        MeshPrimitive primitives[MAX_PRIMITIVES] = {};
        u32 primitiveCount = 0;
        bool inUse = false;
    };

    static constexpr u32 MAX_MESHES = 256;

    struct MeshTag {};
    using MeshHandle = Handle<MeshTag>;
}