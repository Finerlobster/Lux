#pragma once
#include "Types.h"
#include "WindowHandle.h"
#include "Handle.h"

namespace LX {
    class IRendererBackend {
    public:
        virtual ~IRendererBackend() = default;

        virtual bool Init(const WindowHandle& window) = 0;
        virtual void Shutdown() = 0;
        virtual void BeginFrame() = 0;
        virtual void EndFrame() = 0;

        virtual BufferHandle CreateVertexBuffer(const void* data, usize size) = 0;
        virtual void DestroyBuffer(BufferHandle handle) = 0;

        virtual void DrawVertexBuffer(BufferHandle handle, u32 vertexCount) = 0;
    };
}
