#include "vk_renderer.h"
#include "trace.h"
#include "file_helpers.h"
#include <set>
#include <chrono>
#include <GLFW/glfw3.h>

#include "vertex.h"

static bool ADD_VALIDATION_LAYERS = true;
static const char* STANDARD_VALIDATION_LAYER_NAME = "VK_LAYER_LUNARG_standard_validation";

static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
    VkDebugReportFlagsEXT flags,
    VkDebugReportObjectTypeEXT objType,
    uint64_t obj,
    size_t location,
    int32_t code,
    const char* layerPrefix,
    const char* msg,
    void* userData) {

    TRACE("validation layer %s: %s", layerPrefix, msg);
    return VK_FALSE;
};

void VKRenderer::init(GLFWwindow* window, uint32_t windowWidth, uint32_t windowHeight)
{
    m_windowExtents = vk::Extent2D(windowWidth, windowHeight);

    createInstance();
    setupDebugCallback();
    createSurface(window);
    selectPhysicalDevice();
    selectLogicalDevice();
    createSwapChain();
    createRenderPass();
    loadShaders();
    createPipelineCache();
    createGraphicsPipeline();
    createFrameBuffers();
    createCommandPool();
    createVertexBuffer();
    createIndexBuffer();
    createCommandBuffers();
    createSemaphores();
}

void VKRenderer::createInstance()
{
    auto allInstLayers = vk::enumerateInstanceLayerProperties();
    auto allInstExtensions = vk::enumerateInstanceExtensionProperties();

    TRACE("Available Instance Extensions:");
    for (auto it : allInstExtensions)
        TRACE("> %s", it.extensionName);

    TRACE("Available Instance Layers:");
    for (auto it : allInstLayers)
        TRACE("> %s", it.layerName);

    m_validationLayers.clear();

    // Add VK_LAYER_LUNARG_standard_validation if it exists in extension list
    // and validation layers are turned on
    std::vector<const char*> instLayers;
    if (ADD_VALIDATION_LAYERS)
    {
        auto validationLayer = std::find_if(allInstLayers.begin(), allInstLayers.end(), [](const vk::LayerProperties& e) {
            return strcmp(e.layerName, STANDARD_VALIDATION_LAYER_NAME) == 0;
        });

        if (validationLayer != allInstLayers.end())
        {
            instLayers.push_back(validationLayer->layerName);
            m_validationLayers.push_back(validationLayer->layerName);
        }
    }

    // Populate required extensions vector then find and add debug report extension
    uint32_t reqExtCount = 0;
    const char** reqExt = glfwGetRequiredInstanceExtensions(&reqExtCount);
    std::vector<const char*> instExtensions;
    for (uint32_t i = 0; i < reqExtCount; ++i)
        instExtensions.push_back(reqExt[i]);

    // find debug report extension
    if (ADD_VALIDATION_LAYERS)
    {
        auto debugReportExtension = std::find_if(allInstExtensions.begin(), allInstExtensions.end(), [](const vk::ExtensionProperties& e) {
            return strcmp(e.extensionName, VK_EXT_DEBUG_REPORT_EXTENSION_NAME) == 0;
        });

        if (debugReportExtension != allInstExtensions.end())
            instExtensions.push_back(debugReportExtension->extensionName);
    }

    // setup instance creation info
    vk::ApplicationInfo appInfo("vkTest", 1, "vkTest", 1, VK_API_VERSION_1_0);
    vk::InstanceCreateInfo instCreateInfo(vk::InstanceCreateFlags(), &appInfo);
    instCreateInfo.enabledExtensionCount = (uint32_t)instExtensions.size();
    instCreateInfo.ppEnabledExtensionNames = instExtensions.data();
    if (instLayers.empty() == false) {
        instCreateInfo.ppEnabledLayerNames = instLayers.data();
        instCreateInfo.enabledLayerCount = (uint32_t)instLayers.size();
    }

    // create instance
    m_inst = vk::createInstance(instCreateInfo);
}

void VKRenderer::setupDebugCallback()
{
    vk::DebugReportCallbackCreateInfoEXT debugReportCallbackInfo;
    debugReportCallbackInfo.flags =
        vk::DebugReportFlagBitsEXT::eInformation |
        vk::DebugReportFlagBitsEXT::eWarning |
        vk::DebugReportFlagBitsEXT::ePerformanceWarning |
        vk::DebugReportFlagBitsEXT::eError |
        vk::DebugReportFlagBitsEXT::eDebug;

    debugReportCallbackInfo.pfnCallback = debugCallback;

    auto createDebugReportFunc = (PFN_vkCreateDebugReportCallbackEXT)m_inst.getProcAddr("vkCreateDebugReportCallbackEXT");

    auto debugReportCallbackRes = createDebugReportFunc(VkInstance(m_inst), &VkDebugReportCallbackCreateInfoEXT(debugReportCallbackInfo), nullptr, &m_debugCallback);
}

