
#pragma once
#include "Types.h"

namespace LX
{
    template<typename Tag>
    struct Handle
    {
        u32 index = UINT32_MAX;
        bool IsValid() const {return index != UINT32_MAX;}
    };

    struct BufferTag {};
    struct TextureTag {};

    using BufferHandle = Handle<BufferTag>;
    using TextureHandle = Handle<TextureTag>;
}