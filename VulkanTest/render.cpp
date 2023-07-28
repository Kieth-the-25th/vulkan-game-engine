#include "render.h"
#include "util.h"

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <chrono>
#include <fstream>
#include <stdexcept>
#include <cstdint>
#include <algorithm>
#include <optional>
#include <vector>
#include <array>
#include <set>
#include <iostream>
#include <fstream>

#include "vkheaderutil.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define TINYOBJLOADER_IMPLEMENTATION
#include "tiny_obj_loader.h"

using namespace render;

static void resizeCallback(GLFWwindow* window, int w, int h) {
    auto app = reinterpret_cast<Drawer*>(glfwGetWindowUserPointer(window));
    app->framebufferResized = true;
}

inline void initWindow(Drawer* d) {
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

    d->window = glfwCreateWindow(d->WIDTH, d->HEIGHT, "Vulkan", nullptr, nullptr);
    glfwSetWindowUserPointer(d->window, d);
    glfwSetFramebufferSizeCallback(d->window, resizeCallback);
}

static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageType, const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData) {
    std::cerr << "validation layer: " << pCallbackData->pMessage << std::endl;

    return VK_FALSE;
}

inline void populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo) {
    createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    createInfo.pfnUserCallback = debugCallback;
}

inline void loadModel(const char* dir, Drawer* d, Mesh* mesh, uint16_t material) {
    tinyobj::attrib_t attributes;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;
    std::string errorMsg;
    tinyobj::LoadObj(&attributes, &shapes, &materials, &errorMsg, dir);
    std::vector<render::Vertex> vertices;
    std::vector<uint32_t> indices;
    std::vector<Submesh::SubmeshCreateInfo> submeshes;

    for (size_t i = 0; i < attributes.vertices.size() / 3; i++)
    {
        size_t I = i * 3;
        size_t J = i * 2;
        vertices.push_back({ 
            {attributes.vertices[I], attributes.vertices[I + 1], attributes.vertices[I + 2]},
            {attributes.normals[I], attributes.normals[I + 1], attributes.normals[I + 2]},
            {attributes.texcoords[J], attributes.texcoords[J + 1]} });
    }

    //TODO: better iterator?
    std::vector<uint16_t> submeshIndices;
    for (size_t i = 0; i < shapes.size(); i++)
    {
        for (size_t j = 0; j < shapes[i].mesh.indices.size(); j++)
        {
            submeshIndices.push_back(shapes[i].mesh.indices[j].vertex_index);
        }
    }

    submeshes.push_back(Submesh::SubmeshCreateInfo(submeshIndices.data(), submeshIndices.size(), material));

    *mesh = Mesh(d, vertices.data(), vertices.size(), submeshes);
}

inline void createInstance(Drawer* d) {
    if (enableValidationLayers && !checkValidationLayerSupport()) {
        throw std::runtime_error("validation layers requested, but not available!");
    }

    VkApplicationInfo info{};
    info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    info.pApplicationName = "Hello Triangle";
    info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    info.pEngineName = "No Engine";
    info.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    info.apiVersion = VK_API_VERSION_1_0;

    VkInstanceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &info;

    uint32_t glfwExtensionCount = 0;
    const char** glfwExtensions;

    glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

    uint32_t extensionCount = 0;
    vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);

    std::vector<VkExtensionProperties> extensions(extensionCount);

    vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, extensions.data());

    for (int i = 0; i < glfwExtensionCount; i++)
    {
        bool found = false;
        for (const auto& extension : extensions) {
            if (extension.extensionName == glfwExtensions[i]) { found = true; break; }
        }
        if (!found) std::runtime_error("not all required extensions are supported");
    }

    std::cout << "available extensions:\n";

    for (const auto& extension : extensions) {
        std::cout << '\t' << extension.extensionName << '\n';
    }

    createInfo.enabledExtensionCount = glfwExtensionCount;
    createInfo.ppEnabledExtensionNames = glfwExtensions;

    VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{};
    if (enableValidationLayers) {
        createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
        createInfo.ppEnabledLayerNames = validationLayers.data();

        populateDebugMessengerCreateInfo(debugCreateInfo);
        createInfo.pNext = (VkDebugUtilsMessengerCreateInfoEXT*)&debugCreateInfo;
    }
    else {
        createInfo.enabledLayerCount = 0;

        createInfo.pNext = nullptr;
    }

    if (vkCreateInstance(&createInfo, nullptr, &(d->instance)) != VK_SUCCESS) {
        std::runtime_error("fail.");
    }
}

inline bool isSuitable(VkPhysicalDevice d, VkSurfaceKHR s) {
    bool completeQueues = getQueueFamilies(d, s).isComplete();
    bool extensionSupport = checkExtensionSupport(d);
    bool swapchainSupport = false;
    if (extensionSupport) {
        SwapchainSupportDetails ssd = getSwapchainSupportDetails(d, s);
        swapchainSupport = !ssd.formats.empty() && !ssd.modes.empty();
    }

    VkPhysicalDeviceFeatures supportedFeatures;
    vkGetPhysicalDeviceFeatures(d, &supportedFeatures);
    return completeQueues && extensionSupport && swapchainSupport && supportedFeatures.samplerAnisotropy;
}

inline void pickDevice(Drawer* d) {
    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(d->instance, &deviceCount, nullptr);

    if (deviceCount == 0) {
        throw std::runtime_error("you don't have a gpu LOL!");
    }

    std::cout << "number of available devices:" << deviceCount;
    std::cout << "\n";

    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(d->instance, &deviceCount, devices.data());

    VkPhysicalDevice choice = nullptr;
    for (const auto& device : devices) {
        if (isSuitable(device, d->surface)) {
            choice = device;
            break;
        }
    }

    if (choice == VK_NULL_HANDLE) {
        throw std::runtime_error("failed to find a suitable GPU!");
    }

    d->physicalDevice = choice;
}

inline void createLogicalDevice(Drawer* d) {
    RequiredFamilyIndices indices = getQueueFamilies(d->physicalDevice, d->surface);

    std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
    std::set<uint32_t> uniqueQueueFamilies = { indices.graphicsFamily.value(), indices.presentFamily.value() };

    float queuePriority = 1.0f;
    for (uint32_t queueFamily : uniqueQueueFamilies) {
        VkDeviceQueueCreateInfo queueCreateInfo{};
        queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueCreateInfo.queueFamilyIndex = queueFamily;
        queueCreateInfo.queueCount = 1;
        queueCreateInfo.pQueuePriorities = &queuePriority;
        queueCreateInfos.push_back(queueCreateInfo);
    }

    VkPhysicalDeviceFeatures dFeatures{};
    dFeatures.samplerAnisotropy = VK_TRUE;

    VkDeviceCreateInfo dCreateInfo{};
    dCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    dCreateInfo.pQueueCreateInfos = queueCreateInfos.data();

    dCreateInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
    dCreateInfo.pEnabledFeatures = &dFeatures;

    dCreateInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
    dCreateInfo.ppEnabledExtensionNames = deviceExtensions.data();

    dCreateInfo.enabledLayerCount = 0;

    if (vkCreateDevice(d->physicalDevice, &dCreateInfo, nullptr, &(d->device)) != VK_SUCCESS) {
        throw std::runtime_error("error creating logical device");
    }

    vkGetDeviceQueue(d->device, indices.graphicsFamily.value(), 0, &(d->graphicsQueue));
    vkGetDeviceQueue(d->device, indices.presentFamily.value(), 0, &(d->presentQueue));
}


inline void createSwapchain(Drawer* d) {
    SwapchainSupportDetails details = getSwapchainSupportDetails(d->physicalDevice, d->surface);

    VkSurfaceFormatKHR surfaceFormat = chooseSwapchainFormat(details);
    VkPresentModeKHR mode = chooseSwapchainPresentMode(details);
    VkExtent2D scExtent = chooseSwapchainExtent((d->window), details.capabilities);

    uint32_t minImageCount = details.capabilities.minImageCount + 1;
    if (minImageCount > details.capabilities.maxImageCount && details.capabilities.maxImageCount > 0) minImageCount = details.capabilities.maxImageCount;

    VkSwapchainCreateInfoKHR createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    createInfo.surface = d->surface;

    createInfo.minImageCount = minImageCount;
    createInfo.imageFormat = surfaceFormat.format;
    createInfo.imageColorSpace = surfaceFormat.colorSpace;
    createInfo.imageExtent = scExtent;
    createInfo.imageArrayLayers = 1;
    createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;

    RequiredFamilyIndices indices = getQueueFamilies(d->physicalDevice, d->surface);
    uint32_t queueFamilyIndices[] = { indices.graphicsFamily.value(), indices.presentFamily.value() };

    if (indices.graphicsFamily != indices.presentFamily) {
        createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        createInfo.queueFamilyIndexCount = 2;
        createInfo.pQueueFamilyIndices = queueFamilyIndices;
    }
    else {
        createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }

    createInfo.preTransform = details.capabilities.currentTransform;
    createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    createInfo.presentMode = mode;
    createInfo.clipped = VK_TRUE;
    createInfo.oldSwapchain = VK_NULL_HANDLE;

    d->format = surfaceFormat.format;
    d->extent = scExtent;

    if (vkCreateSwapchainKHR(d->device, &createInfo, nullptr, &(d->presentSwapchain)) != VK_SUCCESS) {
        std::runtime_error("Failed to create swapchain.");
    }

    uint32_t imageCount;
    vkGetSwapchainImagesKHR(d->device, d->presentSwapchain, &imageCount, nullptr);
    d->frames.resize(imageCount);
    std::vector<VkImage> images;
    images.resize(imageCount);
    vkGetSwapchainImagesKHR(d->device, d->presentSwapchain, &imageCount, images.data());

    for (size_t i = 0; i < imageCount; i++)
    {
        d->frames[i].swapchainImage = images[i];
    }
}


