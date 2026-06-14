
# Volk

add_library(volk STATIC volk/volk.c)

target_include_directories(volk PUBLIC volk/)

target_link_libraries(volk PUBLIC Vulkan::Vulkan)

if(WIN32)
    target_compile_definitions(volk PUBLIC VK_USE_PLATFORM_WIN32_KHR)
elseif(UNIX AND NOT APPLE)
    target_compile_definitions(volk PUBLIC VK_USE_PLATFORM_XLIB_KHR)
endif()

# VMA

add_library(vma STATIC vma_impl.cpp)

target_include_directories(vma PUBLIC vma/include/)

target_link_libraries(vma PUBLIC volk)

# GLM - header only
add_library(glm INTERFACE)
target_include_directories(glm INTERFACE glm/)

# STB
add_library(stb STATIC stb/stb_impl.cpp)
target_include_directories(stb PUBLIC stb/)

# CGLTF
add_library(cgltf STATIC cgltf_impl.cpp)
target_include_directories(cgltf PUBLIC cgltf/)

# ImGui
add_library(imgui STATIC
    imgui/imgui.cpp
    imgui/imgui_draw.cpp
    imgui/imgui_tables.cpp
    imgui/imgui_widgets.cpp
    imgui/backends/imgui_impl_vulkan.cpp
)

target_include_directories(imgui PUBLIC
    imgui/
    imgui/backends/
    ${CMAKE_SOURCE_DIR}/ThirdParty/SDL3/include
)

target_link_libraries(imgui PUBLIC volk)

target_compile_definitions(imgui PUBLIC
    IMGUI_IMPL_VULKAN_NO_PROTOTYPES
    VK_NO_PROTOTYPES
)