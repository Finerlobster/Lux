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

        //Buffers API
        virtual BufferHandle CreateVertexBuffer(const void* data, usize size) = 0;
        virtual BufferHandle CreateIndexBuffer(const void* data, usize size) = 0;
        virtual void DestroyBuffer(BufferHandle handle) = 0;
    
        //Texture API
        virtual TextureHandle CreateTexture(const char* path) = 0;
        virtual void DestroyTexture(TextureHandle handle) = 0;

        //Draw API
        virtual void DrawIndexed(BufferHandle vertexBuffer, BufferHandle indexBuffer, u32 indexCount, TextureHandle texture) = 0;
    
    };
}
