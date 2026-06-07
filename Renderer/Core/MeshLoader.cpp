#include "Core/MeshLoader.h"
#include "Core/Assert.h"
#include "Core/Vertex.h"

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>

#include <cgltf.h>
#include <cstdio>
#include <cstring>

namespace LX
{
    static void BuildTexturePath(
        const char* gltfPath,
        const char* textureName,
        char*       outPath,
        u32         outSize)
    {
        const char* lastSlash = nullptr;
        for (const char* p = gltfPath; *p; p++)
            if (*p == '/' || *p == '\\')
                lastSlash = p;

        if (lastSlash)
        {
            u32 dirLen = (u32)(lastSlash - gltfPath) + 1;
            ::memcpy(outPath, gltfPath, dirLen);

            #if defined(_MSC_VER)
                ::strncpy_s(
                    outPath + dirLen,
                    outSize - dirLen,
                    textureName,
                    outSize - dirLen - 1);
            #else
                ::strncpy(outPath + dirLen, textureName, outSize - dirLen);
            #endif
        }
        else
        {
            #if defined(_MSC_VER)
                ::strncpy_s(outPath, outSize, textureName, outSize - 1);
            #else
                ::strncpy(outPath, textureName, outSize);
            #endif
        }
    }

    MeshHandle MeshLoader::Load(
        IRendererBackend* renderer,
        const char*       gltfPath,
        Mesh*             outMeshes,
        u32               maxMeshes,
        u32*              outMeshCount)
    {
        LX_ASSERT(renderer    != nullptr, "Renderer is null");
        LX_ASSERT(gltfPath    != nullptr, "Path is null");
        LX_ASSERT(outMeshes   != nullptr, "Output mesh array is null");

        *outMeshCount = 0;

        // ── Parse glTF ────────────────────────────────────────────────────
        cgltf_options options{};
        cgltf_data*   data = nullptr;

        cgltf_result result = cgltf_parse_file(&options, gltfPath, &data);
        LX_ASSERT(result == cgltf_result_success, "Failed to parse glTF file");
        if (result != cgltf_result_success)
            return MeshHandle{};

        result = cgltf_load_buffers(&options, data, gltfPath);
        LX_ASSERT(result == cgltf_result_success, "Failed to load glTF buffers");
        if (result != cgltf_result_success)
        {
            cgltf_free(data);
            return MeshHandle{};
        }

        ::printf("[Lux] Loading glTF: %s\n", gltfPath);
        ::printf("[Lux] Meshes: %zu\n", data->meshes_count);

        u32 meshIndex = 0;

        for (u32 m = 0;
             m < (u32)data->meshes_count && meshIndex < maxMeshes;
             m++)
        {
            cgltf_mesh& gltfMesh    = data->meshes[m];
            Mesh&       mesh        = outMeshes[meshIndex];
            mesh.primitiveCount     = 0;

            for (u32 p = 0;
                 p < (u32)gltfMesh.primitives_count &&
                 mesh.primitiveCount < MAX_PRIMITIVES;
                 p++)
            {
                cgltf_primitive& prim = gltfMesh.primitives[p];

                if (prim.type != cgltf_primitive_type_triangles)
                    continue;

                // ── Get vertex count ──────────────────────────────────────
                u32 vertexCount = 0;
                for (u32 a = 0; a < (u32)prim.attributes_count; a++)
                {
                    if (prim.attributes[a].type == cgltf_attribute_type_position)
                    {
                        vertexCount = (u32)prim.attributes[a].data->count;
                        break;
                    }
                }

                if (vertexCount == 0)
                    continue;

                // ── Read vertices ─────────────────────────────────────────
                Vertex* vertices = new Vertex[vertexCount]();

                for (u32 a = 0; a < (u32)prim.attributes_count; a++)
                {
                    cgltf_attribute& attr     = prim.attributes[a];
                    cgltf_accessor*  accessor = attr.data;

                    for (u32 v = 0; v < vertexCount; v++)
                    {
                        if (attr.type == cgltf_attribute_type_position)
                        {
                            f32 pos[3] = {};
                            cgltf_accessor_read_float(accessor, v, pos, 3);
                            vertices[v].x  = pos[0];
                            vertices[v].y  = pos[1];
                            vertices[v].z  = pos[2];
                        }
                        else if (attr.type == cgltf_attribute_type_normal)
                        {
                            f32 nor[3] = {};
                            cgltf_accessor_read_float(accessor, v, nor, 3);
                            vertices[v].nx = nor[0];
                            vertices[v].ny = nor[1];
                            vertices[v].nz = nor[2];
                        }
                        else if (attr.type == cgltf_attribute_type_texcoord)
                        {
                            f32 uv[2] = {};
                            cgltf_accessor_read_float(accessor, v, uv, 2);
                            vertices[v].u  = uv[0];
                            vertices[v].v  = uv[1];
                        }
                    }
                }

                // ── Read indices ──────────────────────────────────────────
                u32  indexCount = (u32)prim.indices->count;
                u32* indices    = new u32[indexCount];

                for (u32 i = 0; i < indexCount; i++)
                    indices[i] = (u32)cgltf_accessor_read_index(prim.indices, i);


                // ── Upload to GPU ─────────────────────────────────────────
                MeshPrimitive& meshPrim = mesh.primitives[mesh.primitiveCount];

                meshPrim.vertexBuffer = renderer->CreateVertexBuffer(
                    vertices, sizeof(Vertex) * vertexCount);

                meshPrim.indexBuffer = renderer->CreateIndexBuffer(
                    indices, sizeof(u32) * indexCount);

                meshPrim.indexCount = indexCount;

                delete[] vertices;
                delete[] indices;

                // ── Load texture ──────────────────────────────────────────
                if (prim.material &&
                    prim.material->pbr_metallic_roughness
                        .base_color_texture.texture)
                {
                    cgltf_texture* tex =
                        prim.material->pbr_metallic_roughness
                            .base_color_texture.texture;

                    if (tex->image && tex->image->uri)
                    {
                        char texPath[512] = {};
                        BuildTexturePath(
                            gltfPath,
                            tex->image->uri,
                            texPath,
                            512);

                        meshPrim.texture = renderer->CreateTexture(texPath);

                        if (!meshPrim.texture.IsValid())
                            ::printf("[Lux] Warning: Failed to load texture: %s\n",
                                texPath);
                    }
                }

                // ── Allocate descriptor sets for this primitive ───────────
                // Only if we have a valid texture
                // Written once here, never updated during rendering
                if (meshPrim.texture.IsValid())
                {
                    renderer->AllocatePrimitiveDescriptors(meshPrim);
                    ::printf("[Lux] Primitive loaded: %u vertices, %u indices\n",
                        vertexCount, indexCount);
                }
                else
                {
                    ::printf("[Lux] Warning: Primitive has no texture, skipping\n");
                }

                mesh.primitiveCount++;
            }

            mesh.inUse = true;
            meshIndex++;
        }

        *outMeshCount = meshIndex;
        cgltf_free(data);

        ::printf("[Lux] glTF loaded successfully - %u meshes\n", meshIndex);

        MeshHandle handle{};
        handle.index = 0;
        return handle;
    }
}