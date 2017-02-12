#pragma once

#include <vulkan/vulkan.hpp>

#define GLM_FORCE_RADIANS
#include <glm/glm.hpp>

struct GLFWwindow;

struct UniformBufferObject {
    glm::mat4 model;
    glm::mat4 view;
    glm::mat4 proj;
};

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
    void                          createDescriptorSetLayout();
    void                          createGraphicsPipeline();
    void                          createFrameBuffers();
    void                          createVertexBuffer();
    void                          createIndexBuffer();
    void                          createUniformBuffer();
    void                          createDescriptorPool();
    void                          createDescriptorSet();
    void                          createCommandPool();
    void                          createCommandBuffers();
    void                          createSemaphores();

    void                          drawFrame();
    void                          updateFrame();

    void                          shutdown();
    
    static void                   printDecorations();
    static void                   printDecorations(const char* fileName);

private:
    uint32_t                      findMemoryType(uint32_t typeFilter, vk::MemoryPropertyFlags properties);
    void                          createBuffer(vk::DeviceSize size, vk::BufferUsageFlags usage, vk::MemoryPropertyFlags properties, vk::Buffer& buff, vk::DeviceMemory& buffMemory);
    void                          copyBuffer(vk::Buffer src, vk::Buffer dst, vk::DeviceSize size);

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

    vk::DescriptorSetLayout       m_descriptorLayout;
    vk::DescriptorPool            m_descriptorPool;
    vk::DescriptorSet             m_descriptorSet;

    vk::Pipeline                  m_gfxPipeline;
    vk::PipelineLayout            m_gfxPipelineLayout;
    vk::PipelineCache             m_pipelineCache;

    // test shaders
    vk::ShaderModule              m_vertShader;
    vk::ShaderModule              m_fragShader;

    vk::Buffer                    m_vertexBuffer;
    vk::DeviceMemory              m_vertexBufferMemory;

    vk::Buffer                    m_indexBuffer;
    vk::DeviceMemory              m_indexBufferMemory;

    vk::Buffer                    m_uniformStagingBuffer;
    vk::DeviceMemory              m_uniformStagingBufferMemory;
    vk::Buffer                    m_uniformBuffer;
    vk::DeviceMemory              m_uniformBufferMemory;

    vk::CommandPool               m_commandPool;
    std::vector<vk::CommandBuffer> m_commandBuffers;

    vk::Semaphore                 m_imageAvailableSemaphore;
    vk::Semaphore                 m_renderFinishedSemaphore;

    VkDebugReportCallbackEXT      m_debugCallback;
    bool                          m_addStandardValidationLayer;
};