void VKRenderer::createSurface(GLFWwindow* window)
{
    glfwCreateWindowSurface(VkInstance(m_inst), window, nullptr, (VkSurfaceKHR*)&m_surface);
}

void VKRenderer::selectPhysicalDevice()
{
    auto physDevices = m_inst.enumeratePhysicalDevices();
    m_physDevice = physDevices[0];

    auto props = m_physDevice.getProperties();
    auto allPhysDeviceExtensions = m_physDevice.enumerateDeviceExtensionProperties();
    auto allPhysDeviceLayers = m_physDevice.enumerateDeviceLayerProperties();

    TRACE("Selected Physical Device '%s'", props.deviceName);

    TRACE("Physical Device '%s' supported Extensions:", props.deviceName);
    for (auto it : allPhysDeviceExtensions)
        TRACE("> %s", it.extensionName);

    TRACE("Physical Device '%s' supported Layers:", props.deviceName);
    for (auto it : allPhysDeviceLayers)
        TRACE("> %s", it.layerName);
}

void VKRenderer::selectLogicalDevice()
{
    // find graphics + present queues
    auto queueFamilies = m_physDevice.getQueueFamilyProperties();

    m_gfxQueueIx = -1;
    m_presentQueueIx = -1;

    for (int i = 0; i < (int)queueFamilies.size(); ++i)
    {
        auto presentSupport = m_physDevice.getSurfaceSupportKHR(i, m_surface);
        if (queueFamilies[i].queueFlags & vk::QueueFlagBits::eGraphics)
        {
            if (m_gfxQueueIx == -1)
                m_gfxQueueIx = i;

            if (presentSupport)
            {
                m_gfxQueueIx = i;
                m_presentQueueIx = i;
                break;
            }
        }
        else if (m_presentQueueIx == -1 && presentSupport)
        {
            m_presentQueueIx = i;
        }
    }

    if (m_gfxQueueIx == -1 || m_presentQueueIx == -1)
        return;

    // make sure VK_KHR_SWAPCHAIN_EXTENSION_NAME is supported
    auto allPhysDeviceExtensions = m_physDevice.enumerateDeviceExtensionProperties();
    auto swapchainExt = std::find_if(allPhysDeviceExtensions.begin(), allPhysDeviceExtensions.end(), [](vk::ExtensionProperties& e) {
        return strcmp(e.extensionName, VK_KHR_SWAPCHAIN_EXTENSION_NAME) == 0;
    });

    if (swapchainExt == allPhysDeviceExtensions.end())
        return;

    // setup queue info for graphics + presentation queues, which might be different
    std::vector<vk::DeviceQueueCreateInfo> queueCreateInfos;
    std::set<int> uniqueQueueFamilies = { m_gfxQueueIx, m_presentQueueIx };

    float qPriority = 1.0f;
    for (auto it : uniqueQueueFamilies)
    {
        vk::DeviceQueueCreateInfo queueCreateInfo;
        queueCreateInfo.queueCount = 1;
        queueCreateInfo.queueFamilyIndex = it;
        queueCreateInfo.pQueuePriorities = &qPriority;

        queueCreateInfos.push_back(queueCreateInfo);
    }

    vk::DeviceCreateInfo deviceCreateInfo;
    deviceCreateInfo.pQueueCreateInfos = queueCreateInfos.data();
    deviceCreateInfo.queueCreateInfoCount = (uint32_t)queueCreateInfos.size();

    vk::PhysicalDeviceFeatures physDeviceFeatures;
    deviceCreateInfo.pEnabledFeatures = &physDeviceFeatures;

    // enable the swapchain extension (searched for above)
    const char* swapchainExtId[] = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };
    deviceCreateInfo.enabledExtensionCount = 1;
    deviceCreateInfo.ppEnabledExtensionNames = swapchainExtId;

    // and validation layers
    if (ADD_VALIDATION_LAYERS)
    {
        deviceCreateInfo.enabledLayerCount = (uint32_t)m_validationLayers.size();
        deviceCreateInfo.ppEnabledLayerNames = m_validationLayers.data();
    }

    // create the logical device now!
    m_dev = m_physDevice.createDevice(deviceCreateInfo);

    // cache queues for later
    m_gfxQueue = m_dev.getQueue(m_gfxQueueIx, 0);
    m_presentQueue = m_dev.getQueue(m_presentQueueIx, 0);
}

