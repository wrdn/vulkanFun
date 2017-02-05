#include <GLFW/glfw3.h>
#include "vk_renderer.h"

#pragma comment(linker, "/SUBSYSTEM:windows /ENTRY:mainCRTStartup")

int main()
{
    glfwInit();

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
    GLFWwindow* window = glfwCreateWindow(WIDTH, HEIGHT, "vkTest", nullptr, nullptr);

    VKRenderer r;
    r.createInstance();
    r.setupDebugCallback();
    r.createSurface(window);
    r.selectPhysicalDevice();
    r.selectLogicalDevice();
    r.createSwapChain();
    r.createRenderPass();
    r.loadShaders();
    r.createPipelineCache();
    r.createGraphicsPipeline();

    while (!glfwWindowShouldClose(window))
        glfwPollEvents();

    glfwDestroyWindow(window);

    r.shutdown();

    return 0;
}
