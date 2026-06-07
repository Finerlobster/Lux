
#pragma once
#include "Types.h"
#include "Mesh.h"
#include "IRendererBackend.h"

namespace LX {
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
            u32* outMeshCount
        );
    };
}