inline void createImageViews(Drawer* d) {
    for (size_t i = 0; i < d->frames.size(); i++)
    {
        d->frames[i].swapchainImageView = createImageView(d->device, d->frames[i].swapchainImage, VK_IMAGE_VIEW_TYPE_2D, d->format, VK_IMAGE_ASPECT_COLOR_BIT);
    }
}

inline void createFramebuffers(Drawer* d) {
    for (uint32_t i = 0; i < d->frames.size(); i++)
    {
        std::cout << &(d->frames[i].framebuffer) << "\n";
        VkImageView attatchments[] = {
            d->frames[i].swapchainImageView,
            d->depthView
        };

        VkFramebufferCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        createInfo.renderPass = d->renderPass;
        createInfo.pAttachments = attatchments;
        createInfo.width = d->extent.width;
        createInfo.height = d->extent.height;
        createInfo.attachmentCount = 2;
        createInfo.layers = 1;

        if (vkCreateFramebuffer(d->device, &createInfo, nullptr, &(d->frames[i].framebuffer)) != VK_SUCCESS) {
            throw std::runtime_error("failed to create framebuffer!");
        }
    }
}


inline void createRenderPass(Drawer* d) {
    VkAttachmentDescription colorAttachment{};
    colorAttachment.format = d->format;
    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference depthAttachmentRef{};
    depthAttachmentRef.attachment = 1;
    depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentReference colorAttachmentRef{};
    colorAttachmentRef.attachment = 0;
    colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentDescription depthAttachment{};
    depthAttachment.format = findDepthFormat(d->physicalDevice);
    depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorAttachmentRef;
    subpass.pDepthStencilAttachment = &depthAttachmentRef;

    std::array<VkSubpassDependency, 1> dependencies;

    dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
    dependencies[0].dstSubpass = 0;
    dependencies[0].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependencies[0].srcAccessMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
    dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

    VkAttachmentDescription attachments[2] = { colorAttachment, depthAttachment };
    VkRenderPassCreateInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = 2;
    renderPassInfo.pAttachments = attachments;
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;
    renderPassInfo.dependencyCount = dependencies.size();
    renderPassInfo.pDependencies = dependencies.data();

    if (vkCreateRenderPass(d->device, &renderPassInfo, nullptr, &(d->renderPass)) != VK_SUCCESS) {
        throw std::runtime_error("Error creating render pass");
    }
}

//
inline void createDepthOnlyPass(Drawer* d) {
    VkAttachmentReference depthAttachmentRef{};
    depthAttachmentRef.attachment = 0;
    depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    std::cout << findDepthSamplerFormat(d->physicalDevice) << "\n";
    VkAttachmentDescription depthAttachment{};
    depthAttachment.format = findDepthSamplerFormat(d->physicalDevice);
    depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_STORE;
    depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    depthAttachment.finalLayout = VK_IMAGE_LAYOUT_GENERAL; //TODO: better layout?

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 0;
    subpass.pDepthStencilAttachment = &depthAttachmentRef;

    std::array<VkSubpassDependency, 2> dependencies;

    dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
    dependencies[0].dstSubpass = 0;
    dependencies[0].srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    dependencies[0].dstStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependencies[0].srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
    dependencies[0].dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
    dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

    dependencies[1].srcSubpass = 0;
    dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
    dependencies[1].srcStageMask = VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
    dependencies[1].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    dependencies[1].srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
    dependencies[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    dependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

    VkAttachmentDescription attachments[] = { depthAttachment };
    VkRenderPassCreateInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = 1;
    renderPassInfo.pAttachments = attachments;
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;
    renderPassInfo.dependencyCount = 2;
    renderPassInfo.pDependencies = dependencies.data();

    if (vkCreateRenderPass(d->device, &renderPassInfo, nullptr, &(d->shadowPass)) != VK_SUCCESS) {
        throw std::runtime_error("Error creating render pass");
    }
}

inline void createDescriptorPool(Drawer* d) {
    std::vector<VkDescriptorPoolSize> poolSizes{};
    poolSizes.resize(3);
    poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSizes[0].descriptorCount = static_cast<uint32_t>(10);
    poolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSizes[1].descriptorCount = static_cast<uint32_t>(10);
    poolSizes[2].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    poolSizes[2].descriptorCount = static_cast<uint32_t>(10);

    VkDescriptorPoolCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    info.maxSets = 30;
    info.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
    info.pPoolSizes = poolSizes.data();

    if (vkCreateDescriptorPool(d->device, &info, nullptr, &(d->descPool)) != VK_SUCCESS) {
        std::runtime_error("Error creating descriptor pool");
    }
}

inline void createGraphicsPipeline(Drawer* d, std::vector<char> shaders[], Material* mat) {
    VkShaderModule vShader = createShaderModule(d->device, shaders[0]);
    VkShaderModule fShader = createShaderModule(d->device, shaders[1]);

    VkPipelineShaderStageCreateInfo vShaderStageInfo{};
    vShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vShaderStageInfo.module = vShader;
    vShaderStageInfo.pName = "main";

    VkPipelineShaderStageCreateInfo fShaderStageInfo{};
    fShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    fShaderStageInfo.module = fShader;
    fShaderStageInfo.pName = "main";

    VkPipelineShaderStageCreateInfo shaderStages[] = { vShaderStageInfo, fShaderStageInfo };

    std::vector<VkDynamicState> dynamicStates = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR
    };

    VkPipelineDynamicStateCreateInfo dynamicState{};
    dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
    dynamicState.pDynamicStates = dynamicStates.data();

    VkPipelineDepthStencilStateCreateInfo depthState{};
    depthState.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthState.depthTestEnable = VK_TRUE;
    depthState.depthWriteEnable = VK_TRUE;
    depthState.depthCompareOp = VK_COMPARE_OP_LESS;
    depthState.depthBoundsTestEnable = VK_FALSE;
    depthState.minDepthBounds = 0.0f;
    depthState.maxDepthBounds = 1.0f;
    depthState.stencilTestEnable = VK_FALSE;
    depthState.front = {};
    depthState.back = {};

    VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
    vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputInfo.vertexBindingDescriptionCount = 1;
    auto desc = Vertex::getBindingDescription();
    auto attr = Vertex::getAttributeDescriptions();
    vertexInputInfo.pVertexBindingDescriptions = &desc;
    vertexInputInfo.vertexAttributeDescriptionCount = attr.size();
    vertexInputInfo.pVertexAttributeDescriptions = attr.data();

    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    inputAssembly.primitiveRestartEnable = VK_FALSE;

    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = (float)d->extent.width;
    viewport.height = (float)d->extent.height;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    VkRect2D scissor{};
    scissor.offset = { 0, 0 };
    scissor.extent = d->extent;

    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.pViewports = &viewport;
    viewportState.scissorCount = 1;
    viewportState.pScissors = &scissor;

    VkPipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.lineWidth = 1.0f;
    rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
    rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterizer.depthBiasEnable = VK_FALSE;
    rasterizer.depthBiasConstantFactor = 0.0f;
    rasterizer.depthBiasClamp = 0.0f;
    rasterizer.depthBiasSlopeFactor = 0.0f;

    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.sampleShadingEnable = VK_FALSE;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    multisampling.minSampleShading = 1.0f;
    multisampling.pSampleMask = nullptr;
    multisampling.alphaToCoverageEnable = VK_FALSE;
    multisampling.alphaToOneEnable = VK_FALSE;

    VkPipelineColorBlendAttachmentState colorBlendAttachment{};
    colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    colorBlendAttachment.blendEnable = VK_FALSE;
    colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
    colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
    colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
    colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;

    VkPipelineColorBlendStateCreateInfo colorBlending{};
    colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.logicOpEnable = VK_FALSE;
    colorBlending.logicOp = VK_LOGIC_OP_COPY;
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments = &colorBlendAttachment;

    VkPushConstantRange pushConstantRange{};
    pushConstantRange.offset = 0;
    pushConstantRange.size = sizeof(PushConstant);
    pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 2;
    std::vector<VkDescriptorSetLayout> setLayouts = { d->frameDependantLayout, mat->descriptorLayout };
    pipelineLayoutInfo.pSetLayouts = setLayouts.data();
    pipelineLayoutInfo.pushConstantRangeCount = 1;
    pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;

    if (vkCreatePipelineLayout(d->device, &pipelineLayoutInfo, nullptr, &(mat->layout)) != VK_SUCCESS) {
        throw std::runtime_error("Error creating pipeline layout");
    }

    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount = 2;
    pipelineInfo.pStages = shaderStages;
    pipelineInfo.pVertexInputState = &vertexInputInfo;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState = &multisampling;
    pipelineInfo.pColorBlendState = &colorBlending;
    pipelineInfo.pDynamicState = &dynamicState;
    pipelineInfo.pDepthStencilState = &depthState;
    pipelineInfo.layout = mat->layout;
    pipelineInfo.renderPass = d->renderPass;
    pipelineInfo.subpass = 0;
    pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;

    if (vkCreateGraphicsPipelines(d->device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &(mat->pipeline)) != VK_SUCCESS) {
        throw std::runtime_error("failed to create graphics pipeline!");
    }

    vkDestroyShaderModule(d->device, vShader, nullptr);
    vkDestroyShaderModule(d->device, fShader, nullptr);
}

