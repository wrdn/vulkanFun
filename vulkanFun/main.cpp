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
    r.createFrameBuffers();
    r.createCommandPool();
    r.createCommandBuffers();

    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();
        drawFrame(r);
    }

    glfwDestroyWindow(window);

    r.shutdown();

    return 0;
}

void drawFrame(VKRenderer& r)
{
}