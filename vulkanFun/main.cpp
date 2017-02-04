#define NOMINMAX

#include "trace.h"
#include "file_helpers.h"

#include <vulkan/vulkan.hpp>
#include <GLFW/glfw3.h>
#include <set>
#include <limits>
#include <algorithm>
#include <chrono>

#pragma comment(linker, "/SUBSYSTEM:windows /ENTRY:mainCRTStartup")

static bool ADD_VALIDATION_LAYERS = true;
static const char* STANDARD_VALIDATION_LAYER_NAME = "VK_LAYER_LUNARG_standard_validation";

static const unsigned WIDTH = 640;
static const unsigned HEIGHT = 480;

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

class VKRenderer
{
public:
    void                       createInstance();
    void                       setupDebugCallback();
    void                       createSurface(GLFWwindow* window);
    void                       selectPhysicalDevice();
    void                       selectLogicalDevice();
    void                       createSwapChain();

private:
    vk::Instance               m_inst;
    
    vk::SurfaceKHR             m_surface;
    vk::PhysicalDevice         m_physDevice;
    vk::Device                 m_dev;

    int                        m_gfxQueueIx;
    int                        m_presentQueueIx;

    vk::SwapchainKHR           m_swapChain;
    std::vector<vk::Image>     m_swapChainImages;
    std::vector<vk::ImageView> m_swapChainImageViews;

    VkDebugReportCallbackEXT   m_debugCallback;
    std::vector<const char*>   m_validationLayers;
};