//
inline void createDepthOnlyPipeline(Drawer* d) {
    std::vector<char> shaders[] = { getBytes("shaders/shadowmapv.spv"), getBytes("shaders/shadowmapf.spv") };
    VkShaderModule vShader = createShaderModule(d->device, shaders[0]);
    VkShaderModule fShader = createShaderModule(d->device, shaders[1]);

    VkPipelineShaderStageCreateInfo vShaderStageInfo{};
    vShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vShaderStageInfo.module = vShader;
    vShaderStageInfo.pName = "main";

    VkPipelineShaderStageCreateInfo fShaderStageInfo{};
    fShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    fShaderStageInfo.module = fShader;
    fShaderStageInfo.pName = "main";

    VkPipelineShaderStageCreateInfo shaderStages[] = { vShaderStageInfo };

    std::vector<VkDynamicState> dynamicStates = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR
    };

    VkPipelineDynamicStateCreateInfo dynamicState{};
    dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
    dynamicState.pDynamicStates = dynamicStates.data();

    VkPipelineDepthStencilStateCreateInfo depthState{};
    depthState.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthState.depthTestEnable = VK_TRUE;
    depthState.depthWriteEnable = VK_TRUE;
    depthState.depthCompareOp = VK_COMPARE_OP_GREATER;
    depthState.depthBoundsTestEnable = VK_FALSE;
    depthState.minDepthBounds = 0.0f;
    depthState.maxDepthBounds = 1.0f;
    depthState.stencilTestEnable = VK_FALSE;
    depthState.front = {};
    depthState.back = {};

    depthState.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;

    VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
    vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputInfo.vertexBindingDescriptionCount = 1;
    auto desc = Vertex::getBindingDescription();
    auto attr = Vertex::getAttributeDescriptions();
    vertexInputInfo.pVertexBindingDescriptions = &desc;
    vertexInputInfo.vertexAttributeDescriptionCount = attr.size();
    vertexInputInfo.pVertexAttributeDescriptions = attr.data();

    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    inputAssembly.primitiveRestartEnable = VK_FALSE;

    VkExtent2D ext = { 512, 512 };

    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = ext.width;
    viewport.height = ext.height;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    VkRect2D scissor{};
    scissor.offset = { 0, 0 };
    scissor.extent = ext;

    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.pViewports = &viewport;
    viewportState.scissorCount = 1;
    viewportState.pScissors = &scissor;

    VkPipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.lineWidth = 1.0f;
    rasterizer.cullMode = VK_CULL_MODE_NONE;
    rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterizer.depthBiasEnable = VK_TRUE;
    rasterizer.depthBiasConstantFactor = 0.0001f;
    rasterizer.depthBiasClamp = 0.0f;
    rasterizer.depthBiasSlopeFactor = -0.1f;

    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.sampleShadingEnable = VK_FALSE;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    multisampling.minSampleShading = 1.0f;
    multisampling.pSampleMask = nullptr;
    multisampling.alphaToCoverageEnable = VK_FALSE;
    multisampling.alphaToOneEnable = VK_FALSE;

    VkPipelineColorBlendStateCreateInfo colorBlending{};
    colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.logicOpEnable = VK_FALSE;
    colorBlending.logicOp = VK_LOGIC_OP_COPY;
    colorBlending.attachmentCount = 0;
    colorBlending.pAttachments = nullptr;

    VkPushConstantRange pushConstantRange{};
    pushConstantRange.offset = 0;
    pushConstantRange.size = sizeof(PushConstant);
    pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 1;
    std::vector<VkDescriptorSetLayout> setLayouts = { d->shadowMappingLayout };
    pipelineLayoutInfo.pSetLayouts = setLayouts.data();
    pipelineLayoutInfo.pushConstantRangeCount = 1;
    pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;

    VkPipelineLayout pLayout{};
    if (vkCreatePipelineLayout(d->device, &pipelineLayoutInfo, nullptr, &d->shadowPipelineLayout) != VK_SUCCESS) {
        throw std::runtime_error("Error creating pipeline layout");
    }
    //d->shadowPipelineLayout = pLayout;

    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount = 1;
    pipelineInfo.pStages = shaderStages;
    pipelineInfo.pVertexInputState = &vertexInputInfo;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState = &multisampling;
    pipelineInfo.pColorBlendState = &colorBlending;
    pipelineInfo.pDynamicState = &dynamicState;
    pipelineInfo.pDepthStencilState = &depthState;
    pipelineInfo.layout = d->shadowPipelineLayout;
    pipelineInfo.renderPass = d->shadowPass;
    pipelineInfo.subpass = 0;
    pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;

    if (vkCreateGraphicsPipelines(d->device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &(d->shadowPipeline)) != VK_SUCCESS) {
        throw std::runtime_error("failed to create graphics pipeline!");
    }
    //d->shadowPipeline = shadowPipeline;

    vkDestroyShaderModule(d->device, vShader, nullptr);
    vkDestroyShaderModule(d->device, fShader, nullptr);
}


inline void createDefaultDescSetLayout(Drawer* d) {
    VkDescriptorSetLayoutBinding samplerBinding{};
    samplerBinding.binding = 0;
    samplerBinding.descriptorCount = 1;
    samplerBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    samplerBinding.pImmutableSamplers = nullptr;
    samplerBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    std::vector<VkDescriptorSetLayoutBinding> bindings = { samplerBinding };

    VkDescriptorSetLayoutCreateInfo info{};
    info.bindingCount = static_cast<uint32_t>(bindings.size());
    info.pBindings = bindings.data();
    info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;

    if (vkCreateDescriptorSetLayout(d->device, &info, nullptr, &(d->defaultMaterialLayout)) != VK_SUCCESS) {
        std::runtime_error("Error creating descriptor set layout");
    };
}

//
inline void createShadowDescSetLayout(Drawer* d) {
    VkDescriptorSetLayoutBinding binding{};
    binding.binding = 0;
    binding.descriptorCount = 1;
    binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    binding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

    std::vector<VkDescriptorSetLayoutBinding> bindings = { binding };

    VkDescriptorSetLayoutCreateInfo createInfo{};
    createInfo.bindingCount = static_cast<uint32_t>(bindings.size());
    createInfo.pBindings = bindings.data();
    createInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;

    if (vkCreateDescriptorSetLayout(d->device, &createInfo, nullptr, &(d->shadowMappingLayout)) != VK_SUCCESS) {
        std::runtime_error("Error creating descriptor set layout");
    };
}

