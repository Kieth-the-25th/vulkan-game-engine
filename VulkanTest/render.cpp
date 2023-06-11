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

#include "vkheaderutil.h"

#include "stb_image.h"

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
    createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

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
        d->frames[i].swapchainImageView = createImageView(d->device, d->frames[i].swapchainImage, d->format, VK_IMAGE_ASPECT_COLOR_BIT);
    }
}

inline void createFramebuffers(Drawer* d) {
    for (uint32_t i = 0; i < d->frames.size(); i++)
    {
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

    VkSubpassDependency dependency{};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.srcAccessMask = 0;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    VkAttachmentDescription attachments[2] = { colorAttachment, depthAttachment };
    VkRenderPassCreateInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = 2;
    renderPassInfo.pAttachments = attachments;
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;
    renderPassInfo.dependencyCount = 1;
    renderPassInfo.pDependencies = &dependency;

    if (vkCreateRenderPass(d->device, &renderPassInfo, nullptr, &(d->renderPass)) != VK_SUCCESS) {
        throw std::runtime_error("Error creating render pass");
    }
}

inline void createDescriptorPool(Drawer* d) {
    std::vector<VkDescriptorPoolSize> poolSizes{};
    poolSizes.resize(2);
    poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSizes[0].descriptorCount = static_cast<uint32_t>(10);
    poolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSizes[1].descriptorCount = static_cast<uint32_t>(10);

    VkDescriptorPoolCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    info.maxSets = 20;
    info.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
    info.pPoolSizes = poolSizes.data();

    if (vkCreateDescriptorPool(d->device, &info, nullptr, &(d->descPool)) != VK_SUCCESS) {
        std::runtime_error("Error creating descriptor pool");
    }
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


inline void createFrameDescriptorSets(Drawer* d) {
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

        std::vector<VkWriteDescriptorSet> descriptorWrites{};
        descriptorWrites.resize(1);

        descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[0].dstSet = descriptors[i];
        descriptorWrites[0].dstBinding = 0;
        descriptorWrites[0].dstArrayElement = 0;
        descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        descriptorWrites[0].descriptorCount = 1;
        descriptorWrites[0].pBufferInfo = &bInfo;

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
    imgInfo.imageView = tex.imgView;
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
    render::Texture texture(d, "textures/texture.bmp");

    createDefaultDescSetLayout(d);
    material.descriptorLayout = d->defaultMaterialLayout;
    createDefaultDescriptorSet(d, &material, texture);

    std::vector<char> shaders[] = {getBytes("shaders/vert.spv"), getBytes("shaders/frag.spv")};
    createGraphicsPipeline(d, shaders, &material);
    d->registeredTextures.push_back(texture);
    d->registeredMaterials.push_back(material);
}

inline void createDefaultMesh(Drawer* d) {
    Mesh mesh;
    mesh.copyToDeviceLocal(d, d->defaultBox.data(), d->defaultIndices.data(), d->defaultBox.size(), d->defaultIndices.size());
    d->registeredMeshes.push_back(mesh);
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
    createImage(d->device, d->physicalDevice, d->extent.width, d->extent.height, depthFormat, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, d->depthImage, d->depthMemory);
    d->depthView = createImageView(d->device, d->depthImage, depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT);
}


inline void createUniformBuffers(Drawer* d) {
    VkDeviceSize size = sizeof(UniformBufferObject);

    //d->uniformBuffers.resize(d->FRAMES_IN_FLIGHT);
    //d->uniformMemory.resize(d->FRAMES_IN_FLIGHT);
    //d->mappedMemory.resize(d->FRAMES_IN_FLIGHT);

    for (size_t i = 0; i < d->FRAMES_IN_FLIGHT; i++)
    {
        createBuffer(d->device, d->physicalDevice, size, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, d->frameOrder[i].uniformBuffer, d->frameOrder[i].uniformBufferMemory);
        vkMapMemory(d->device, d->frameOrder[i].uniformBufferMemory, 0, size, 0, &(d->frameOrder[i].uniformMappedMemory));
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


inline void test(Drawer* d) {
    VkBuffer b1;
    VkDeviceMemory m1;
    VkBuffer b2;
    VkDeviceMemory m2;
    VkBuffer b3;
    VkDeviceMemory m3;

    createBuffer(d->device, d->physicalDevice, sizeof(uint64_t), VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, b1, m1);
    createBuffer(d->device, d->physicalDevice, sizeof(uint64_t), VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, b2, m2);

    util::staging stage{d->device, d->physicalDevice, d->graphicsQueue};
    uint64_t number = 69;
    stage.transfer(sizeof(uint64_t), &number, &b2, &(d->commandPool));
    copyBuffer(d->device, d->graphicsQueue, d->commandPool, b2, b1, sizeof(uint64_t));

    void* map;
    vkMapMemory(d->device, m1, 0, sizeof(uint64_t), 0, &map);

    void* map2;
    vkMapMemory(d->device, m2, 0, sizeof(uint64_t), 0, &map2);

    std::cout << number << "\n";
    std::cout << *(reinterpret_cast<uint64_t*>(map)) << "\n";
    std::cout << *(reinterpret_cast<uint64_t*>(map2)) << "\n";
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
    createDepthStuff(this);
    createFramebuffers(this);
    std::cout << "Creating buffers and pools...\n";
    createCommandPool(this);
    createDescriptorPool(this);
    createUniformBuffers(this);
    createCommandBuffers(this);
    createFrameDescriptorSets(this);
    std::cout << "Creating debug objects...\n";
    createDefaultMaterial(this);
    createDefaultMesh(this);
    std::cout << "Creating sync objects...\n";
    createSyncObjects(this);

    test(this);
}

void Drawer::beginFrame(glm::mat4 cameraView, double FOV) {
    UniformBufferObject ubo{};
    ubo.view = cameraView;
    ubo.proj = glm::perspective(glm::radians(FOV), extent.width / (double)extent.height, 0.1, 10000.0);
    ubo.proj[1][1] *= -1;

    //ubo.view = glm::lookAt(cameraPos, glm::vec3(0, 0, 0), glm::vec3(0, 0, 1));
    //ubo.proj = glm::perspective(glm::radians(60.0), extent.width / (double)extent.height, 0.1, 10.0);
    memcpy(frameOrder[currentFrame].uniformMappedMemory, &ubo, sizeof(ubo));

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

    VkRenderPassBeginInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass = renderPass;
    renderPassInfo.framebuffer = frames[currentSwapchainIndex].framebuffer;

    renderPassInfo.renderArea.offset = { 0, 0 };
    renderPassInfo.renderArea.extent = extent;

    VkClearValue clearValues[2]{};
    clearValues[0].color = { 0.0f, 0.0f, 0.0f, 1.0f };
    clearValues[1].depthStencil = { 1.0f, 0 };
    renderPassInfo.clearValueCount = 2;
    renderPassInfo.pClearValues = clearValues;

    std::cout << "Beginning render pass...\n";

    vkCmdBeginRenderPass(frameOrder[currentFrame].frameCommandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = static_cast<float>(extent.width);
    viewport.height = static_cast<float>(extent.height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(frameOrder[currentFrame].frameCommandBuffer, 0, 1, &viewport);

    VkRect2D scissor{};
    scissor.offset = { 0, 0 };
    scissor.extent = extent;
    vkCmdSetScissor(frameOrder[currentFrame].frameCommandBuffer, 0, 1, &scissor);
}

/* Record the command buffer with all the draw commands */
void Drawer::draw() {

}
void Drawer::draw(Sprite* s) {

}
void Drawer::draw(Mesh* m, Material* mat, glm::mat4 modelMatrix) {
    if (frameOrder[currentFrame].boundMaterial == nullptr || mat != frameOrder[currentFrame].boundMaterial) {
        //Check not working
        std::cout << "Binding new material...\n";
        frameOrder[currentFrame].boundMaterial = mat;
        vkCmdBindPipeline(frameOrder[currentFrame].frameCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, mat->pipeline);
        std::vector<VkDescriptorSet> sets = { frameOrder[currentFrame].frameDescSet, (mat->materialDescriptor)};
        vkCmdBindDescriptorSets(frameOrder[currentFrame].frameCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, mat->layout, 0, sets.size(), sets.data(), 0, nullptr);
    }
    std::cout << "Beginning draw...\n";

    VkBuffer vertBuffers[] = { m->vertexBuffer };
    VkDeviceSize offsets[] = { 0 };
    PushConstant push{};
    push.model = modelMatrix;

    //std::cout << modelMatrix[0][0] << " " << modelMatrix[1][0] << " " << modelMatrix[2][0] << " " << modelMatrix[3][0] << "\n";
    //std::cout << modelMatrix[0][1] << " " << modelMatrix[1][1] << " " << modelMatrix[2][1] << " " << modelMatrix[3][1] << "\n";
    //std::cout << modelMatrix[0][2] << " " << modelMatrix[1][2] << " " << modelMatrix[2][2] << " " << modelMatrix[3][2] << "\n";
    //std::cout << modelMatrix[0][3] << " " << modelMatrix[1][3] << " " << modelMatrix[2][3] << " " << modelMatrix[3][3] << "\n";

    vkCmdBindVertexBuffers(frameOrder[currentFrame].frameCommandBuffer, 0, 1, vertBuffers, offsets);
    vkCmdBindIndexBuffer(frameOrder[currentFrame].frameCommandBuffer, m->indexBuffer, 0, VK_INDEX_TYPE_UINT16);
    vkCmdPushConstants(frameOrder[currentFrame].frameCommandBuffer, mat->layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(PushConstant), &push);
    vkCmdDrawIndexed(frameOrder[currentFrame].frameCommandBuffer, static_cast<uint32_t>(m->iBufferSize), 1, 0, 0, 0);
    //Pipeline not being bound?
}
/* Submit the recorded command buffer */
void Drawer::submitDraws() {
    vkCmdEndRenderPass(frameOrder[currentFrame].frameCommandBuffer);

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
    std::cout << "bruh\n";
}

render::Texture::Texture(Drawer* d, const char* dir) {
    int width, height;

    int texChannels;
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

    createImage(d->device, d->physicalDevice, width, height, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, imgBuffer, imgMemory);

    transitionImageLayout(d->device, d->graphicsQueue, d->commandPool, imgBuffer, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    VkCommandBuffer cmdBuffer = beginSimpleCommands(d->device, d->commandPool);

    VkBufferImageCopy region{};
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;
    region.bufferOffset = 0;
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;
    region.imageExtent = { static_cast<uint32_t>(width), static_cast<uint32_t>(height), 1 };
    region.imageOffset = { 0, 0, 0 };

    vkCmdCopyBufferToImage(cmdBuffer, staging, imgBuffer, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

    endSimpleCommands(cmdBuffer, d->graphicsQueue);
    transitionImageLayout(d->device, d->graphicsQueue, d->commandPool, imgBuffer, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    vkDestroyBuffer(d->device, staging, nullptr);
    vkFreeMemory(d->device, memory, nullptr);

    imgView = createImageView(d->device, imgBuffer, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_ASPECT_COLOR_BIT);

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

render::Material::Material() {

}

render::Mesh::Mesh() {

}

void render::Mesh::copyToDeviceLocal(const Drawer* d, const Vertex* v, const uint16_t* i, uint32_t vcount, uint32_t icount) {
    createBuffer(d->device, d->physicalDevice, vcount * sizeof(Vertex), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, vertexBuffer, vertexMemory);
    createBuffer(d->device, d->physicalDevice, icount * sizeof(uint16_t), VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, indexBuffer, indexMemory);

    vBufferSize = vcount;
    iBufferSize = icount;

    util::staging vstaging(d->device, d->physicalDevice, d->graphicsQueue);
    vstaging.transfer(vcount * sizeof(Vertex), v, &(Mesh::vertexBuffer), &(d->commandPool));

    util::staging istaging(d->device, d->physicalDevice, d->graphicsQueue);
    istaging.transfer(icount * sizeof(uint16_t), i, &(Mesh::indexBuffer), &(d->commandPool));

    void* map;
    vkMapMemory(d->device, indexMemory, 0, icount * sizeof(uint16_t), 0, &map);
    for (size_t i = 0; i < icount; i++)
    {
        std::cout << reinterpret_cast<uint16_t*>(map)[i] << "\n";
    }
    vkUnmapMemory(d->device, indexMemory);
}