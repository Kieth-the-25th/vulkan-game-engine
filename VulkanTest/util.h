#pragma once

#include <stdexcept>
#include <cstdlib>
#include <iostream>
#include <fstream>
#include <vector>
#include <vulkan/vulkan.h>

/*
 * Various utility functions.
 * Memory management, math, vulkan config
 */

inline static VkCommandBuffer beginSimpleCommands(VkDevice device, VkCommandPool commandPool) {
    VkCommandBuffer buffer{};

    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = commandPool;
    allocInfo.commandBufferCount = 1;

    vkAllocateCommandBuffers(device, &allocInfo, &buffer);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    vkBeginCommandBuffer(buffer, &beginInfo);

    return buffer;
}

inline void endSimpleCommands(VkCommandBuffer buffer, VkQueue queue) {
    vkEndCommandBuffer(buffer);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &buffer;

    vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(queue);
}

inline void copyBuffer(VkDevice device, VkQueue queue, VkCommandPool pool, VkBuffer src, VkBuffer dst, VkDeviceSize size) {
    VkCommandBuffer buffer = beginSimpleCommands(device, pool);

    VkBufferCopy copyRegion{};
    copyRegion.srcOffset = 0; // Optional
    copyRegion.dstOffset = 0; // Optional
    copyRegion.size = size;
    vkCmdCopyBuffer(buffer, src, dst, 1, &copyRegion);

    endSimpleCommands(buffer, queue);
}

inline uint32_t findMemoryType(uint32_t filter, VkPhysicalDevice physicalDevice, VkMemoryPropertyFlags flags) {
    VkPhysicalDeviceMemoryProperties physicalProperties;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &physicalProperties);

    for (size_t i = 0; i < physicalProperties.memoryTypeCount; i++)
    {
        if (filter & (1 << i) && (physicalProperties.memoryTypes[i].propertyFlags == flags)) return i;
    }

    throw std::runtime_error("Error finding memory type");
    return 0;
}

inline void createBuffer(VkDevice device, VkPhysicalDevice physicalDevice, VkDeviceSize size, VkBufferUsageFlags flags, VkMemoryPropertyFlags memFlags, VkBuffer& buffer, VkDeviceMemory& memory) {
    VkBufferCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    info.size = size;
    info.usage = flags;
    info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateBuffer(device, &info, nullptr, &buffer) != VK_SUCCESS) {
        std::runtime_error("Error creating buffer");
    }

    VkMemoryRequirements requirements;
    vkGetBufferMemoryRequirements(device, buffer, &requirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = requirements.size;
    allocInfo.memoryTypeIndex = findMemoryType(requirements.memoryTypeBits, physicalDevice, memFlags);

    if (vkAllocateMemory(device, &allocInfo, nullptr, &memory) != VK_SUCCESS) {
        std::runtime_error("Error allocating GPU memory");
    }

    vkBindBufferMemory(device, buffer, memory, 0);
}

inline void destroyBuffer(VkDevice device, VkBuffer buffer, VkDeviceMemory memory) {
    vkDestroyBuffer(device, buffer, nullptr);
    vkFreeMemory(device, memory, nullptr);
}

inline void createImage(VkDevice device, VkPhysicalDevice physicalDevice, uint32_t width, uint32_t height, uint32_t arrayLayers, VkFormat format, VkImageTiling tiling, VkImageCreateFlags createFlags, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImage& image, VkDeviceMemory& imageMemory) {
    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width = width;
    imageInfo.extent.height = height;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.format = format;
    imageInfo.tiling = tiling;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = usage;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageInfo.arrayLayers = arrayLayers;
    imageInfo.flags = createFlags;

    if (vkCreateImage(device, &imageInfo, nullptr, &image) != VK_SUCCESS) {
        throw std::runtime_error("failed to create image!");
    }

    VkMemoryRequirements memRequirements;
    vkGetImageMemoryRequirements(device, image, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, physicalDevice, properties);

    if (vkAllocateMemory(device, &allocInfo, nullptr, &imageMemory) != VK_SUCCESS) {
        throw std::runtime_error("failed to allocate image memory!");
    }

    vkBindImageMemory(device, image, imageMemory, 0);
}


inline static std::vector<char> getBytes(const char* fileName) {
    std::ifstream fileStream(fileName, std::ios::ate | std::ios::binary);

    if (!fileStream.is_open()) {
        throw std::runtime_error("Could not open file");
    }

    size_t fileSize = fileStream.tellg();
    std::cout << "File size: " << fileSize << "\n";

    std::vector<char> r(fileSize);

    fileStream.seekg(0);
    fileStream.read(r.data(), fileSize);
    fileStream.close();

    return r;
}

namespace util {
    class staging {
    private:
        VkDevice device;
        VkPhysicalDevice physicalDevice;
        VkQueue transferQueue;
        VkBuffer stagingBuffer;
        VkDeviceMemory stagingMemory;

    public:
        staging(VkDevice device, VkPhysicalDevice physicalDevice, VkQueue queue) {
            this->device = device;
            this->physicalDevice = physicalDevice;
            this->transferQueue = queue;
        }

        void transfer(VkDeviceSize size, const void* src, VkBuffer* dst, const VkCommandPool* commandPool) {
            createBuffer(device, physicalDevice, size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingMemory);

            void* stage;
            vkMapMemory(device, stagingMemory, 0, size, 0, &stage);
            memcpy(stage, src, static_cast<size_t>(size));
            vkUnmapMemory(device, stagingMemory);

            copyBuffer(device, transferQueue, *commandPool, stagingBuffer, *dst, size);

            destroyBuffer(device, stagingBuffer, stagingMemory);
        }
    };
};