void VKRenderer::recreateSwapChain(uint32_t windowWidth, uint32_t windowHeight)
{
    m_dev.waitIdle();
    
    m_windowExtents = vk::Extent2D(windowWidth, windowHeight);

    m_dev.freeCommandBuffers(m_commandPool, m_commandBuffers);

    m_dev.destroyPipeline(m_gfxPipeline);
    m_dev.destroyPipelineLayout(m_gfxPipelineLayout);

    m_dev.destroyRenderPass(m_renderPass);

    for (auto it : m_swapChainFrameBuffers)
        m_dev.destroyFramebuffer(it);
    m_swapChainFrameBuffers.clear();

    for (auto it : m_swapChainImageViews)
        m_dev.destroyImageView(it);
    m_swapChainImageViews.clear();

    m_dev.destroySwapchainKHR(m_swapChain);

    createSwapChain();
    createRenderPass();
    createGraphicsPipeline();
    createFrameBuffers();
    createCommandBuffers();
}

void VKRenderer::createSwapChain()
{
    auto surfaceCaps = m_physDevice.getSurfaceCapabilitiesKHR(m_surface);
    auto surfaceFormats = m_physDevice.getSurfaceFormatsKHR(m_surface);
    auto surfacePresentModes = m_physDevice.getSurfacePresentModesKHR(m_surface);

    if (surfaceFormats.empty() || surfacePresentModes.empty())
        return;

    // get best surface format for swapchain
    vk::SurfaceFormatKHR selectedSurfaceFormat = surfaceFormats[0];

    if (surfaceFormats.size() == 1 &&
        selectedSurfaceFormat.format == vk::Format::eUndefined)
    {
        // surface has no preferred format, yay :)
        selectedSurfaceFormat.format = vk::Format::eB8G8R8A8Unorm;
        selectedSurfaceFormat.colorSpace = vk::ColorSpaceKHR::eSrgbNonlinear;
    }
    else
    {
        auto f = std::find_if(surfaceFormats.begin(), surfaceFormats.end(), [](const vk::SurfaceFormatKHR& fm) {
            return fm.format == vk::Format::eB8G8R8A8Unorm && fm.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear;
        });
        if (f != surfaceFormats.end())
            selectedSurfaceFormat = *f;
    }

    // get best present mode for swapchain
    vk::PresentModeKHR selectedPresentMode = vk::PresentModeKHR::eFifo;
    for (auto it : surfacePresentModes)
    {
        if (it == vk::PresentModeKHR::eMailbox)
        {
            selectedPresentMode = it;
            break;
        }
    }

    // calculate swap extent
    if (surfaceCaps.currentExtent.width != UINT32_MAX)
    {
        m_swapExtent = surfaceCaps.currentExtent;
    }
    else
    {
        m_swapExtent.width = m_windowExtents.width;
        m_swapExtent.height = m_windowExtents.height;

        m_swapExtent.width = std::max(surfaceCaps.minImageExtent.width, std::min(surfaceCaps.maxImageExtent.width, m_swapExtent.width));
        m_swapExtent.height = std::max(surfaceCaps.minImageExtent.height, std::min(surfaceCaps.maxImageExtent.height, m_swapExtent.height));
    }

    uint32_t imageCount = surfaceCaps.minImageCount + 1;
    if (surfaceCaps.maxImageCount > 0 && imageCount > surfaceCaps.maxImageCount) {
        imageCount = surfaceCaps.maxImageCount;
    }

    // time to create the swapchain, first setup the details struct
    vk::SwapchainCreateInfoKHR swapchainCreateInfo;
    swapchainCreateInfo.surface = m_surface;
    swapchainCreateInfo.minImageCount = imageCount;
    swapchainCreateInfo.imageFormat = selectedSurfaceFormat.format;
    swapchainCreateInfo.imageColorSpace = selectedSurfaceFormat.colorSpace;
    swapchainCreateInfo.imageExtent = m_swapExtent;
    swapchainCreateInfo.imageArrayLayers = 1;
    swapchainCreateInfo.imageUsage = vk::ImageUsageFlagBits::eColorAttachment;

    m_swapChainImageFormat = swapchainCreateInfo.imageFormat;

    uint32_t queueFamilyIxs[] = { (uint32_t)m_gfxQueueIx, (uint32_t)m_presentQueueIx };
    if (m_gfxQueueIx != m_presentQueueIx)
    {
        swapchainCreateInfo.imageSharingMode = vk::SharingMode::eConcurrent;
        swapchainCreateInfo.queueFamilyIndexCount = 2;
        swapchainCreateInfo.pQueueFamilyIndices = queueFamilyIxs;
    }
    else
    {
        swapchainCreateInfo.imageSharingMode = vk::SharingMode::eExclusive;
        swapchainCreateInfo.queueFamilyIndexCount = 0;
        swapchainCreateInfo.pQueueFamilyIndices = 0;
    }

    swapchainCreateInfo.preTransform = surfaceCaps.currentTransform;
    swapchainCreateInfo.compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque;
    swapchainCreateInfo.presentMode = selectedPresentMode;
    swapchainCreateInfo.clipped = VK_TRUE;

    // now create the swapchain!
    m_swapChain = m_dev.createSwapchainKHR(swapchainCreateInfo);
    m_swapChainImages = m_dev.getSwapchainImagesKHR(m_swapChain);

    // create image views so we can work with the swapchain images
    for (auto it : m_swapChainImages)
    {
        vk::ImageViewCreateInfo imageViewCreateInfo;
        imageViewCreateInfo.image = it;
        imageViewCreateInfo.format = swapchainCreateInfo.imageFormat;

        imageViewCreateInfo.components.r = vk::ComponentSwizzle::eIdentity;
        imageViewCreateInfo.components.g = vk::ComponentSwizzle::eIdentity;
        imageViewCreateInfo.components.b = vk::ComponentSwizzle::eIdentity;
        imageViewCreateInfo.components.a = vk::ComponentSwizzle::eIdentity;

        imageViewCreateInfo.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
        imageViewCreateInfo.subresourceRange.baseMipLevel = 0;
        imageViewCreateInfo.subresourceRange.levelCount = 1;
        imageViewCreateInfo.subresourceRange.baseArrayLayer = 0;
        imageViewCreateInfo.subresourceRange.layerCount = 1;

        auto imgView = m_dev.createImageView(imageViewCreateInfo);
        m_swapChainImageViews.push_back(imgView);
    }
}

