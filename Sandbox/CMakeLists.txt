
add_executable(LuxSandbox
    Main.cpp
)

target_link_libraries(LuxSandbox PRIVATE 
    Lux
)

target_compile_definitions(LuxSandbox PRIVATE LX_DEBUG)

set_target_properties(LuxSandbox PROPERTIES
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/Debug"
)

add_custom_command(TARGET LuxSandbox POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_BINARY_DIR}/bin/Debug/Shaders
        $<TARGET_FILE_DIR:LuxSandbox>/Shaders
    COMMENT "Copying shaders to output directory"
)