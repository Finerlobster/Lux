
cmake_minimum_required(VERSION 3.25)
project(Lux VERSION 0.1.0 LANGUAGES C CXX)

if(CMAKE_BUILD_TYPE STREQUAL "Debug" OR NOT CMAKE_BUILD_TYPE)
    add_compile_definitions(LX_DEBUG)
endif()

# Global settings
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Output binaries to a predictable location
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# Vulkan SDK
find_package(Vulkan REQUIRED)

# Subdirectories
add_subdirectory(ThirdParty)
add_subdirectory(Renderer)
add_subdirectory(Sandbox)

#Shader compilation
find_program(GLSLC glslc HINTS $ENV{VULKAN_SDK}/Bin)

if(NOT GLSLC)
    message(FATAL_ERROR "glslc not found. Is the Vulkan SDK installed?")
endif()

set(SHADER_SOURCE_DIR ${CMAKE_SOURCE_DIR}/Shaders)
set(SHADER_OUTPUT_DIR ${CMAKE_BINARY_DIR}/bin/Debug/Shaders)

file(MAKE_DIRECTORY ${SHADER_OUTPUT_DIR})

set(SHADERS 
    ${SHADER_SOURCE_DIR}/triangle.vert 
    ${SHADER_SOURCE_DIR}/triangle.frag
)

foreach(SHADER ${SHADERS})
    get_filename_component(SHADER_NAME ${SHADER} NAME)
    set(OUTPUT ${SHADER_OUTPUT_DIR}/${SHADER_NAME}.spv)

    add_custom_command(
        OUTPUT ${OUTPUT}
        COMMAND ${GLSLC} ${SHADER} -o ${OUTPUT}
        DEPENDS ${SHADER}
        COMMENT "Compiling shader: ${SHADER_NAME}"
    )

    list(APPEND SPIRV_BINARIES ${OUTPUT})
endforeach()

add_custom_target(Shaders ALL DEPENDS ${SPIRV_BINARIES})