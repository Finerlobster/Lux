
add_library(Lux STATIC 
    Core/Assert.h
    Core/AnimationClip.h
    Core/Buffer.h
    Core/Handle.h
    Core/IRendererBackend.h
    Core/Mesh.h
    Core/MeshLoader.h
    Core/MeshLoader.cpp
    Core/Skeleton.h
    Core/Texture.h
    Core/UniformBuffer.h
    Core/Types.h
    Core/Vertex.h
    Vulkan/VulkanBackend.h
    Vulkan/VulkanBackend.cpp
)

target_include_directories(Lux PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(Lux
    PUBLIC 
        volk
        vma
        glm
        stb
        cgltf
    PRIVATE
)

target_precompile_headers(Lux PRIVATE
    <volk.h>
    <vk_mem_alloc.h>
    <glm/glm.hpp>
)

target_compile_definitions(Lux PRIVATE LX_DEBUG)

if(MSVC)
    target_compile_options(Lux PRIVATE /W4 /WX)
else()
    target_compile_options(Lux PRIVATE -Wall -Wextra -Werror)
endif()