
#pragma once
#include "Types.h"
#include "Mesh.h"
#include "IRendererBackend.h"
#include "Skeleton.h"
#include "AnimationClip.h"

namespace LX {

    static constexpr u32 MAX_ANIMATION_CLIPS = 32;

    class MeshLoader
    {
        public:
        //Load a gltf file and upload all meshes to the GPU
        //Returns a handle of the first mesh in the file
        static MeshHandle Load(
            IRendererBackend* renderer,
            const char* gltfPath,
            Mesh* mesh,
            u32 maxMeshes,
            u32* outMeshCount,
            Skeleton* outSkeleton = nullptr,
            AnimationClip* outClips = nullptr,
            u32* outClipCount = nullptr
        );
    };
}