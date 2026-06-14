#pragma once
#include "Types.h"
#include "WindowHandle.h"
#include "Handle.h"
#include "Mesh.h"

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

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
    
        virtual void UploadBoneMatrices(const glm::mat4* matrices, u32 count) = 0;

        //Texture API
        virtual TextureHandle CreateTexture(const char* path) = 0;
        virtual void DestroyTexture(TextureHandle handle) = 0;

        //Draw API
        virtual void DrawIndexed(BufferHandle vertexBuffer, BufferHandle indexBuffer, u32 indexCount, TextureHandle texture) = 0;
        virtual void DrawPrimitive(const MeshPrimitive& primitive) = 0;

        virtual void AllocatePrimitiveDescriptors(MeshPrimitive& primitive) = 0;
    
        virtual void SetCamera(const glm::mat4& view, const glm::mat4& projection) = 0;
        virtual void SetModelTransform(const glm::mat4& model) = 0;

        #if defined(LX_DEBUG)
        virtual void DrawDebugLine(
            f32 x0, f32 y0, f32 z0, 
            f32 x1, f32 y1, f32 z1, 
            f32 r, f32 g, f32 b) = 0;
        virtual void FlushDebugLines() = 0;
        #endif
    };
}