void VKRenderer::createRenderPass()
{
    vk::AttachmentDescription colorAttachment;
    colorAttachment.format = m_swapChainImageFormat;
    colorAttachment.samples = vk::SampleCountFlagBits::e1;
    colorAttachment.loadOp = vk::AttachmentLoadOp::eClear;
    colorAttachment.storeOp = vk::AttachmentStoreOp::eStore;
    colorAttachment.stencilLoadOp = vk::AttachmentLoadOp::eDontCare;
    colorAttachment.stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
    colorAttachment.initialLayout = vk::ImageLayout::eUndefined;
    colorAttachment.finalLayout = vk::ImageLayout::ePresentSrcKHR;

    vk::AttachmentReference colorAttachmentRef;
    colorAttachmentRef.attachment = 0;
    colorAttachmentRef.layout = vk::ImageLayout::eColorAttachmentOptimal;

    vk::SubpassDescription subpass;
    subpass.pipelineBindPoint = vk::PipelineBindPoint::eGraphics;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorAttachmentRef;

    vk::SubpassDependency dependency;
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput;
    dependency.srcAccessMask = vk::AccessFlags();
    dependency.dstStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput;
    dependency.dstAccessMask = vk::AccessFlagBits::eColorAttachmentRead |
        vk::AccessFlagBits::eColorAttachmentWrite;

    vk::RenderPassCreateInfo renderPassInfo;
    renderPassInfo.attachmentCount = 1;
    renderPassInfo.pAttachments = &colorAttachment;
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;

    renderPassInfo.dependencyCount = 1;
    renderPassInfo.pDependencies = &dependency;

    m_renderPass = m_dev.createRenderPass(renderPassInfo);
}

void VKRenderer::loadShaders()
{
    // load shaders
    auto vertShaderSrc = file_helpers::readFile("shaders/vert.spv");
    auto fragShaderSrc = file_helpers::readFile("shaders/frag.spv");

    vk::ShaderModuleCreateInfo vertShaderCreateInfo;
    vertShaderCreateInfo.codeSize = vertShaderSrc.size();
    vertShaderCreateInfo.pCode = (uint32_t*)vertShaderSrc.data();
    m_vertShader = m_dev.createShaderModule(vertShaderCreateInfo);

    vk::ShaderModuleCreateInfo fragShaderCreateInfo;
    fragShaderCreateInfo.codeSize = fragShaderSrc.size();
    fragShaderCreateInfo.pCode = (uint32_t*)fragShaderSrc.data();
    m_fragShader = m_dev.createShaderModule(fragShaderCreateInfo);
}

void VKRenderer::createPipelineCache()
{
    vk::PipelineCacheCreateInfo cacheCreateInfo;

    auto pipelineCacheStoredData = file_helpers::readFile("pipeline_cache/cache.bin");
    //file_helpers::decrypt(pipelineCacheStoredData);

    if (pipelineCacheStoredData.empty() == false)
    {
        cacheCreateInfo.initialDataSize = pipelineCacheStoredData.size();
        cacheCreateInfo.pInitialData = (const void*)pipelineCacheStoredData.data();
    }

    m_pipelineCache = m_dev.createPipelineCache(cacheCreateInfo);
}

void VKRenderer::flushPipelineCache()
{
    auto pipelineCacheData = m_dev.getPipelineCacheData(m_pipelineCache);
    //file_helpers::encrypt(pipelineCacheData);
    file_helpers::writeFile("pipeline_cache/cache.bin", pipelineCacheData);
}