void VKRenderer::createInstance()
{
    auto allInstLayers = vk::enumerateInstanceLayerProperties();
    auto allInstExtensions = vk::enumerateInstanceExtensionProperties();

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
    // enumerate devices, print details and select best one (for now, just select the first entry)
    auto physDevices = m_inst.enumeratePhysicalDevices();
    
    for (auto it : physDevices)
        TRACE("Device: %s", it.getProperties().deviceName);

    m_physDevice = physDevices[0];
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
    vk::Extent2D swapExtent;
    if (surfaceCaps.currentExtent.width != UINT32_MAX)
    {
        swapExtent = surfaceCaps.currentExtent;
    }
    else
    {
        swapExtent.width = WIDTH;
        swapExtent.height = HEIGHT;

        swapExtent.width = std::max(surfaceCaps.minImageExtent.width, std::min(surfaceCaps.maxImageExtent.width, swapExtent.width));
        swapExtent.height = std::max(surfaceCaps.minImageExtent.height, std::min(surfaceCaps.maxImageExtent.height, swapExtent.height));
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
    swapchainCreateInfo.imageExtent = swapExtent;
    swapchainCreateInfo.imageArrayLayers = 1;
    swapchainCreateInfo.imageUsage = vk::ImageUsageFlagBits::eColorAttachment;

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
}

int main()
{
    glfwInit();

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
    GLFWwindow* window = glfwCreateWindow(WIDTH, HEIGHT, "vkTest", nullptr, nullptr);

    // todo: enable this once all existing code below has been converted to member functions!
    bool use_new_code = false;
    if (use_new_code)
    {
        VKRenderer r;
        r.createInstance();
        r.setupDebugCallback();
        r.createSurface(window);
        r.selectPhysicalDevice();
        r.selectLogicalDevice();
        r.createSwapChain();
    }

    // print available instance extensions and layers
    auto allInstExtensions = vk::enumerateInstanceExtensionProperties();
    auto allInstLayers = vk::enumerateInstanceLayerProperties();

    TRACE("Available Instance Extensions:");
    for (auto it : allInstExtensions)
        TRACE("> %s", it.extensionName);

    TRACE("Available Instance Layers:");
    for (auto it : allInstLayers)
        TRACE("> %s", it.layerName);

    bool ADD_VALIDATION_LAYERS = true;

    // Find and add validation layer
    std::vector<const char*> instLayers;
    std::vector<const char*> validationLayers;

    if (ADD_VALIDATION_LAYERS)
    {
        auto validationLayer = std::find_if(allInstLayers.begin(), allInstLayers.end(), [](const vk::LayerProperties& e) {
            return strcmp(e.layerName, "VK_LAYER_LUNARG_standard_validation") == 0;
        });
        if (validationLayer != allInstLayers.end())
        {
            instLayers.push_back(validationLayer->layerName);
            validationLayers.push_back(validationLayer->layerName);
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
    vk::Instance inst = vk::createInstance(instCreateInfo);

    // setup debug callback
    vk::DebugReportCallbackCreateInfoEXT debugReportCallbackInfo;
    debugReportCallbackInfo.flags =
        vk::DebugReportFlagBitsEXT::eInformation |
        vk::DebugReportFlagBitsEXT::eWarning |
        vk::DebugReportFlagBitsEXT::ePerformanceWarning |
        vk::DebugReportFlagBitsEXT::eError |
        vk::DebugReportFlagBitsEXT::eDebug;
    debugReportCallbackInfo.pfnCallback = debugCallback;
    VkDebugReportCallbackEXT callback;
    auto createDebugReportFunc = (PFN_vkCreateDebugReportCallbackEXT)inst.getProcAddr("vkCreateDebugReportCallbackEXT");
    auto debugReportCallbackRes = createDebugReportFunc(VkInstance(inst), &VkDebugReportCallbackCreateInfoEXT(debugReportCallbackInfo), nullptr, &callback);

    // create window surface
    vk::SurfaceKHR surface;
    if (glfwCreateWindowSurface(VkInstance(inst), window, nullptr, (VkSurfaceKHR*)&surface) != VK_SUCCESS)
        return 0;

    // enumerate devices, print details and select best one (for now, just select the first entry)
    auto allPhysDevices = inst.enumeratePhysicalDevices();
    if (allPhysDevices.empty())
        return 0;
    for (auto it : allPhysDevices)
    {
        auto props = it.getProperties();
        TRACE("Device: %s", props.deviceName);
    }
    auto physDevice = allPhysDevices[0];

    // find graphics + present queues
    auto queueFamilies = physDevice.getQueueFamilyProperties();
    int graphics_queue_ix = -1;
    int present_queue_ix = -1;

    for (int i = 0; i < (int)queueFamilies.size(); ++i)
    {
        auto presentSupport = physDevice.getSurfaceSupportKHR(i, surface);
        if (queueFamilies[i].queueFlags & vk::QueueFlagBits::eGraphics)
        {
            if (graphics_queue_ix == -1)
                graphics_queue_ix = i;

            if (presentSupport)
            {
                graphics_queue_ix = i;
                present_queue_ix = i;
                break;
            }
        }
        else if (present_queue_ix == -1 && presentSupport)
        {
            present_queue_ix = i;
        }
    }

    if (graphics_queue_ix == -1 || present_queue_ix == -1)
        return 0;

    TRACE("Found gfx (%d) and present (%d) queues!", graphics_queue_ix, present_queue_ix);

    // print extensions and layers supported by selected device
    auto allPhysDeviceExtensions = physDevice.enumerateDeviceExtensionProperties();
    auto allPhysDeviceLayers = physDevice.enumerateDeviceLayerProperties();

    TRACE("Physical Device '%s' supported Extensions:", physDevice.getProperties().deviceName);
    for (auto it : allPhysDeviceExtensions)
        TRACE("> %s", it.extensionName);

    TRACE("Physical Device '%s' supported Layers:", physDevice.getProperties().deviceName);
    for (auto it : allPhysDeviceLayers)
        TRACE("> %s", it.layerName);

    // make sure VK_KHR_SWAPCHAIN_EXTENSION_NAME is supported
    auto swapchainExt = std::find_if(allPhysDeviceExtensions.begin(), allPhysDeviceExtensions.end(), [](vk::ExtensionProperties& e) {
        return strcmp(e.extensionName, VK_KHR_SWAPCHAIN_EXTENSION_NAME) == 0;
    });
    if (swapchainExt == allPhysDeviceExtensions.end())
        return 0;



    // create the logical device
    vk::PhysicalDeviceFeatures physDeviceFeatures;

    // setup queue info for graphics + presentation queues,
    // which _might_ be different
    std::vector<vk::DeviceQueueCreateInfo> queueCreateInfos;
    std::set<int> uniqueQueueFamilies = { graphics_queue_ix, present_queue_ix };
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
    deviceCreateInfo.pEnabledFeatures = &physDeviceFeatures;

    // enable the swapchain extension (searched for above)
    const char* swapchainExtId[] = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };
    deviceCreateInfo.enabledExtensionCount = 1;
    deviceCreateInfo.ppEnabledExtensionNames = swapchainExtId;

    // and validation layers
    if (ADD_VALIDATION_LAYERS)
    {
        deviceCreateInfo.enabledLayerCount = (uint32_t)validationLayers.size();
        deviceCreateInfo.ppEnabledLayerNames = validationLayers.data();
    }

    // create the logical device now!
    auto dev = physDevice.createDevice(deviceCreateInfo);

    auto gfxQueue = dev.getQueue(graphics_queue_ix, 0);
    auto presentQueue = graphics_queue_ix == present_queue_ix ?
        gfxQueue :
        dev.getQueue(present_queue_ix, 0);

    auto surfaceCaps = physDevice.getSurfaceCapabilitiesKHR(surface);
    auto surfaceFormats = physDevice.getSurfaceFormatsKHR(surface);
    auto surfacePresentModes = physDevice.getSurfacePresentModesKHR(surface);

    if (surfaceFormats.empty() || surfacePresentModes.empty())
        return 0;


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
    vk::Extent2D swapExtent;
    if (surfaceCaps.currentExtent.width != UINT32_MAX)
    {
        swapExtent = surfaceCaps.currentExtent;
    }
    else
    {
        swapExtent.width = WIDTH;
        swapExtent.height = HEIGHT;

        swapExtent.width = std::max(surfaceCaps.minImageExtent.width, std::min(surfaceCaps.maxImageExtent.width, swapExtent.width));
        swapExtent.height = std::max(surfaceCaps.minImageExtent.height, std::min(surfaceCaps.maxImageExtent.height, swapExtent.height));
    }

    uint32_t imageCount = surfaceCaps.minImageCount + 1;
    if (surfaceCaps.maxImageCount > 0 && imageCount > surfaceCaps.maxImageCount) {
        imageCount = surfaceCaps.maxImageCount;
    }

    // time to create the swapchain, first setup the details struct
    vk::SwapchainCreateInfoKHR swapchainCreateInfo;
    swapchainCreateInfo.surface = surface;
    swapchainCreateInfo.minImageCount = imageCount;
    swapchainCreateInfo.imageFormat = selectedSurfaceFormat.format;
    swapchainCreateInfo.imageColorSpace = selectedSurfaceFormat.colorSpace;
    swapchainCreateInfo.imageExtent = swapExtent;
    swapchainCreateInfo.imageArrayLayers = 1;
    swapchainCreateInfo.imageUsage = vk::ImageUsageFlagBits::eColorAttachment;

    uint32_t queueFamilyIxs[] = { (uint32_t)graphics_queue_ix, (uint32_t)present_queue_ix };
    if (graphics_queue_ix != present_queue_ix)
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
    auto swapChain = dev.createSwapchainKHR(swapchainCreateInfo);
    auto swapChainImages = dev.getSwapchainImagesKHR(swapChain);

    // create image views so we can work with the swapchain images
    std::vector<vk::ImageView> swapChainImageViews;
    for (auto it : swapChainImages)
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

        auto imgView = dev.createImageView(imageViewCreateInfo);
        swapChainImageViews.push_back(imgView);
    }

    // load shaders
    auto vertShaderSrc = file_helpers::readFile("shaders/vert.spv");
    auto fragShaderSrc = file_helpers::readFile("shaders/frag.spv");

    std::vector<std::vector<char>> shaders = { vertShaderSrc, fragShaderSrc };
    std::vector<vk::ShaderModule> shaderModules;
    for (auto it : shaders)
    {
        vk::ShaderModuleCreateInfo shaderCreateInfo;
        shaderCreateInfo.codeSize = it.size();
        shaderCreateInfo.pCode = (uint32_t*)it.data();

        auto sm = dev.createShaderModule(shaderCreateInfo);
        shaderModules.push_back(sm);
    }

    std::vector<vk::ShaderStageFlagBits> shaderStages = {
        vk::ShaderStageFlagBits::eVertex ,
        vk::ShaderStageFlagBits::eFragment
    };

    std::vector<vk::PipelineShaderStageCreateInfo> shaderPipelineCreateInfo;
    for (size_t i = 0; i < shaderModules.size(); ++i)
    {
        vk::PipelineShaderStageCreateInfo shaderStageInfo;
        shaderStageInfo.stage = shaderStages[i];
        shaderStageInfo.module = shaderModules[i];
        shaderStageInfo.pName = "main";

        shaderPipelineCreateInfo.push_back(shaderStageInfo);
    }

    vk::AttachmentDescription colorAttachment;
    colorAttachment.format = swapchainCreateInfo.imageFormat;
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

    vk::RenderPassCreateInfo renderPassInfo;
    renderPassInfo.attachmentCount = 1;
    renderPassInfo.pAttachments = &colorAttachment;
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;

    auto renderPass = dev.createRenderPass(renderPassInfo);

    // https://vulkan-tutorial.com/Drawing_a_triangle/Graphics_pipeline_basics/Fixed_functions
    vk::PipelineVertexInputStateCreateInfo vertexInputInfo;
    vertexInputInfo.vertexBindingDescriptionCount = 0;
    vertexInputInfo.pVertexBindingDescriptions = nullptr; // Optional
    vertexInputInfo.vertexAttributeDescriptionCount = 0;
    vertexInputInfo.pVertexAttributeDescriptions = nullptr; // Optional

    vk::PipelineInputAssemblyStateCreateInfo inputAssembly;
    inputAssembly.topology = vk::PrimitiveTopology::eTriangleList;
    inputAssembly.primitiveRestartEnable = VK_FALSE;

    vk::Viewport viewport;
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = (float)swapExtent.width;
    viewport.height = (float)swapExtent.height;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    vk::Rect2D scissor;
    scissor.offset = { 0, 0 };
    scissor.extent = swapExtent;

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

    auto pipelineLayout = dev.createPipelineLayout(pipelineLayoutInfo);

    vk::GraphicsPipelineCreateInfo pipelineInfo;
    pipelineInfo.stageCount = 2;
    pipelineInfo.pStages = shaderPipelineCreateInfo.data();
    pipelineInfo.pVertexInputState = &vertexInputInfo;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState = &multisampling;
    pipelineInfo.pDepthStencilState = nullptr; // Optional
    pipelineInfo.pColorBlendState = &colorBlending;
    pipelineInfo.pDynamicState = nullptr; // Optional

    pipelineInfo.layout = pipelineLayout;

    pipelineInfo.renderPass = renderPass;
    pipelineInfo.subpass = 0;

    // create pipeline cache (empty for the time being)
    bool PIPELINE_CACHE_ENABLED = true;
    vk::PipelineCache pipelineCache;
    if (PIPELINE_CACHE_ENABLED)
    {
        vk::PipelineCacheCreateInfo cacheCreateInfo;

        auto pipelineCacheStoredData = file_helpers::readFile("pipeline_cache/cache.bin");
        if (pipelineCacheStoredData.empty() == false)
        {
            cacheCreateInfo.initialDataSize = pipelineCacheStoredData.size();
            cacheCreateInfo.pInitialData = (const void*)pipelineCacheStoredData.data();
        }

        pipelineCache = dev.createPipelineCache(cacheCreateInfo);
    }
    
    
    auto pipelineCreateBegin = std::chrono::high_resolution_clock::now();

    // create gfx pipeline
    auto gfxPipeline = dev.createGraphicsPipeline(pipelineCache, pipelineInfo);

    auto pipelineCreateEnd = std::chrono::high_resolution_clock::now();
    TRACE("gfx pipeline creation took: %lld ms", std::chrono::duration_cast<std::chrono::milliseconds>(pipelineCreateEnd - pipelineCreateBegin).count());

    if (PIPELINE_CACHE_ENABLED)
    {
        auto pipelineCacheData = dev.getPipelineCacheData(pipelineCache);
        file_helpers::writeFile("pipeline_cache/cache.bin", pipelineCacheData);
    }

    while (!glfwWindowShouldClose(window))
        glfwPollEvents();

    // shutdown
    dev.waitIdle();

    dev.destroyPipeline(gfxPipeline);
    dev.destroyPipelineCache(pipelineCache);

    dev.destroyRenderPass(renderPass);

    dev.destroyPipelineLayout(pipelineLayout);

    for(auto it : shaderModules)
        dev.destroyShaderModule(it);
    shaderModules.clear();

    for (auto it : swapChainImageViews)
        dev.destroyImageView(it);
    swapChainImageViews.clear();

    dev.destroySwapchainKHR(swapChain);

    inst.destroySurfaceKHR(surface);
    glfwDestroyWindow(window);

    auto destroyDebugReportFunc = (PFN_vkDestroyDebugReportCallbackEXT)inst.getProcAddr("vkDestroyDebugReportCallbackEXT");
    destroyDebugReportFunc(VkInstance(inst), callback, nullptr);

    dev.destroy();
    inst.destroy();

    return 0;
}