inline void createFrameDescriptorSets(Drawer* d) {
    std::vector<VkDescriptorSetLayoutBinding> bindings;
    bindings.resize(4);
    bindings[0].binding = 0;
    bindings[0].descriptorCount = 1;
    bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    bindings[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

    bindings[1].binding = 1;
    bindings[1].descriptorCount = 1;
    bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    bindings[1].stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

    bindings[2].binding = 2;
    bindings[2].descriptorCount = 1;
    bindings[2].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    bindings[2].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    bindings[3].binding = 3;
    bindings[3].descriptorCount = 1;
    bindings[3].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    bindings[3].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

    VkDescriptorSetLayoutCreateInfo createInfo{};
    createInfo.bindingCount = bindings.size();
    createInfo.pBindings = bindings.data();
    createInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;

    if (vkCreateDescriptorSetLayout(d->device, &createInfo, nullptr, &(d->frameDependantLayout)) != VK_SUCCESS) {
        std::runtime_error("Error creating descriptor set layout");
    };

    std::vector<VkDescriptorSetLayout> layouts(d->FRAMES_IN_FLIGHT, d->frameDependantLayout);
    VkDescriptorSetAllocateInfo allocateInfo{};
    allocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocateInfo.descriptorPool = d->descPool;
    allocateInfo.descriptorSetCount = static_cast<uint32_t>(d->FRAMES_IN_FLIGHT);
    allocateInfo.pSetLayouts = layouts.data();
    std::vector<VkDescriptorSet> descriptors(d->frameOrder.size(), nullptr);
    if (vkAllocateDescriptorSets(d->device, &allocateInfo, descriptors.data()) != VK_SUCCESS) {
        throw std::runtime_error("failed to allocate descriptor sets!");
    }

    for (size_t i = 0; i < d->frameOrder.size(); i++)
    {
        VkDescriptorBufferInfo bInfo{};
        bInfo.buffer = d->frameOrder[i].uniformBuffer;
        bInfo.offset = 0;
        bInfo.range = sizeof(UniformBufferObject);

        VkDescriptorBufferInfo b2Info{};
        b2Info.buffer = d->frameOrder[i].baseLightBuffer;
        b2Info.offset = 0;
        b2Info.range = sizeof(LightBufferObject);

        VkDescriptorImageInfo iInfo{};
        iInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
        iInfo.imageView = d->mainLight->shMapView;
        iInfo.sampler = d->mainLight->shMapSampler;

        VkDescriptorBufferInfo b3Info{};
        b3Info.buffer = d->frameOrder[i].vLightBuffer;
        b3Info.offset = 0;
        b3Info.range = d->maxVertLights * sizeof(VertexLight);

        std::vector<VkWriteDescriptorSet> descriptorWrites{};
        descriptorWrites.resize(4);

        descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[0].dstSet = descriptors[i];
        descriptorWrites[0].dstBinding = 0;
        descriptorWrites[0].dstArrayElement = 0;
        descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        descriptorWrites[0].descriptorCount = 1;
        descriptorWrites[0].pBufferInfo = &bInfo;

        descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[1].dstSet = descriptors[i];
        descriptorWrites[1].dstBinding = 1;
        descriptorWrites[1].dstArrayElement = 0;
        descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        descriptorWrites[1].descriptorCount = 1;
        descriptorWrites[1].pBufferInfo = &b2Info;

        descriptorWrites[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[2].dstSet = descriptors[i];
        descriptorWrites[2].dstBinding = 2;
        descriptorWrites[2].dstArrayElement = 0;
        descriptorWrites[2].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        descriptorWrites[2].descriptorCount = 1;
        descriptorWrites[2].pImageInfo = &iInfo;

        descriptorWrites[3].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[3].dstSet = descriptors[i];
        descriptorWrites[3].dstBinding = 3;
        descriptorWrites[3].dstArrayElement = 0;
        descriptorWrites[3].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        descriptorWrites[3].descriptorCount = 1;
        descriptorWrites[3].pBufferInfo = &b3Info;


        vkUpdateDescriptorSets(d->device, static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);

        d->frameOrder[i].frameDescSet = descriptors[i];
    }
}

inline void createDefaultDescriptorSet(Drawer* d, Material* mat, const render::Texture tex) {
    VkDescriptorSetAllocateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    info.descriptorPool = d->descPool;
    info.descriptorSetCount = static_cast<uint32_t>(1);
    info.pSetLayouts = &(mat->descriptorLayout);

    if (vkAllocateDescriptorSets(d->device, &info, &(mat->materialDescriptor)) != VK_SUCCESS) {
        throw std::runtime_error("failed to allocate descriptor sets!");
    }

    VkDescriptorImageInfo imgInfo{};
    imgInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    imgInfo.imageView = tex.textureView;
    imgInfo.sampler = tex.sampler;

    std::vector<VkWriteDescriptorSet> descriptorWrites{};
    descriptorWrites.resize(1);

    descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrites[0].dstSet = mat->materialDescriptor;
    descriptorWrites[0].dstBinding = 0;
    descriptorWrites[0].dstArrayElement = 0;
    descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    descriptorWrites[0].descriptorCount = 1;
    descriptorWrites[0].pImageInfo = &imgInfo;

    vkUpdateDescriptorSets(d->device, static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);
}


inline void createDefaultMaterial(Drawer* d) {
    Material material{};
    render::Texture texture(d, "textures/oad.ktx2", VK_IMAGE_VIEW_TYPE_2D);

    createDefaultDescSetLayout(d);
    material.descriptorLayout = d->defaultMaterialLayout;
    createDefaultDescriptorSet(d, &material, texture);

    std::vector<char> shaders[] = {getBytes("shaders/vert.spv"), getBytes("shaders/frag.spv")};
    createGraphicsPipeline(d, shaders, &material);
    d->registeredTextures.push_back(texture);
    d->registeredMaterials.push_back(material);
}

inline void createDefaultMesh(Drawer* d) {
    std::vector<Submesh::SubmeshCreateInfo> sub;
    sub.push_back(Submesh::SubmeshCreateInfo(d->defaultIndices.data(), d->defaultIndices.size(), 0));
    Mesh mesh(
        d,
        d->defaultBox.data(),
        d->defaultBox.size(),
        sub);
    d->registeredMeshes.push_back(mesh);
}

inline void createSkybox(Drawer* d) {
    Material material{};
    render::Texture texture(d, "textures/oadskybox.ktx2", VK_IMAGE_VIEW_TYPE_CUBE);

    material.descriptorLayout = d->defaultMaterialLayout;
    createDefaultDescriptorSet(d, &material, texture);

    std::vector<char> shaders[] = { getBytes("shaders/skyboxv.spv"), getBytes("shaders/skyboxf.spv") };
    createGraphicsPipeline(d, shaders, &material);
    d->registeredTextures.push_back(texture);
    d->registeredMaterials.push_back(material);
}


inline void createCommandPool(Drawer* d) {
    RequiredFamilyIndices queueFamilyIndices = getQueueFamilies(d->physicalDevice, d->surface);

    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    poolInfo.queueFamilyIndex = queueFamilyIndices.graphicsFamily.value();

    if (vkCreateCommandPool(d->device, &poolInfo, nullptr, &(d->commandPool)) != VK_SUCCESS) {
        throw std::runtime_error("failed to create command pool!");
    }
}


inline void createDepthStuff(Drawer* d) {
    VkFormat depthFormat = findSupportedFormat(
        d->physicalDevice,
        { VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT },
        VK_IMAGE_TILING_OPTIMAL,
        VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);
    createImage(d->device, d->physicalDevice, d->extent.width, d->extent.height, 1, depthFormat, VK_IMAGE_TILING_OPTIMAL, 0, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, d->depthImage, d->depthMemory);
    d->depthView = createImageView(d->device, d->depthImage, VK_IMAGE_VIEW_TYPE_2D, depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT);
    VkSamplerCreateInfo depthSamplerCreateInfo{};
    depthSamplerCreateInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    depthSamplerCreateInfo.magFilter = VK_FILTER_LINEAR;
    depthSamplerCreateInfo.minFilter = VK_FILTER_LINEAR;
    depthSamplerCreateInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    depthSamplerCreateInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    depthSamplerCreateInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    depthSamplerCreateInfo.anisotropyEnable = VK_TRUE;
    depthSamplerCreateInfo.flags = 0;
    VkPhysicalDeviceProperties properties{};
    vkGetPhysicalDeviceProperties(d->physicalDevice, &properties);
    depthSamplerCreateInfo.maxAnisotropy = properties.limits.maxSamplerAnisotropy;
    depthSamplerCreateInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    depthSamplerCreateInfo.unnormalizedCoordinates = VK_FALSE;
    depthSamplerCreateInfo.compareEnable = VK_FALSE;
    depthSamplerCreateInfo.compareOp = VK_COMPARE_OP_ALWAYS;
    depthSamplerCreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    depthSamplerCreateInfo.mipLodBias = 0.0f;
    depthSamplerCreateInfo.minLod = 0.0f;
    depthSamplerCreateInfo.maxLod = 0.0f;
    depthSamplerCreateInfo.anisotropyEnable = VK_TRUE;
    depthSamplerCreateInfo.pNext = nullptr;
    vkCreateSampler(d->device, &depthSamplerCreateInfo, nullptr, &(d->depthSampler));
}


inline void createUniformBuffers(Drawer* d) {
    VkDeviceSize size = sizeof(UniformBufferObject);
    VkDeviceSize size2 = sizeof(LightBufferObject);
    VkDeviceSize size3 = sizeof(VertexLight) * d->maxVertLights;

    for (size_t i = 0; i < d->FRAMES_IN_FLIGHT; i++)
    {
        createBuffer(d->device, d->physicalDevice, size, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, d->frameOrder[i].uniformBuffer, d->frameOrder[i].uniformBufferMemory);
        vkMapMemory(d->device, d->frameOrder[i].uniformBufferMemory, 0, size, 0, &(d->frameOrder[i].uniformMappedMemory));

        createBuffer(d->device, d->physicalDevice, size2, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, d->frameOrder[i].baseLightBuffer, d->frameOrder[i].baseLightBufferMemory);
        vkMapMemory(d->device, d->frameOrder[i].baseLightBufferMemory, 0, size2, 0, &(d->frameOrder[i].lightMappedMemory));

        createBuffer(d->device, d->physicalDevice, size3, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, d->frameOrder[i].vLightBuffer, d->frameOrder[i].vLightMemory);
    }
}

inline void createCommandBuffers(Drawer* d) {
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = d->commandPool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = 1;

    for (size_t i = 0; i < d->frameOrder.size(); i++)
    {
        if (vkAllocateCommandBuffers(d->device, &allocInfo, &d->frameOrder[i].frameCommandBuffer) != VK_SUCCESS) {
            throw std::runtime_error("failed to allocate command buffers!");
        }
    }
}


inline void createSyncObjects(Drawer* d) {
    VkSemaphoreCreateInfo semaphoreInfo{};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    /*if (vkCreateSemaphore(device, &semaphoreInfo, nullptr, &imageAvailableSemaphore) != VK_SUCCESS ||
        vkCreateSemaphore(device, &semaphoreInfo, nullptr, &renderFinishedSemaphore) != VK_SUCCESS ||
        vkCreateFence(device, &fenceInfo, nullptr, &inFlightFence) != VK_SUCCESS) {
        throw std::runtime_error("failed to create semaphores!");
    }*/

    for (size_t i = 0; i < d->frameOrder.size(); i++)
    {
        d->frameOrder[i].imageAvailable = VkSemaphore{};
        d->frameOrder[i].imageFinished = VkSemaphore{};
        d->frameOrder[i].fence = VkFence{};

        if (vkCreateSemaphore(d->device, &semaphoreInfo, nullptr, &d->frameOrder[i].imageAvailable) != VK_SUCCESS ||
            vkCreateSemaphore(d->device, &semaphoreInfo, nullptr, &d->frameOrder[i].imageFinished) != VK_SUCCESS ||
            vkCreateFence(d->device, &fenceInfo, nullptr, &d->frameOrder[i].fence) != VK_SUCCESS) {
            throw std::runtime_error("failed to create semaphores!");
        }
    }
}

inline void createKTXstuff(Drawer* d) {
    d->ktxVulkanInfo = ktxVulkanDeviceInfo_CreateEx(d->instance, d->physicalDevice, d->device, d->graphicsQueue, d->commandPool, nullptr, nullptr);
}

inline void test(Drawer* d) {
    for (size_t i = 0; i < d->frameOrder.size(); i++)
    {
        VkDescriptorImageInfo iInfo{};
        iInfo.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
        iInfo.imageView = d->mainLight->shMapView;
        iInfo.sampler = d->mainLight->shMapSampler;
        iInfo.imageView = d->registeredTextures[0].textureView;
        iInfo.sampler = d->registeredTextures[0].sampler;

        std::vector<VkWriteDescriptorSet> descriptorWrites{};
        descriptorWrites.resize(1);

        descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[0].dstSet = d->frameOrder[i].frameDescSet;
        descriptorWrites[0].dstBinding = 2;
        descriptorWrites[0].dstArrayElement = 0;
        descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        descriptorWrites[0].descriptorCount = 1;
        descriptorWrites[0].pImageInfo = &iInfo;

        vkUpdateDescriptorSets(d->device, static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);
    }
}

Drawer::Drawer() {
    this->init();
}

void Drawer::init() {
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

    window = glfwCreateWindow(WIDTH, HEIGHT, "Vulkan", nullptr, nullptr);
    glfwSetWindowUserPointer(window, this);
    glfwSetFramebufferSizeCallback(window, resizeCallback);

    createInstance(this);

    if (glfwCreateWindowSurface(instance, window, nullptr, &surface) != VK_SUCCESS) {
        throw std::runtime_error("failed to create window surface!");
    }

    frameOrder.resize(FRAMES_IN_FLIGHT);

    pickDevice(this);
    createLogicalDevice(this);
    createSwapchain(this); //frames resized here
    createImageViews(this);
    createRenderPass(this);
    createDepthOnlyPass(this);
    createDepthStuff(this);
    createFramebuffers(this);
    std::cout << "Creating buffers and pools...\n";
    createCommandPool(this);
    createDescriptorPool(this);
    createUniformBuffers(this);
    createCommandBuffers(this);
    createShadowDescSetLayout(this);
    mainLight = new Light(this, glm::mat4(10.0), 60, true);
    createFrameDescriptorSets(this);
    createKTXstuff(this);
    std::cout << "Creating debug objects...\n";
    createDepthOnlyPipeline(this);
    createDefaultMaterial(this);
    createDefaultMesh(this);
    //createSkybox(this);
    std::cout << "Creating sync objects...\n";
    createSyncObjects(this);

    vkUtil::init(device, physicalDevice, graphicsQueue);

    vertexLights.push_back({{10, 10, 1, 0}, {1, 0, 1, 0}});

    //glfw
}

void Drawer::loadMesh(const char* dir, uint16_t* index, uint16_t materialIndex) {
    tinyobj::attrib_t attributes;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;
    std::string errorMsg;
    tinyobj::LoadObj(&attributes, &shapes, &materials, &errorMsg, dir);
    std::vector<render::Vertex> vertices;
    std::vector<uint32_t> indices;
    std::vector<Submesh::SubmeshCreateInfo> submeshes;

    for (size_t i = 0; i < attributes.vertices.size() / 3; i++)
    {
        size_t I = i * 3;
        size_t J = i * 2;
        vertices.push_back({
            {attributes.vertices[I], attributes.vertices[I + 1], attributes.vertices[I + 2]},
            {attributes.normals[I], attributes.normals[I + 1], attributes.normals[I + 2]},
            {attributes.texcoords[J], attributes.texcoords[J + 1]} });
    }

    //TODO: better iterator?
    std::vector<uint16_t> submeshIndices;
    for (size_t i = 0; i < shapes.size(); i++)
    {
        for (size_t j = 0; j < shapes[i].mesh.indices.size(); j++)
        {
            submeshIndices.push_back(shapes[i].mesh.indices[j].vertex_index);
        }
    }

    submeshes.push_back(Submesh::SubmeshCreateInfo(submeshIndices.data(), submeshIndices.size(), materialIndex));

    Mesh m(this, vertices.data(), vertices.size(), submeshes);
    registeredMeshes.push_back(m);
    *index = registeredMeshes.size() - 1;
}

void Drawer::beginPass(std::vector<VkClearValue> clearValues) {
    beginPass(frames[currentSwapchainIndex].framebuffer, renderPass, clearValues, extent);
};

void Drawer::beginPass(VkFramebuffer frame, VkRenderPass pass, std::vector<VkClearValue> clearValues, VkExtent2D ext) {
    VkRenderPassBeginInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass = pass;
    renderPassInfo.framebuffer = frame;

    renderPassInfo.renderArea.offset = { 0, 0 };
    renderPassInfo.renderArea.extent = ext;
    renderPassInfo.clearValueCount = clearValues.size();
    renderPassInfo.pClearValues = clearValues.data();

#ifdef DEBUG_GRAPHICS
    std::cout << "Beginning render pass...\n";
#endif

    vkCmdBeginRenderPass(frameOrder[currentFrame].frameCommandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = static_cast<float>(ext.width);
    viewport.height = static_cast<float>(ext.height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(frameOrder[currentFrame].frameCommandBuffer, 0, 1, &viewport);

    VkRect2D scissor{};
    scissor.offset = { 0, 0 };
    scissor.extent = ext;
    vkCmdSetScissor(frameOrder[currentFrame].frameCommandBuffer, 0, 1, &scissor);
}

inline void updateUBOs(Drawer* d, int frame, glm::mat4 cameraView, double FOV, std::vector<glm::mat4> lightViews, std::vector<double> lightFOVs) {
    UniformBufferObject ubo{};
    ubo.view = cameraView;
    ubo.proj = glm::perspective(glm::radians(FOV), d->extent.width / (double)d->extent.height, 0.1, 10000.0);
    ubo.proj[1][1] *= -1;
    memcpy(d->frameOrder[frame].uniformMappedMemory, &ubo, sizeof(ubo));

    LightBufferObject lbo{};
    lbo.view = lightViews[0];
    lbo.proj = glm::perspective(glm::radians(lightFOVs[0]), 1.0, 100.0, 0.1);
    lbo.color = { 1, 1, 1, 1 };
    memcpy(d->mainLight->mappedMemory[frame], &lbo, sizeof(lbo));
    memcpy(d->frameOrder[frame].lightMappedMemory, &lbo, sizeof(lbo));

    vkUtil::begincpy(d->device, d->commandPool);
    VkBufferCopy cpy1{};
    cpy1.dstOffset = 0;
    cpy1.srcOffset = 0;
    cpy1.size = sizeof(glm::vec4);
    glm::vec4 data;
    data.x = static_cast<float>(d->vertexLights.size());
    std::cout << cpy1.size << "\n";
    vkUtil::gpumemcpy(d->device, d->physicalDevice, d->graphicsQueue, d->commandPool, d->frameOrder[frame].vLightBuffer, &data, cpy1);
    VkBufferCopy cpy2{};
    cpy2.dstOffset = sizeof(glm::vec4);
    cpy2.srcOffset = 0;
    cpy2.size = sizeof(VertexLight) * d->vertexLights.size();
    std::cout << cpy2.size << "\n";
    vkUtil::gpumemcpy(d->device, d->physicalDevice, d->graphicsQueue, d->commandPool, d->frameOrder[frame].vLightBuffer, d->vertexLights.data(), cpy2);
    vkUtil::endcpy(d->graphicsQueue);

#ifdef DEBUG_GRAPHICS
    std::cout << lbo.proj[0][0] << " " << lbo.proj[0][1] << " " << lbo.proj[0][2] << " " << lbo.proj[0][3] << "\n";
    std::cout << lbo.proj[1][0] << " " << lbo.proj[1][1] << " " << lbo.proj[1][2] << " " << lbo.proj[1][3] << "\n";
    std::cout << lbo.proj[2][0] << " " << lbo.proj[2][1] << " " << lbo.proj[2][2] << " " << lbo.proj[2][3] << "\n";
    std::cout << lbo.proj[3][0] << " " << lbo.proj[3][1] << " " << lbo.proj[3][2] << " " << lbo.proj[3][3] << "\n";
#endif


    /*
//Normal proj
1 0 0 0
0 1 0 0
0 0 -1.001 -1
0 0 -0.1001 0

//Reversed proj (working)
1 0 0 0
0 1 0 0
0 0 0.001001 -1
0 0 0.1001 0

//Bruh
1 0 0 0
0 1 0 0
0 0 0.001001 -1
0 0 -0.1001 0
    */
}

void Drawer::beginFrame(glm::mat4 cameraView, double FOV, std::vector<glm::mat4> lightViews, std::vector<double> lightFOVs) {
    /*UniformBufferObject ubo{};
    ubo.view = cameraView;
    ubo.proj = glm::perspective(glm::radians(FOV), extent.width / (double)extent.height, 0.1, 10000.0);
    ubo.proj[1][1] *= -1;
    memcpy(frameOrder[currentFrame].uniformMappedMemory, &ubo, sizeof(ubo));*/

    updateUBOs(this, currentFrame, cameraView, FOV, lightViews, lightFOVs);

    vkWaitForFences(device, 1, &frameOrder[currentFrame].fence, true, UINT64_MAX);

    VkResult result = vkAcquireNextImageKHR(device, presentSwapchain, UINT64_MAX, frameOrder[currentFrame].imageAvailable, VK_NULL_HANDLE, &currentSwapchainIndex);

    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        recreateSwapchain();
        return;
    }
    else if (result != VK_SUBOPTIMAL_KHR && result != VK_SUCCESS) {
        throw std::runtime_error("Error fetching swapchain image");
    }

    vkResetFences(device, 1, &frameOrder[currentFrame].fence);

    vkResetCommandBuffer(frameOrder[currentFrame].frameCommandBuffer, 0);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

    if (vkBeginCommandBuffer(frameOrder[currentFrame].frameCommandBuffer, &beginInfo) != VK_SUCCESS) {
        throw std::runtime_error("failed to begin recording command buffer!");
    }
}

void Drawer::bindShadowPassPipeline() {
    vkCmdBindPipeline(frameOrder[currentFrame].frameCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, shadowPipeline);
    vkCmdBindDescriptorSets(frameOrder[currentFrame].frameCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, shadowPipelineLayout, 0, 1, &(mainLight->frameSets[currentFrame]), 0, nullptr);
}

/* Record the command buffer with all the draw commands */
void Drawer::draw() {

}
void Drawer::draw(Sprite* s) {

}
void Drawer::draw(Mesh* m, Submesh* s, Material* mat, glm::mat4 modelMatrix, bool bindMaterial) {
    if ((frameOrder[currentFrame].boundMaterial == nullptr || mat != frameOrder[currentFrame].boundMaterial) && bindMaterial) {
#ifdef DEBUG_GRAPHICS
        std::cout << "Binding new material...\n";
#endif
        frameOrder[currentFrame].boundMaterial = mat;
        vkCmdBindPipeline(frameOrder[currentFrame].frameCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, mat->pipeline);
        std::vector<VkDescriptorSet> sets = { frameOrder[currentFrame].frameDescSet, (mat->materialDescriptor)};
        vkCmdBindDescriptorSets(frameOrder[currentFrame].frameCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, mat->layout, 0, sets.size(), sets.data(), 0, nullptr);
    }
#ifdef DEBUG_GRAPHICS
    std::cout << "Beginning draw...\n";
    std::cout << "Index count: " << s->iBufferSize << "\n";
#endif

    VkBuffer vertBuffers[] = { m->vertexBuffer };
    VkDeviceSize offsets[] = { 0 };
    PushConstant push{};
    push.model = modelMatrix;

    //std::cout << modelMatrix[0][0] << " " << modelMatrix[1][0] << " " << modelMatrix[2][0] << " " << modelMatrix[3][0] << "\n";
    //std::cout << modelMatrix[0][1] << " " << modelMatrix[1][1] << " " << modelMatrix[2][1] << " " << modelMatrix[3][1] << "\n";
    //std::cout << modelMatrix[0][2] << " " << modelMatrix[1][2] << " " << modelMatrix[2][2] << " " << modelMatrix[3][2] << "\n";
    //std::cout << modelMatrix[0][3] << " " << modelMatrix[1][3] << " " << modelMatrix[2][3] << " " << modelMatrix[3][3] << "\n";

    vkCmdBindVertexBuffers(frameOrder[currentFrame].frameCommandBuffer, 0, 1, vertBuffers, offsets);
    vkCmdBindIndexBuffer(frameOrder[currentFrame].frameCommandBuffer, s->indexBuffer, 0, VK_INDEX_TYPE_UINT16);
    vkCmdPushConstants(frameOrder[currentFrame].frameCommandBuffer, mat->layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(PushConstant), &push);
    vkCmdDrawIndexed(frameOrder[currentFrame].frameCommandBuffer, static_cast<uint32_t>(s->iBufferSize), 1, 0, 0, 0);
    //Pipeline not being bound?
}

void Drawer::endPass() {
    vkCmdEndRenderPass(frameOrder[currentFrame].frameCommandBuffer);
}

/* Submit the recorded command buffer */
void Drawer::submitDraws() {
    if (vkEndCommandBuffer(frameOrder[currentFrame].frameCommandBuffer) != VK_SUCCESS) {
        throw std::runtime_error("failed to record command buffer!");
    }

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

    VkSemaphore waitSemaphores[] = { frameOrder[currentFrame].imageAvailable };
    VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = waitSemaphores;
    submitInfo.pWaitDstStageMask = waitStages;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &frameOrder[currentFrame].frameCommandBuffer;

    VkSemaphore signalSemaphores[] = { frameOrder[currentFrame].imageFinished };
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = signalSemaphores;

    if (vkQueueSubmit(graphicsQueue, 1, &submitInfo, frameOrder[currentFrame].fence) != VK_SUCCESS) {
        throw std::runtime_error("failed to submit draw command buffer!");
    }
}

/*Queue the presentation */
void Drawer::endFrame() {
    frameOrder[currentFrame].boundMaterial = nullptr;

    VkPresentInfoKHR presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;

    VkSemaphore signalSemaphores[] = { frameOrder[currentFrame].imageFinished };
    presentInfo.pWaitSemaphores = signalSemaphores;

    VkSwapchainKHR swapChains[] = { presentSwapchain };
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = swapChains;
    presentInfo.pImageIndices = &currentSwapchainIndex;
    presentInfo.pResults = nullptr; // Optional

    std::cout << currentSwapchainIndex << " " << currentFrame << "\n";

    VkResult result = vkQueuePresentKHR(presentQueue, &presentInfo);

    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        recreateSwapchain();
        return;
    }
    else if (result != VK_SUBOPTIMAL_KHR && result != VK_SUCCESS) {
        throw std::runtime_error("Error fetching swapchain image");
    }

    currentFrame++;
    currentFrame %= FRAMES_IN_FLIGHT;
}

void Drawer::recreateSwapchain() {
    int w, h = 0;
    glfwGetFramebufferSize(window, &w, &h);

    while (w == 0 || h == 0) {
        glfwGetFramebufferSize(window, &w, &h);
        glfwWaitEvents();
    }

    vkDeviceWaitIdle(device);

    cleanupSwapchain();

    createSwapchain(this);
    createImageViews(this);
    createDepthStuff(this);
    createFramebuffers(this);
}

void Drawer::cleanupSwapchain() {
    std::cout << "Destroying framebuffers & image views...\n";
    for (size_t i = 0; i < frames.size(); i++)
    {
        vkDestroyFramebuffer(device, frames[i].framebuffer, nullptr);
        vkDestroyImageView(device, frames[i].swapchainImageView, nullptr);
    }
    std::cout << "Destroying swapchain...\n";
    vkDestroySwapchainKHR(device, presentSwapchain, nullptr);
}

Drawer::~Drawer() {
    vkDeviceWaitIdle(device);
    std::cout << "Cleaning up...\n";

    std::cout << "Destroying desc pool & layouts...\n";
    vkDestroyDescriptorPool(device, descPool, nullptr);
    vkDestroyDescriptorSetLayout(device, descSetLayout, nullptr);
    vkDestroyDescriptorSetLayout(device, frameDependantLayout, nullptr);
    vkDestroyDescriptorSetLayout(device, defaultMaterialLayout, nullptr);
    vkDestroyDescriptorSetLayout(device, shadowMappingLayout, nullptr);

    std::cout << "Destroying uniform buffers & sync objects...\n";
    for (size_t i = 0; i < frameOrder.size(); i++)
    {
        vkDestroyBuffer(device, frameOrder[i].uniformBuffer, nullptr);
        vkFreeMemory(device, frameOrder[i].uniformBufferMemory, nullptr);

        vkDestroyBuffer(device, frameOrder[i].baseLightBuffer, nullptr);
        vkFreeMemory(device, frameOrder[i].baseLightBufferMemory, nullptr);

        vkDestroyBuffer(device, frameOrder[i].vLightBuffer, nullptr);
        vkFreeMemory(device, frameOrder[i].vLightMemory, nullptr);

        vkDestroySemaphore(device, frameOrder[i].imageAvailable, nullptr);
        vkDestroySemaphore(device, frameOrder[i].imageFinished, nullptr);
        vkDestroyFence(device, frameOrder[i].fence, nullptr);
    }
    vkUtil::free(device);

    std::cout << "Destroying command pool...\n";
    vkDestroyCommandPool(device, commandPool, nullptr);

    std::cout << "Destroying swapchain...\n";
    cleanupSwapchain();

    std::cout << "Freeing texture resources...\n";
    for (size_t i = 0; i < registeredTextures.size(); i++)
    {
        registeredTextures[i].free(this);
    }

    std::cout << "Freeing model resources...\n";
    for (size_t i = 0; i < registeredMeshes.size(); i++)
    {
        registeredMeshes[i].free(this);
    }

    std::cout << "Destroying materials...\n";
    for (size_t i = 0; i < registeredMaterials.size(); i++)
    {
        vkDestroyPipeline(device, registeredMaterials[i].pipeline, nullptr);
        vkDestroyPipelineLayout(device, registeredMaterials[i].layout, nullptr);
    }

    std::cout << "Destroying lights...\n";
    mainLight->free(this);
    for (size_t i = 0; i < registeredLights.size(); i++)
    {
        registeredLights[i].free(this);
    }
    vkDestroyPipeline(device, shadowPipeline, nullptr);
    vkDestroyPipelineLayout(device, shadowPipelineLayout, nullptr);

    std::cout << "Freeing depth & stencil resources...\n";
    vkDestroyImage(device, depthImage, nullptr);
    vkDestroyImageView(device, depthView, nullptr);
    vkFreeMemory(device, depthMemory, nullptr);
    vkDestroySampler(device, depthSampler, nullptr);

    std::cout << "Destroying pipeline and render passes...\n";
    vkDestroyRenderPass(device, renderPass, nullptr);
    vkDestroyRenderPass(device, shadowPass, nullptr);

    std::cout << "Destroying virtual device...\n";
    vkDestroyDevice(device, nullptr);

    std::cout << "Destroying KHR surface and instance...\n";
    vkDestroySurfaceKHR(instance, surface, nullptr);
    vkDestroyInstance(instance, nullptr);

    std::cout << "Terminating GLFW...\n";
    glfwTerminate();
}

inline void stbTextureLoad(Drawer* d, const char* dir, VkImage* imgBuffer, VkFormat format, int usage, VkImageAspectFlagBits aspect, VkImageLayout finalLayout, VkDeviceMemory* imgMemory, VkImageView* imgView, VkSamplerCreateInfo info, VkSampler* sampler) {
    int width, height;

    int texChannels;
    std::cout << dir << "\n";
    stbi_uc* pixels = stbi_load(dir, &width, &height, &texChannels, STBI_rgb_alpha);
    VkDeviceSize size = width * height * 4;

    if (!pixels) {
        throw std::runtime_error("Failed to load image");
    }

    VkBuffer staging;
    VkDeviceMemory memory;
    createBuffer(d->device, d->physicalDevice, size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, staging, memory);

    void* data;
    vkMapMemory(d->device, memory, 0, size, 0, &data);
    memcpy(data, pixels, static_cast<uint32_t>(size));
    vkUnmapMemory(d->device, memory);

    stbi_image_free(pixels);

    createImage(d->device, d->physicalDevice, width, height, 1, format, VK_IMAGE_TILING_OPTIMAL, 0, VK_IMAGE_USAGE_TRANSFER_DST_BIT | usage, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, *imgBuffer, *imgMemory);

    transitionImageLayout(d->device, d->graphicsQueue, d->commandPool, *imgBuffer, format, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, aspect);
    VkCommandBuffer cmdBuffer = beginSimpleCommands(d->device, d->commandPool);

    VkBufferImageCopy region{};
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;
    region.bufferOffset = 0;
    region.imageSubresource.aspectMask = aspect;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;
    region.imageExtent = { static_cast<uint32_t>(width), static_cast<uint32_t>(height), 1 };
    region.imageOffset = { 0, 0, 0 };

    vkCmdCopyBufferToImage(cmdBuffer, staging, *imgBuffer, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

    endSimpleCommands(cmdBuffer, d->graphicsQueue);
    transitionImageLayout(d->device, d->graphicsQueue, d->commandPool, *imgBuffer, format, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, finalLayout, aspect);

    vkDestroyBuffer(d->device, staging, nullptr);
    vkFreeMemory(d->device, memory, nullptr);

    *imgView = createImageView(d->device, *imgBuffer, VK_IMAGE_VIEW_TYPE_2D, format, aspect);

    if (vkCreateSampler(d->device, &info, nullptr, sampler) != VK_SUCCESS) {
        std::runtime_error("Failed to create image sampler");
    }
}

inline void ktxTextureLoad(Drawer* d, const char* dir, ktxTexture2* texture, ktxVulkanTexture* dstTexture, VkImageUsageFlags usageFlags) {
    /*std::ifstream file(dir, std::ios::in | std::ios::binary);
    file.seekg(0, std::ios::end);
    std::cout << file.tellg() << "\n";
    std::vector<char> dataBytes(file.tellg());
    file.seekg(0);
    file.read(dataBytes.data(), dataBytes.size());
    std::cout << ktxTexture_CreateFromMemory(reinterpret_cast<ktx_uint8_t*>(dataBytes.data()), dataBytes.size(), KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT, &texture) << "\n";*/
    ktxTexture2_CreateFromNamedFile(dir, KTX_TEXTURE_CREATE_NO_FLAGS, &texture);
    //std::cout << texture->numDimensions << "\n";
    //VkFormatProperties properties;
    //VkImageFormatProperties imgProperties;
    //vkGetPhysicalDeviceFormatProperties(d->physicalDevice, (VkFormat)29, &properties);
    //std::cout << texture->numDimensions << "\n";
    //std::cout << vkGetPhysicalDeviceImageFormatProperties(d->physicalDevice, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_TYPE_2D, VK_IMAGE_TILING_OPTIMAL, usageFlags, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, &imgProperties) << "\n";
    //std::cout << !texture->pData << "\n";
    //std::cout << properties.
    //std::cout << ktxTexture_GetVkFormat(texture) << "\n";
    //VK_FORMAT_R8G8B8A8_UNORM;
    std::cout << "Texture VkFormat:" << ktxTexture2_GetVkFormat(texture) << "\n"; 
    ktxTexture2_VkUploadEx(texture, d->ktxVulkanInfo, dstTexture, VK_IMAGE_TILING_OPTIMAL, usageFlags, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    //std::cout << dstTexture->height << "\n";
}

render::Texture::Texture(Drawer* d, const char* dir, VkImageViewType imageType) {
    ktxTexture2* texPtr = &texture;
    ktxTextureLoad(d, dir, texPtr, &vkTexture, VK_IMAGE_USAGE_SAMPLED_BIT);
    texture = *texPtr;
    //std::cout << texture.baseHeight << "\n";

    textureView = createImageView(d->device, vkTexture.image, imageType, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_ASPECT_COLOR_BIT);

    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.anisotropyEnable = VK_TRUE;
    VkPhysicalDeviceProperties properties{};
    vkGetPhysicalDeviceProperties(d->physicalDevice, &properties);
    samplerInfo.maxAnisotropy = properties.limits.maxSamplerAnisotropy;
    samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;
    samplerInfo.compareEnable = VK_FALSE;
    samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerInfo.mipLodBias = 0.0f;
    samplerInfo.minLod = 0.0f;
    samplerInfo.maxLod = 0.0f;
    samplerInfo.anisotropyEnable = VK_TRUE;

    if (vkCreateSampler(d->device, &samplerInfo, nullptr, &sampler) != VK_SUCCESS) {
        std::runtime_error("Failed to create image sampler");
    }
}

void Texture::free(Drawer* d) {
    vkDestroySampler(d->device, sampler, nullptr);
    vkDestroyImageView(d->device, textureView, nullptr);
    vkDestroyImage(d->device, vkTexture.image, nullptr);
    vkFreeMemory(d->device, vkTexture.deviceMemory, nullptr);
}

render::Light::Light(Drawer* d, glm::mat4 origin, float FOV, bool isDynamic) {
    transform = origin;
    this->FOV = FOV;
    this->isDynamic = isDynamic;
    VkFormat format = findDepthSamplerFormat(d->physicalDevice);
    std::cout << format << "\n";
    createImage(d->device, d->physicalDevice, 512, 512, 1, format, VK_IMAGE_TILING_OPTIMAL, 0, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, this->shMap, this->shMapMemory);
    this->shMapView = createImageView(d->device, this->shMap, VK_IMAGE_VIEW_TYPE_2D, format, VK_IMAGE_ASPECT_DEPTH_BIT);

    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    samplerInfo.anisotropyEnable = VK_TRUE;
    VkPhysicalDeviceProperties properties{};
    vkGetPhysicalDeviceProperties(d->physicalDevice, &properties);
    samplerInfo.maxAnisotropy = properties.limits.maxSamplerAnisotropy;
    samplerInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;
    samplerInfo.compareEnable = VK_FALSE;
    samplerInfo.compareOp = VK_COMPARE_OP_NEVER;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerInfo.mipLodBias = 0.0f;
    samplerInfo.minLod = 0.0f;
    samplerInfo.maxLod = 0.0f;
    samplerInfo.anisotropyEnable = VK_TRUE;

    if (vkCreateSampler(d->device, &samplerInfo, nullptr, &(this->shMapSampler)) != VK_SUCCESS) {
        std::runtime_error("Failed to create image sampler");
    }

    frames.resize(d->frames.size());
    for (size_t i = 0; i < d->frames.size(); i++)
    {
        VkImageView attatchments[] = {
            shMapView
        };

        VkFramebufferCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        createInfo.renderPass = d->shadowPass;
        createInfo.pAttachments = attatchments;
        createInfo.width = 512;
        createInfo.height = 512;
        createInfo.attachmentCount = 1;
        createInfo.layers = 1;

        if (vkCreateFramebuffer(d->device, &createInfo, nullptr, &(this->frames[i])) != VK_SUCCESS) {
            throw std::runtime_error("failed to create framebuffer!");
        }
    }

    framePositions.resize(d->FRAMES_IN_FLIGHT);
    frameSets.resize(d->FRAMES_IN_FLIGHT);
    mappedMemory.resize(d->FRAMES_IN_FLIGHT);

    std::vector<VkDescriptorSetLayout> layouts(d->FRAMES_IN_FLIGHT, d->shadowMappingLayout);
    VkDescriptorSetAllocateInfo allocateInfo{};
    allocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocateInfo.descriptorPool = d->descPool;
    allocateInfo.descriptorSetCount = static_cast<uint32_t>(d->FRAMES_IN_FLIGHT);
    allocateInfo.pSetLayouts = layouts.data();
    if (vkAllocateDescriptorSets(d->device, &allocateInfo, frameSets.data()) != VK_SUCCESS) {
        throw std::runtime_error("failed to allocate descriptor sets!");
    }

    for (size_t i = 0; i < framePositions.size(); i++)
    {
        createBuffer(d->device, d->physicalDevice, sizeof(LightBufferObject), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, framePositions[i].buffer, framePositions[i].memory);
        vkMapMemory(d->device, framePositions[i].memory, 0, sizeof(LightBufferObject), 0, &mappedMemory[i]);

        VkDescriptorBufferInfo bInfo{};
        bInfo.buffer = framePositions[i].buffer;
        bInfo.offset = 0;
        bInfo.range = sizeof(LightBufferObject);

        std::vector<VkWriteDescriptorSet> descriptorWrites{};
        descriptorWrites.resize(1);

        descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[0].dstSet = frameSets[i];
        descriptorWrites[0].dstBinding = 0;
        descriptorWrites[0].dstArrayElement = 0;
        descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        descriptorWrites[0].descriptorCount = 1;
        descriptorWrites[0].pBufferInfo = &bInfo;

        vkUpdateDescriptorSets(d->device, static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);
    }
};

void Light::free(Drawer* d) {
    vkDestroyImage(d->device, shMap, nullptr);
    vkDestroyImageView(d->device, shMapView, nullptr);
    vkFreeMemory(d->device, shMapMemory, nullptr);
    vkDestroySampler(d->device, shMapSampler, nullptr);

    for (size_t i = 0; i < frames.size(); i++)
    {
        vkDestroyFramebuffer(d->device, frames[i], nullptr);
    }

    for (size_t i = 0; i < framePositions.size(); i++)
    {
        vkDestroyBuffer(d->device, framePositions[i].buffer, nullptr);
        vkFreeMemory(d->device, framePositions[i].memory, nullptr);
    }
}

render::Material::Material() {

}

render::Submesh::Submesh() {

}

render::Submesh::Submesh(const Drawer* d, Submesh::SubmeshCreateInfo info) {
    this->materialIndex = info.materialIndex;
    createBuffer(d->device, d->physicalDevice, info.count * sizeof(uint16_t), VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, indexBuffer, indexMemory);
    iBufferSize = info.count;
    vkUtil::staging istaging(d->device, d->physicalDevice, d->graphicsQueue);
    istaging.transfer(info.count * sizeof(uint16_t), info.indices, &(indexBuffer), &(d->commandPool));
}

Submesh::SubmeshCreateInfo::SubmeshCreateInfo(const uint16_t* indices, uint32_t count, uint16_t materialIndex) {
    this->indices = indices;
    this->count = count;
    this->materialIndex = materialIndex;
}

render::Mesh::Mesh(const Drawer* d, const Vertex* vertices, const uint32_t vcount, const std::vector<Submesh::SubmeshCreateInfo> createInfos) {
    std::vector<Submesh> s;

    for (size_t i = 0; i < createInfos.size(); i++)
    {
        s.push_back(Submesh(d, createInfos[i]));
    }

    this->submeshes = s;

    createBuffer(d->device, d->physicalDevice, vcount * sizeof(Vertex), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, vertexBuffer, vertexMemory);
    vBufferSize = vcount;
    vkUtil::staging vstaging(d->device, d->physicalDevice, d->graphicsQueue);
    vstaging.transfer(vcount * sizeof(Vertex), vertices, &(Mesh::vertexBuffer), &(d->commandPool));
}

void Mesh::free(Drawer* d) {
    vkDestroyBuffer(d->device, vertexBuffer, nullptr);
    vkFreeMemory(d->device, vertexMemory, nullptr);

    for (size_t i = 0; i < submeshes.size(); i++)
    {
        vkDestroyBuffer(d->device, submeshes[i].indexBuffer, nullptr);
        vkFreeMemory(d->device, submeshes[i].indexMemory, nullptr);
    }
}