void VKRenderer::createGraphicsPipeline()
{
    vk::PipelineShaderStageCreateInfo shaderStages[2];

    vk::PipelineShaderStageCreateInfo& vertShaderStageInfo = shaderStages[0];
    vertShaderStageInfo.stage = vk::ShaderStageFlagBits::eVertex;
    vertShaderStageInfo.module = m_vertShader;
    vertShaderStageInfo.pName = "main";

    vk::PipelineShaderStageCreateInfo& fragShaderStageInfo = shaderStages[1];
    fragShaderStageInfo.stage = vk::ShaderStageFlagBits::eFragment;
    fragShaderStageInfo.module = m_fragShader;
    fragShaderStageInfo.pName = "main";


    auto bindingDesc = Vertex::getBindingDesc();
    auto attributeDesc = Vertex::getAttributeDesc();

    vk::PipelineVertexInputStateCreateInfo vertexInputInfo;
    vertexInputInfo.vertexBindingDescriptionCount = 1;
    vertexInputInfo.pVertexBindingDescriptions = &bindingDesc;
    vertexInputInfo.vertexAttributeDescriptionCount = attributeDesc.size();
    vertexInputInfo.pVertexAttributeDescriptions = attributeDesc.data();

    vk::PipelineInputAssemblyStateCreateInfo inputAssembly;
    inputAssembly.topology = vk::PrimitiveTopology::eTriangleList;
    inputAssembly.primitiveRestartEnable = VK_FALSE;

    vk::Viewport viewport;
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = (float)m_swapExtent.width;
    viewport.height = (float)m_swapExtent.height;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    vk::Rect2D scissor;
    scissor.offset = { 0, 0 };
    scissor.extent = m_swapExtent;

    vk::PipelineViewportStateCreateInfo viewportState;
    viewportState.viewportCount = 1;
    viewportState.pViewports = &viewport;
    viewportState.scissorCount = 1;
    viewportState.pScissors = &scissor;

    vk::PipelineRasterizationStateCreateInfo rasterizer;
    rasterizer.depthClampEnable = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode = vk::PolygonMode::eFill;
    rasterizer.lineWidth = 1.0f;
    rasterizer.cullMode = vk::CullModeFlagBits::eBack;
    rasterizer.frontFace = vk::FrontFace::eClockwise;
    rasterizer.depthBiasEnable = VK_FALSE;
    rasterizer.depthBiasConstantFactor = 0.0f; // Optional
    rasterizer.depthBiasClamp = 0.0f; // Optional
    rasterizer.depthBiasSlopeFactor = 0.0f; // Optional

    vk::PipelineMultisampleStateCreateInfo multisampling;
    multisampling.sampleShadingEnable = VK_FALSE;
    multisampling.rasterizationSamples = vk::SampleCountFlagBits::e1;
    multisampling.minSampleShading = 1.0f; // Optional
    multisampling.pSampleMask = nullptr; /// Optional
    multisampling.alphaToCoverageEnable = VK_FALSE; // Optional
    multisampling.alphaToOneEnable = VK_FALSE; // Optional

    vk::PipelineColorBlendAttachmentState colorBlendAttachment;
    colorBlendAttachment.colorWriteMask =
        vk::ColorComponentFlagBits::eR |
        vk::ColorComponentFlagBits::eG |
        vk::ColorComponentFlagBits::eB |
        vk::ColorComponentFlagBits::eA;
    colorBlendAttachment.blendEnable = VK_FALSE;
    colorBlendAttachment.srcColorBlendFactor = vk::BlendFactor::eOne;
    colorBlendAttachment.dstColorBlendFactor = vk::BlendFactor::eZero;
    colorBlendAttachment.colorBlendOp = vk::BlendOp::eAdd;
    colorBlendAttachment.srcAlphaBlendFactor = vk::BlendFactor::eOne;
    colorBlendAttachment.dstAlphaBlendFactor = vk::BlendFactor::eZero;
    colorBlendAttachment.alphaBlendOp = vk::BlendOp::eAdd;

    vk::PipelineColorBlendStateCreateInfo colorBlending;
    colorBlending.logicOpEnable = VK_FALSE;
    colorBlending.logicOp = vk::LogicOp::eCopy;
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments = &colorBlendAttachment;
    colorBlending.blendConstants[0] = 0.0f; // Optional
    colorBlending.blendConstants[1] = 0.0f; // Optional
    colorBlending.blendConstants[2] = 0.0f; // Optional
    colorBlending.blendConstants[3] = 0.0f; // Optional

    vk::DynamicState dynamicsStates[] = {
        vk::DynamicState::eViewport,
        vk::DynamicState::eLineWidth
    };
    vk::PipelineDynamicStateCreateInfo dynamicState;
    dynamicState.dynamicStateCount = 2;
    dynamicState.pDynamicStates = dynamicsStates;

    vk::PipelineLayoutCreateInfo pipelineLayoutInfo;
    pipelineLayoutInfo.setLayoutCount = 0;

    m_gfxPipelineLayout = m_dev.createPipelineLayout(pipelineLayoutInfo);

    vk::GraphicsPipelineCreateInfo pipelineInfo;
    pipelineInfo.stageCount = 2;
    pipelineInfo.pStages = shaderStages;
    pipelineInfo.pVertexInputState = &vertexInputInfo;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState = &multisampling;
    pipelineInfo.pDepthStencilState = nullptr; // Optional
    pipelineInfo.pColorBlendState = &colorBlending;
    pipelineInfo.pDynamicState = nullptr; // Optional
    pipelineInfo.layout = m_gfxPipelineLayout;
    pipelineInfo.renderPass = m_renderPass;
    pipelineInfo.subpass = 0;

    m_gfxPipeline = m_dev.createGraphicsPipeline(m_pipelineCache, pipelineInfo);
}

