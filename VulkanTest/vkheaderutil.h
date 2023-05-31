#pragma once

#include <render.h>
#include <vulkan/vulkan.h>

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

inline RequiredFamilyIndices getQueueFamilies(VkPhysicalDevice physicalDevice) {
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