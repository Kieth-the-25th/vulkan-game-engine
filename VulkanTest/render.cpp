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
#include <set>
#include <iostream>

#ifdef NDEBUG
const bool enableValidationLayers = false;
#else
const bool enableValidationLayers = true;
#endif

const std::vector<const char*> validationLayers = {
    "VK_LAYER_KHRONOS_validation"
};

const std::vector<const char*> deviceExtensions = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME
};

using namespace render;

Drawer::Drawer() {
    createInstance(this);

    if (glfwCreateWindowSurface(instance, window, nullptr, &surface) != VK_SUCCESS) {
        throw std::runtime_error("failed to create window surface!");
    }

    pickDevice(this);
    createLogicalDevice();
    createSwapchain();
    createImageViews();
    createRenderPass();
    createDescSetLayout();
    createGraphicsPipeline();
    createCommandPool();
    createDepthStuff();
    createFramebuffers();
    createTexture();
    createTextureImageView();
    createTextureSampler();
    createVertexBuffer();
    createUniformBuffers();
    createDescriptorPool();
    createDescriptorSets();
    createCommandBuffers();
    createSyncObjects();
}

void initWindow(render::Drawer* d) {
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

    d->window = glfwCreateWindow(d->WIDTH, d->HEIGHT, "Vulkan", nullptr, nullptr);
    glfwSetWindowUserPointer(d->window, d);
    glfwSetFramebufferSizeCallback(d->window, resizeCallback);
}

static void resizeCallback(GLFWwindow* window, int w, int h) {
    auto app = reinterpret_cast<Drawer*>(glfwGetWindowUserPointer(window));
    app->framebufferResized = true;
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

inline void createInstance(render::Drawer* d) {
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

inline void pickDevice(render::Drawer* d) {
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
        if (isSuitable(device)) {
            choice = device;
            break;
        }
    }

    if (choice == VK_NULL_HANDLE) {
        throw std::runtime_error("failed to find a suitable GPU!");
    }

    d->physicalDevice = choice;
}

inline bool isSuitable(VkPhysicalDevice d) {
    bool completeQueues = getQueueFamilies(d).isComplete();
    bool extensionSupport = checkExtensionSupport(d);
    bool swapchainSupport = false;
    if (extensionSupport) {
        SwapchainSupportDetails ssd = getSwapchainSupportDetails(d);
        swapchainSupport = !ssd.formats.empty() && !ssd.modes.empty();
    }

    VkPhysicalDeviceFeatures supportedFeatures;
    vkGetPhysicalDeviceFeatures(d, &supportedFeatures);
    return completeQueues && extensionSupport && swapchainSupport && supportedFeatures.samplerAnisotropy;
}

inline void createLogicalDevice(render::Drawer* d) {
    RequiredFamilyIndices indices = getQueueFamilies(d);

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

inline SwapchainSupportDetails getSwapchainSupportDetails(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface) {
    SwapchainSupportDetails r{};

    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, &r.capabilities);

    uint32_t formatCount = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, nullptr);

    if (formatCount != 0) {
        r.formats.resize(formatCount);
        vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, r.formats.data());
    }

    uint32_t modeCount = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &modeCount, nullptr);

    if (modeCount != 0) {
        r.modes.resize(modeCount);
        vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &modeCount, r.modes.data());
    }

    return r;
}

inline RequiredFamilyIndices getQueueFamilies(render::Drawer* d) {
    RequiredFamilyIndices indices{};

    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(d->physicalDevice, &queueFamilyCount, nullptr);

    if (queueFamilyCount == 0) std::runtime_error("bruh");

    std::vector<VkQueueFamilyProperties> properties(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(d->physicalDevice, &queueFamilyCount, properties.data());

    uint16_t i = 0;
    for (const auto& property : properties)
    {
        if (property.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            indices.graphicsFamily = i;
        }
        VkBool32 presentSupported = false;
        vkGetPhysicalDeviceSurfaceSupportKHR(d->physicalDevice, i, d->surface, &presentSupported);
        if (presentSupported) indices.presentFamily = i;
        i++;
    }

    return indices;
}

inline bool checkExtensionSupport(VkPhysicalDevice d) {
    uint32_t extensionCount = 0;
    vkEnumerateDeviceExtensionProperties(d, nullptr, &extensionCount, nullptr);

    std::vector<VkExtensionProperties> supportedExtensions(extensionCount);
    vkEnumerateDeviceExtensionProperties(d, nullptr, &extensionCount, supportedExtensions.data());

    for (const char* required : deviceExtensions)
    {
        bool found = false;
        for (const auto& extension : supportedExtensions) {
            if (strcmp(required, extension.extensionName)) found = true;
        }
        if (!found) return false;
    }
    return true;
}

inline void populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo) {
    createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    createInfo.pfnUserCallback = debugCallback;
}

