#pragma once

#include <stdexcept>
#include <cstdlib>
#include <cstdint>
#include <vulkan/vulkan.h>

/*
 * Various utility functions.
 * Memory management, math, vulkan config
 */

inline VkCommandBuffer beginSimpleCommands(VkDevice device, VkCommandPool commandPool) {
    VkCommandBuffer buffer;

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

        void transfer(uint32_t usageFlags, VkDeviceSize size, const void* src, VkBuffer* dst, const VkCommandPool* commandPool) {
            createBuffer(device, physicalDevice, size, usageFlags, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingMemory);

            void* stage;
            vkMapMemory(device, stagingMemory, 0, size, 0, &stage);
            memcpy(stage, src, static_cast<size_t>(size));
            vkUnmapMemory(device, stagingMemory);

            copyBuffer(device, transferQueue, *commandPool, stagingBuffer, *dst, size);

            destroyBuffer(device, stagingBuffer, stagingMemory);
        }
    };
};