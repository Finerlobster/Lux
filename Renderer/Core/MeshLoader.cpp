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
        u32*              outMeshCount,
        Skeleton* outSkeleton,
        AnimationClip* outClips,
        u32* outClipCount)
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
                        else if (attr.type == cgltf_attribute_type_joints)
                        {
                            cgltf_uint joints[4] = {0, 0, 0, 0};
                            cgltf_accessor_read_uint(accessor, v, joints, 4);

                            vertices[v].boneIndices[0] = (u32)joints[0];
                            vertices[v].boneIndices[1] = (u32)joints[1];
                            vertices[v].boneIndices[2] = (u32)joints[2];
                            vertices[v].boneIndices[3] = (u32)joints[3];
                        }
                        else if (attr.type == cgltf_attribute_type_weights)
                        {

                            f32 weights[4] = {1.0f, 0.0f, 0.0f, 0.0f};
                            cgltf_accessor_read_float(accessor, v, weights, 4);

                            vertices[v].boneWeights[0] = weights[0];
                            vertices[v].boneWeights[1] = weights[1];
                            vertices[v].boneWeights[2] = weights[2];
                            vertices[v].boneWeights[3] = weights[3];
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

        if (outSkeleton && data->skins_count > 0)
        {
            cgltf_skin& skin = data->skins[0]; // use first skin

            outSkeleton->boneCount = (u32)skin.joints_count;
            LX_ASSERT(outSkeleton->boneCount <= MAX_BONES,
                "Too many bones");

            ::printf("[Lux] Loading skeleton: %u bones\n",
                outSkeleton->boneCount);

            for (u32 b = 0; b < outSkeleton->boneCount; b++)
            {
                cgltf_node* joint = skin.joints[b];
                Bone&   bone  = outSkeleton->bones[b];

                // ── Name ──────────────────────────────────────────────────────
                if (joint->name)
                {
                    #if defined(_MSC_VER)
                        ::strncpy_s(bone.name, sizeof(bone.name),
                            joint->name, sizeof(bone.name) - 1);
                    #else
                        ::strncpy(bone.name, joint->name,
                            sizeof(bone.name) - 1);
                    #endif
                }

                // ── Parent index ──────────────────────────────────────────────
                bone.parentIndex = INVALID_BONE;
                if (joint->parent)
                {
                    for (u32 p = 0; p < outSkeleton->boneCount; p++)
                    {
                        if (skin.joints[p] == joint->parent)
                        {
                            bone.parentIndex = p;
                            break;
                        }
                    }
                }

                // ── Inverse bind matrix ───────────────────────────────────────
                if (skin.inverse_bind_matrices)
                {
                    cgltf_accessor_read_float(
                        skin.inverse_bind_matrices,
                        b,
                        (f32*)&bone.inverseBindMatrix,
                        16);
                }

                // ── Bind pose local transform ─────────────────────────────────
                if (joint->has_translation)
                {
                    bone.bindPosition = {
                        joint->translation[0],
                        joint->translation[1],
                        joint->translation[2]
                    };
                }

                if (joint->has_rotation)
                {
                    bone.bindRotation = glm::quat(
                        joint->rotation[3],  // w first in GLM
                        joint->rotation[0],
                        joint->rotation[1],
                        joint->rotation[2]);
                }

                if (joint->has_scale)
                {
                    bone.bindScale = {
                        joint->scale[0],
                        joint->scale[1],
                        joint->scale[2]
                    };
                }
            }

            ::printf("[Lux] Skeleton loaded successfully\n");
        }

        if (outClips && outClipCount && data->animations_count > 0)
        {
            *outClipCount = 0;
            u32 clipLimit = data->animations_count < MAX_ANIMATION_CLIPS
                        ? (u32)data->animations_count
                        : MAX_ANIMATION_CLIPS;

            for (u32 a = 0; a < clipLimit; a++)
            {
                cgltf_animation& anim = data->animations[a];
                AnimationClip&   clip = outClips[a];

                // ── Clip name ─────────────────────────────────────────────────
                if (anim.name)
                {
                    #if defined(_MSC_VER)
                        ::strncpy_s(clip.name, sizeof(clip.name),
                            anim.name, sizeof(clip.name) - 1);
                    #else
                        ::strncpy(clip.name, anim.name, sizeof(clip.name) - 1);
                    #endif
                }
                else
                {
                    // No name — generate one
                    #if defined(_MSC_VER)
                        ::sprintf_s(clip.name, sizeof(clip.name), "Clip_%u", a);
                    #else
                        ::sprintf(clip.name, "Clip_%u", a);
                    #endif
                }

                clip.duration     = 0.0f;
                clip.channelCount = 0;

                // ── Channels ──────────────────────────────────────────────────
                for (u32 c = 0;
                    c < (u32)anim.channels_count &&
                    clip.channelCount < MAX_CHANNELS;
                    c++)
                {
                    cgltf_animation_channel& gltfChannel = anim.channels[c];
                    cgltf_animation_sampler& sampler     = *gltfChannel.sampler;

                    // Find which bone this channel affects
                    if (!gltfChannel.target_node || !outSkeleton)
                        continue;

                    u32 boneIndex = INVALID_BONE;
                    if (gltfChannel.target_node->name)
                        boneIndex = outSkeleton->FindBone(
                            gltfChannel.target_node->name);

                    if (boneIndex == INVALID_BONE)
                        continue;

                    // Determine channel type
                    ChannelType channelType;
                    switch (gltfChannel.target_path)
                    {
                        case cgltf_animation_path_type_translation:
                            channelType = ChannelType::Translation;
                            break;
                        case cgltf_animation_path_type_rotation:
                            channelType = ChannelType::Rotation;
                            break;
                        case cgltf_animation_path_type_scale:
                            channelType = ChannelType::Scale;
                            break;
                        default:
                            continue; // skip weights etc.
                    }

                    AnimationChannel& channel = clip.channels[clip.channelCount];
                    channel.boneIndex         = boneIndex;
                    channel.type              = channelType;
                    channel.keyframeCount     = 0;

                    // Read keyframe times
                    u32 keyCount = (u32)sampler.input->count;
                    if (keyCount > MAX_KEYFRAMES)
                        keyCount = MAX_KEYFRAMES;

                    for (u32 k = 0; k < keyCount; k++)
                    {
                        cgltf_accessor_read_float(sampler.input, k, &channel.times[k], 1);

                        // Track clip duration
                        if (channel.times[k] > clip.duration)
                            clip.duration = channel.times[k];
                    }

                    // Read keyframe values
                    for (u32 k = 0; k < keyCount; k++)
                    {
                        f32 val[4] = {0.0f, 0.0f, 0.0f, 1.0f};

                        if (channelType == ChannelType::Rotation)
                            cgltf_accessor_read_float(sampler.output, k, val, 4);
                        else
                            cgltf_accessor_read_float(sampler.output, k, val, 3);

                        channel.values[k] = glm::vec4(val[0], val[1], val[2], val[3]);
                    }

                    channel.keyframeCount = keyCount;
                    clip.channelCount++;
                }

                (*outClipCount)++;
                ::printf("[Lux] Animation clip loaded: '%s' (%.2fs, %u channels)\n",
                    clip.name, clip.duration, clip.channelCount);
            }

            ::printf("[Lux] Loaded %u animation clips\n", *outClipCount);
        }
        *outMeshCount = meshIndex;
        cgltf_free(data);

        ::printf("[Lux] glTF loaded successfully - %u meshes\n", meshIndex);

        MeshHandle handle{};
        handle.index = 0;
        return handle;
    }
}