inline bool checkValidationLayerSupport() {
    uint32_t layerCount;
    vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

    std::vector<VkLayerProperties> availableLayers(layerCount);
    vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

    for (const char* layerName : validationLayers) {
        bool layerFound = false;

        for (const auto& layerProperties : availableLayers) {
            if (strcmp(layerName, layerProperties.layerName) == 0) {
                layerFound = true;
                break;
            }
        }

        if (!layerFound) {
            return false;
        }
    }

    return true;
}


void Drawer::beginFrame() {
    vkWaitForFences(device, 1, &frameObjects[currentFrame].fence, true, UINT64_MAX);

    VkResult result = vkAcquireNextImageKHR(device, presentSwapchain, UINT64_MAX, frameObjects[currentFrame].imageAvailable, VK_NULL_HANDLE, &currentSwapchainIndex);

    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        recreateSwapchain();
        return;
    }
    else if (result != VK_SUBOPTIMAL_KHR && result != VK_SUCCESS) {
        throw std::runtime_error("Error fetching swapchain image");
    }

    vkResetFences(device, 1, &frameObjects[currentFrame].fence);

    vkResetCommandBuffer(frameObjects[currentFrame].frameCommandBuffer, 0);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

    if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) {
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

void Drawer::beginBatch() {

}

/* Record the command buffer with all the draw commands */
void Drawer::draw() {

}
void Drawer::draw(Sprite* s) {

}
void Drawer::draw(Mesh* m, Material* mat, glm::mat4 modelMatrix) {
    if (mat->pipeline != currentMaterial.pipeline) {
        currentMaterial = *mat;
        vkCmdBindPipeline(frameOrder[currentFrame].frameCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, currentMaterial.pipeline);
        vkCmdBindDescriptorSets(frameOrder[currentFrame].frameCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, currentMaterial.layout, 0, 1, &descSets[currentFrame], 0, nullptr);
    }

    VkBuffer vertBuffers[] = { m->vertexBuffer };
    VkDeviceSize offsets[] = { 0 };
    vkCmdBindVertexBuffers(frameOrder[currentFrame].frameCommandBuffer, 0, 1, vertBuffers, offsets);
    vkCmdBindIndexBuffer(frameOrder[currentFrame].frameCommandBuffer, m->indexBuffer, 0, VK_INDEX_TYPE_UINT16);
    vkCmdDrawIndexed(frameOrder[currentFrame].frameCommandBuffer, static_cast<uint32_t>(m->iBufferSize), 1, 0, 0, 0);
}
void Drawer::submitDraws() {
    vkCmdEndRenderPass(frameOrder[currentFrame].frameCommandBuffer);

    if (vkEndCommandBuffer(frameOrder[currentFrame].frameCommandBuffer) != VK_SUCCESS) {
        throw std::runtime_error("failed to record command buffer!");
    }

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

    VkSemaphore waitSemaphores[] = { frameObjects[currentFrame].imageAvailable };
    VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = waitSemaphores;
    submitInfo.pWaitDstStageMask = waitStages;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &frameObjects[currentFrame].frameCommandBuffer;

    VkSemaphore signalSemaphores[] = { frameObjects[currentFrame].imageFinished };
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = signalSemaphores;

    if (vkQueueSubmit(graphicsQueue, 1, &submitInfo, frameObjects[currentFrame].fence) != VK_SUCCESS) {
        throw std::runtime_error("failed to submit draw command buffer!");
    }
}

void Drawer::endFrame() {
    VkPresentInfoKHR presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;

    VkSemaphore signalSemaphores[] = { frameObjects[currentFrame].imageFinished };
    presentInfo.pWaitSemaphores = signalSemaphores;

    VkSwapchainKHR swapChains[] = { presentSwapchain };
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = swapChains;
    presentInfo.pImageIndices = &currentSwapchainIndex;
    presentInfo.pResults = nullptr; // Optional

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

}

void Mesh::copyToDeviceLocal(const render::Drawer* d, const glm::vec3* v, const int* i, uint32_t vcount, uint32_t icount) {
    util::staging vstaging(d->device, d->physicalDevice, d->graphicsQueue);
    vstaging.transfer(VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, vcount, v, &(Mesh::vertexBuffer), &(d->commandPool));

    util::staging istaging(d->device, d->physicalDevice, d->graphicsQueue);
    istaging.transfer(VK_BUFFER_USAGE_INDEX_BUFFER_BIT, icount, i, &(Mesh::indexBuffer), &(d->commandPool));
}