#include <GLFW/glfw3.h>
#include "vk_renderer.h"

#pragma comment(linker, "/SUBSYSTEM:windows /ENTRY:mainCRTStartup")

VKRenderer r;

static void onWindowResized(GLFWwindow* window, int width, int height)
{
    if (width <= 0 || height <= 0)
        return;

    r.recreateSwapChain((uint32_t)width, (uint32_t)height);
}

int main()
{
    const unsigned WIDTH = 640;
    const unsigned HEIGHT = 480;

    glfwInit();

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    GLFWwindow* window = glfwCreateWindow(WIDTH, HEIGHT, "vkTest", nullptr, nullptr);
    
    glfwSetWindowSizeCallback(window, onWindowResized);

    VKRenderer::printDecorations();
    r.init(window, WIDTH, HEIGHT);

    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();
        r.updateFrame();
        r.drawFrame();
    }

    glfwDestroyWindow(window);

    r.shutdown();

    return 0;
}

