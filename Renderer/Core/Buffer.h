#pragma once
#include "Types.h"
#include <volk.h>
#include <vk_mem_alloc.h>

namespace LX
{
    struct Buffer
    {
        VkBuffer buffer = VK_NULL_HANDLE;
        VmaAllocation allocation = VK_NULL_HANDLE;
        usize size = 0;
        bool inUse = false;
    };

    static constexpr u32 MAX_BUFFERS = 256;
}