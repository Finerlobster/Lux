
add_library(Lux STATIC 
    Core/Assert.h
    Core/IRendererBackend.h
    Core/Types.h
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
    PRIVATE
)

target_compile_definitions(Lux PRIVATE LX_DEBUG)

if(MSVC)
    target_compile_options(Lux PRIVATE /W4 /WX)
else()
    target_compile_options(Lux PRIVATE -Wall -Wextra -Werror)
endif()