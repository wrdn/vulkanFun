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
    r.init(window);

    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();
        r.drawFrame();
    }

    glfwDestroyWindow(window);

    r.shutdown();

    return 0;
}