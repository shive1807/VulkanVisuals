
cmake_minimum_required(VERSION 3.24)

project(VulkanVisuals
    VERSION 0.1.0
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Vulkan REQUIRED)
find_package(glfw3 3.4 REQUIRED)

add_executable(VulkanVisuals
    src/main.cpp
)

target_link_libraries(VulkanVisuals
    PRIVATE
        Vulkan::Vulkan
        glfw
)

target_compile_options(VulkanVisuals
    PRIVATE
        -Wall
        -Wextra
        -Wpedantic
)