void VKRenderer::createFrameBuffers()
{
    for (auto it : m_swapChainImageViews)
    {
        vk::FramebufferCreateInfo framebufferInfo;
        framebufferInfo.renderPass = m_renderPass;
        framebufferInfo.attachmentCount = 1;
        framebufferInfo.pAttachments = &it;
        framebufferInfo.width = m_swapExtent.width;
        framebufferInfo.height = m_swapExtent.height;
        framebufferInfo.layers = 1;

        vk::Framebuffer fb = m_dev.createFramebuffer(framebufferInfo);
        m_swapChainFrameBuffers.push_back(fb);
    }
}

uint32_t VKRenderer::findMemoryType(uint32_t typeFilter, vk::MemoryPropertyFlags properties)
{
    const vk::PhysicalDeviceMemoryProperties memoryProps =
        m_physDevice.getMemoryProperties();

    for (uint32_t i = 0; i < memoryProps.memoryTypeCount;++i)
    {
        const auto& p = memoryProps.memoryTypes[i];

        if (typeFilter & (1 << i) &&
            (p.propertyFlags & properties) == properties)
        {
            return i;
        }
    }

    return 0;
}

void VKRenderer::createBuffer(vk::DeviceSize size, vk::BufferUsageFlags usage, vk::MemoryPropertyFlags properties, vk::Buffer& buff, vk::DeviceMemory& buffMemory)
{
    vk::BufferCreateInfo bufferInfo;
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = vk::SharingMode::eExclusive;

    buff = m_dev.createBuffer(bufferInfo);

    vk::MemoryRequirements memReq = m_dev.getBufferMemoryRequirements(buff);

    vk::MemoryAllocateInfo allocInfo;
    allocInfo.allocationSize = memReq.size;
    allocInfo.memoryTypeIndex = findMemoryType(memReq.memoryTypeBits, properties);

    buffMemory = m_dev.allocateMemory(allocInfo);

    m_dev.bindBufferMemory(buff, buffMemory, 0);
}

void VKRenderer::copyBuffer(vk::Buffer src, vk::Buffer dst, vk::DeviceSize size)
{
    vk::CommandBufferAllocateInfo allocInfo;
    allocInfo.level = vk::CommandBufferLevel::ePrimary;
    allocInfo.commandPool = m_commandPool;
    allocInfo.commandBufferCount = 1;

    auto commandBuffers = m_dev.allocateCommandBuffers(allocInfo);
    vk::CommandBuffer commandBuffer = commandBuffers[0];

    vk::CommandBufferBeginInfo beginInfo;
    beginInfo.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit;

    commandBuffer.begin(beginInfo);

    vk::BufferCopy copyRegion;
    copyRegion.srcOffset = 0;
    copyRegion.dstOffset = 0;
    copyRegion.size = size;
    commandBuffer.copyBuffer(src, dst, copyRegion);

    commandBuffer.end();

    vk::SubmitInfo submitInfo;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    m_gfxQueue.submit(submitInfo, vk::Fence());
    m_gfxQueue.waitIdle();

    m_dev.freeCommandBuffers(m_commandPool, commandBuffers);
}

void VKRenderer::createVertexBuffer()
{
    vk::DeviceSize buffSize = sizeof(vertices[0]) * vertices.size();
    
    // create staging buffer
    vk::Buffer stagingBuff;
    vk::DeviceMemory stagingBuffMem;

    createBuffer(
        buffSize,
        vk::BufferUsageFlagBits::eTransferSrc,
        vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
        stagingBuff,
        stagingBuffMem);

    // copy vertex data into it
    void* data = m_dev.mapMemory(stagingBuffMem, 0, buffSize);
    memcpy(data, vertices.data(), (size_t)buffSize);
    m_dev.unmapMemory(stagingBuffMem);

    // create device only vertex buffer
    createBuffer(
        buffSize,
        vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eVertexBuffer,
        vk::MemoryPropertyFlagBits::eDeviceLocal,
        m_vertexBuffer,
        m_vertexBufferMemory);

    // copy from staging to vertex buffer
    copyBuffer(stagingBuff, m_vertexBuffer, buffSize);

    // and finally destroy the staging buffer, free buffer memory
    m_dev.destroyBuffer(stagingBuff);
    m_dev.freeMemory(stagingBuffMem);
}

