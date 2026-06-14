
#pragma once
#include "Types.h"
#include "Handle.h"
#include <volk.h>

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