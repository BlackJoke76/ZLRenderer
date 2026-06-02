cmake_minimum_required(VERSION 3.25)

project(ZLRenderer
    VERSION 0.1.0
    DESCRIPTION "Learning Vulkan game renderer"
    LANGUAGES CXX
)

option(ZL_ENABLE_VALIDATION "Enable Vulkan validation layers" ON)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(FetchContent)
include(CTest)

find_package(Vulkan REQUIRED)

find_program(SLANGC_EXECUTABLE
    NAMES slangc
    HINTS "$ENV{VULKAN_SDK}/Bin"
    REQUIRED
)

set(ZL_SHADER_OUTPUT_DIR ${CMAKE_CURRENT_BINARY_DIR}/shaders)
set(ZL_TRIANGLE_SHADER ${PROJECT_SOURCE_DIR}/shaders/triangle.slang)
set(ZL_TRIANGLE_VERT_SPV ${ZL_SHADER_OUTPUT_DIR}/triangle.vert.spv)
set(ZL_TRIANGLE_FRAG_SPV ${ZL_SHADER_OUTPUT_DIR}/triangle.frag.spv)

add_custom_command(
    OUTPUT ${ZL_TRIANGLE_VERT_SPV}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${ZL_SHADER_OUTPUT_DIR}
    COMMAND ${SLANGC_EXECUTABLE}
        ${ZL_TRIANGLE_SHADER}
        -entry vertexMain
        -stage vertex
        -target spirv
        -profile spirv_1_5
        -o ${ZL_TRIANGLE_VERT_SPV}
    DEPENDS ${ZL_TRIANGLE_SHADER}
    VERBATIM
)

add_custom_command(
    OUTPUT ${ZL_TRIANGLE_FRAG_SPV}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${ZL_SHADER_OUTPUT_DIR}
    COMMAND ${SLANGC_EXECUTABLE}
        ${ZL_TRIANGLE_SHADER}
        -entry fragmentMain
        -stage fragment
        -target spirv
        -profile spirv_1_5
        -o ${ZL_TRIANGLE_FRAG_SPV}
    DEPENDS ${ZL_TRIANGLE_SHADER}
    VERBATIM
)

add_custom_target(zl_shaders
    DEPENDS
        ${ZL_TRIANGLE_VERT_SPV}
        ${ZL_TRIANGLE_FRAG_SPV}
)

set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
    glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG 3.4
)

FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG v1.92.8
)

FetchContent_MakeAvailable(glfw imgui)

add_library(imgui STATIC
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_demo.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_vulkan.cpp
)
target_include_directories(imgui
    PUBLIC
        ${imgui_SOURCE_DIR}
        ${imgui_SOURCE_DIR}/backends
)
target_link_libraries(imgui
    PUBLIC
        glfw
        Vulkan::Vulkan
)

add_library(zl_render_graph STATIC
    src/render_graph/RenderGraph.cpp
)

target_include_directories(zl_render_graph
    PUBLIC
        ${PROJECT_SOURCE_DIR}/include
)

add_executable(zl_renderer
    src/app/Application.cpp
    src/main.cpp
    src/rhi/vulkan/VulkanDevice.cpp
    src/rhi/vulkan/VulkanPipelineCache.cpp
)

target_include_directories(zl_renderer
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
)

target_compile_definitions(zl_renderer
    PRIVATE
        ZL_ENABLE_VALIDATION=$<BOOL:${ZL_ENABLE_VALIDATION}>
        ZL_SHADER_DIR="${ZL_SHADER_OUTPUT_DIR}"
)

target_link_libraries(zl_renderer
    PRIVATE
        zl_render_graph
        glfw
        Vulkan::Vulkan
        imgui
)

add_dependencies(zl_renderer zl_shaders)

add_executable(zl_render_graph_tests
    tests/render_graph/RenderGraphTests.cpp
)

target_link_libraries(zl_render_graph_tests
    PRIVATE
        zl_render_graph
)

add_test(NAME render_graph_tests COMMAND zl_render_graph_tests)

if(MSVC)
    target_compile_options(zl_render_graph PRIVATE /W4 /permissive- /EHsc)
    target_compile_options(zl_renderer PRIVATE /W4 /permissive- /EHsc)
    target_compile_options(zl_render_graph_tests PRIVATE /W4 /permissive- /EHsc)
    target_compile_options(imgui PRIVATE /W3 /permissive- /EHsc)
else()
    target_compile_options(zl_render_graph PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(zl_renderer PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(zl_render_graph_tests PRIVATE -Wall -Wextra -Wpedantic)
endif()