void VKRenderer::createIndexBuffer()
{
    vk::DeviceSize buffSize = sizeof(indices[0]) * indices.size();

    // create staging buffer
    vk::Buffer stagingBuff;
    vk::DeviceMemory stagingBuffMem;

    createBuffer(
        buffSize,
        vk::BufferUsageFlagBits::eTransferSrc,
        vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
        stagingBuff,
        stagingBuffMem);

    // copy index data into it
    void* data = m_dev.mapMemory(stagingBuffMem, 0, buffSize);
    memcpy(data, indices.data(), (size_t)buffSize);
    m_dev.unmapMemory(stagingBuffMem);

    // create device only vertex buffer
    createBuffer(
        buffSize,
        vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eIndexBuffer,
        vk::MemoryPropertyFlagBits::eDeviceLocal,
        m_indexBuffer,
        m_indexBufferMemory);

    // copy from staging to index buffer
    copyBuffer(stagingBuff, m_indexBuffer, buffSize);

    // and finally destroy the staging buffer, free buffer memory
    m_dev.destroyBuffer(stagingBuff);
    m_dev.freeMemory(stagingBuffMem);
}

void VKRenderer::createCommandPool()
{
    vk::CommandPoolCreateInfo poolInfo;
    poolInfo.queueFamilyIndex = m_gfxQueueIx;

    m_commandPool = m_dev.createCommandPool(poolInfo);
}

void VKRenderer::createCommandBuffers()
{
    vk::CommandBufferAllocateInfo allocInfo;
    allocInfo.commandPool = m_commandPool;
    allocInfo.level = vk::CommandBufferLevel::ePrimary;
    allocInfo.commandBufferCount = (uint32_t)m_swapChainFrameBuffers.size();

    m_commandBuffers = m_dev.allocateCommandBuffers(allocInfo);

    for (size_t i = 0; i < m_commandBuffers.size(); ++i)
    {
        vk::CommandBuffer& cmd = m_commandBuffers[i];

        vk::CommandBufferBeginInfo beginInfo;
        beginInfo.flags = vk::CommandBufferUsageFlagBits::eSimultaneousUse;
        
        vk::ClearValue clearColour;
        clearColour.color.float32[0] = 0.0f;
        clearColour.color.float32[1] = 0.0f;
        clearColour.color.float32[2] = 0.0f;
        clearColour.color.float32[3] = 1.0f;

        vk::RenderPassBeginInfo renderPassInfo;
        renderPassInfo.renderPass = m_renderPass;
        renderPassInfo.framebuffer = m_swapChainFrameBuffers[i];;
        renderPassInfo.renderArea.offset = { 0, 0 };
        renderPassInfo.renderArea.extent = m_swapExtent;
        renderPassInfo.clearValueCount = 1;
        renderPassInfo.pClearValues = &clearColour;
        
        cmd.begin(beginInfo);
        cmd.beginRenderPass(renderPassInfo, vk::SubpassContents::eInline);

        cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, m_gfxPipeline);

        vk::ArrayProxy<const vk::Buffer> vertexBuffers = { m_vertexBuffer };
        vk::ArrayProxy<const vk::DeviceSize> offsets = { 0 };
        cmd.bindVertexBuffers(0, vertexBuffers, offsets);

        cmd.bindIndexBuffer(m_indexBuffer, 0, vk::IndexType::eUint16);

        cmd.drawIndexed(indices.size(), 1, 0, 0, 0);

        cmd.endRenderPass();
        cmd.end();
    }
}

void VKRenderer::createSemaphores()
{
    m_imageAvailableSemaphore = m_dev.createSemaphore(vk::SemaphoreCreateInfo());
    m_renderFinishedSemaphore = m_dev.createSemaphore(vk::SemaphoreCreateInfo());
}

