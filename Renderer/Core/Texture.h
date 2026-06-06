
#pragma once
#include "Types.h"
#include <volk.h>
#include <vk_mem_alloc.h>

namespace LX
{
    struct  Texture
    {
        VkImage image = VK_NULL_HANDLE;
        VmaAllocation allocation = VK_NULL_HANDLE;
        VkImageView view = VK_NULL_HANDLE;
        u32 width = 0;
        u32 height = 0;
        bool inUse = false;
    };

    static constexpr u32 MAX_TEXTURES = 256;
}