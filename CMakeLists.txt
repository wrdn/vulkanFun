cmake_minimum_required(VERSION 2.8.11)
project(vulkanFun)
find_package(Vulkan)
find_package(glfw3 REQUIRED)
set(CMAKE_CXX_STANDARD 17)
add_executable(vulkanFun external/spirv_cross/spirv_cross.cpp
						 external/spirv_cross/spirv_cross_util.cpp
						 external/spirv_cross/spirv_cpp.cpp
						 external/spirv_cross/spirv_cfg.cpp
						 external/spirv_cross/spirv_glsl.cpp
						 vulkanFun/main.cpp
						 vulkanFun/vk_renderer.cpp)
target_include_directories(vulkanFun PRIVATE Vulkan::Vulkan)
target_include_directories(vulkanFun PRIVATE "external")
target_link_libraries(vulkanFun Vulkan::Vulkan glfw)