void VKRenderer::drawFrame()
{
    auto imageAquireRes =
        m_dev.acquireNextImageKHR(
            m_swapChain,
            std::numeric_limits<uint64_t>::max(),
            m_imageAvailableSemaphore,
            vk::Fence());

    if (imageAquireRes.result == vk::Result::eErrorOutOfDateKHR ||
        imageAquireRes.result == vk::Result::eSuboptimalKHR)
    {
        recreateSwapChain(m_windowExtents.width, m_windowExtents.height);
        return;
    }

    uint32_t imageIx = imageAquireRes.value;

    vk::PipelineStageFlags waitStages[] = {
        vk::PipelineStageFlagBits::eColorAttachmentOutput
    };

    vk::SubmitInfo submitInfo;
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = &m_imageAvailableSemaphore;
    submitInfo.pWaitDstStageMask = waitStages;

    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &m_commandBuffers[imageIx];

    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = &m_renderFinishedSemaphore;

    m_gfxQueue.submit(submitInfo, vk::Fence());

    vk::PresentInfoKHR presentInfo;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = &m_renderFinishedSemaphore;

    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = &m_swapChain;
    presentInfo.pImageIndices = &imageIx;

    m_presentQueue.presentKHR(presentInfo);
}

void VKRenderer::shutdown()
{
    m_dev.waitIdle();

    flushPipelineCache();

    m_dev.destroySemaphore(m_imageAvailableSemaphore);
    m_dev.destroySemaphore(m_renderFinishedSemaphore);

    m_dev.freeCommandBuffers(m_commandPool, m_commandBuffers);

    m_dev.destroyCommandPool(m_commandPool);

    for (auto it : m_swapChainFrameBuffers)
        m_dev.destroyFramebuffer(it);
    m_swapChainFrameBuffers.clear();

    m_dev.destroyPipeline(m_gfxPipeline);
    m_dev.destroyPipelineLayout(m_gfxPipelineLayout);
    m_dev.destroyPipelineCache(m_pipelineCache);

    m_dev.destroyBuffer(m_vertexBuffer);
    m_dev.freeMemory(m_vertexBufferMemory);

    m_dev.destroyBuffer(m_indexBuffer);
    m_dev.freeMemory(m_indexBufferMemory);

    m_dev.destroyShaderModule(m_vertShader);
    m_dev.destroyShaderModule(m_fragShader);

    m_dev.destroyRenderPass(m_renderPass);

    for (auto it : m_swapChainImageViews)
        m_dev.destroyImageView(it);
    m_swapChainImageViews.clear();

    m_dev.destroySwapchainKHR(m_swapChain);

    m_inst.destroySurfaceKHR(m_surface);

    auto destroyDebugReportFunc = (PFN_vkDestroyDebugReportCallbackEXT)m_inst.getProcAddr("vkDestroyDebugReportCallbackEXT");
    destroyDebugReportFunc(VkInstance(m_inst), m_debugCallback, nullptr);

    m_dev.destroy();
    m_inst.destroy();
}

#include <spirv_glsl.hpp>
void VKRenderer::printDecorations()
{
    printDecorations("shaders/vert.spv");
    printDecorations("shaders/frag.spv");
}

void VKRenderer::printDecorations(const char* fileName)
{
    auto spirvData = file_helpers::readFile(fileName);

    uint32_t* w = (uint32_t*)&spirvData[0];
    uint32_t len = spirvData.size() / 4;

    std::vector<uint32_t> spirvBytes(len);
    spirvBytes.assign(w, w + len);

    spirv_cross::CompilerGLSL glsl(std::move(spirvBytes));

    auto printDecorations = [&](const std::vector<spirv_cross::Resource>& v, const char* titleStr) {
        TRACE("%s:", titleStr);
        for (auto& it : v)
        {
            auto decorationMask = glsl.get_decoration_mask(it.id);
            for (uint64_t i = 0; i <= spv::DecorationAlignment; ++i) // eugh, no max enum value!
            {
                auto decorationType = (spv::Decoration)i;
                if (decorationMask & (1ull << i))
                {
                    auto decorationVal = glsl.get_decoration(it.id, decorationType);
                    TRACE("Dec %d: %s %d", decorationType, it.name.c_str(), decorationVal);
                }
            }
        }
    };


    char buff[256];
    sprintf_s(buff, sizeof(buff), "------------ %s ------------", fileName);
    std::string dashStr(strlen(buff), '-');

    TRACE("%s", dashStr.c_str());
    TRACE("%s", buff);
    TRACE("%s", dashStr.c_str());

    auto shaderResources = glsl.get_shader_resources();

    printDecorations(shaderResources.uniform_buffers, "uniform_buffers");
    printDecorations(shaderResources.storage_buffers, "storage_buffers");
    printDecorations(shaderResources.stage_inputs, "stage_inputs");
    printDecorations(shaderResources.stage_outputs, "stage_outputs");
    printDecorations(shaderResources.subpass_inputs, "subpass_inputs");
    printDecorations(shaderResources.storage_images, "storage_images");
    printDecorations(shaderResources.sampled_images, "sampled_images");
    printDecorations(shaderResources.atomic_counters, "atomic_counters");
    printDecorations(shaderResources.push_constant_buffers, "push_constant_buffers");
    printDecorations(shaderResources.separate_images, "separate_images");
    printDecorations(shaderResources.separate_samplers, "separate_samplers");
}