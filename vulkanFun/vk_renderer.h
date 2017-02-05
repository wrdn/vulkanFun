#pragma once

#include <vulkan/vulkan.hpp>

struct GLFWwindow;

class VKRenderer
{
public:
    void                          init(GLFWwindow* window, uint32_t windowWidth, uint32_t windowHeight);

    void                          createInstance();
    void                          setupDebugCallback();
    void                          createSurface(GLFWwindow* window);
    void                          selectPhysicalDevice();
    void                          selectLogicalDevice();
    void                          recreateSwapChain(uint32_t windowWidth, uint32_t windowHeight);
    void                          createSwapChain();
    void                          createRenderPass();
    void                          loadShaders();
    void                          createPipelineCache();
    void                          flushPipelineCache();
    void                          createGraphicsPipeline();
    void                          createFrameBuffers();
    void                          createCommandPool();
    void                          createCommandBuffers();
    void                          createSemaphores();

    void                          drawFrame();

    void                          shutdown();

private:
    vk::Instance                  m_inst;

    vk::SurfaceKHR                m_surface;
    vk::PhysicalDevice            m_physDevice;
    vk::Device                    m_dev;

    int                           m_gfxQueueIx;
    vk::Queue                     m_gfxQueue;

    int                           m_presentQueueIx;
    vk::Queue                     m_presentQueue;

    vk::SwapchainKHR              m_swapChain;
    vk::Format                    m_swapChainImageFormat;
    vk::Extent2D                  m_swapExtent;
    std::vector<vk::Image>        m_swapChainImages;
    std::vector<vk::ImageView>    m_swapChainImageViews;
    std::vector<vk::Framebuffer>  m_swapChainFrameBuffers;

    vk::Extent2D                  m_windowExtents;

    vk::RenderPass                m_renderPass;

    vk::Pipeline                  m_gfxPipeline;
    vk::PipelineLayout            m_gfxPipelineLayout;
    vk::PipelineCache             m_pipelineCache;

    // test shaders
    vk::ShaderModule              m_vertShader;
    vk::ShaderModule              m_fragShader;

    vk::CommandPool               m_commandPool;
    std::vector<vk::CommandBuffer> m_commandBuffers;

    vk::Semaphore                 m_imageAvailableSemaphore;
    vk::Semaphore                 m_renderFinishedSemaphore;

    VkDebugReportCallbackEXT      m_debugCallback;
    std::vector<const char*>      m_validationLayers;
};