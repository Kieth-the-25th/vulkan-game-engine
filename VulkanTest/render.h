#pragma once
#include <vulkan/vulkan.h>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include <chrono>

#include <iostream>
#include <fstream>
#include <stdexcept>
#include <cstdlib>
#include <cstdint>
#include <algorithm>
#include <optional>
#include <vector>
#include <string>
#include <cstring>
#include <set>
#include "util.h"

struct SwapchainSupportDetails {
    VkSurfaceCapabilitiesKHR capabilities;
    std::vector<VkSurfaceFormatKHR> formats;
    std::vector<VkPresentModeKHR> modes;
};

struct Frame {
public:
    VkImage swapchainImage;
    VkImageView swapchainImageView;
    VkFramebuffer framebuffer;
    VkCommandBuffer frameCommandBuffer;
    VkSemaphore imageFinished;
    VkSemaphore imageAvailable;
    VkFence fence;
};

struct FrameObjects {
public:
    VkCommandBuffer frameCommandBuffer;
    VkSemaphore imageFinished;
    VkSemaphore imageAvailable;
    VkFence fence;
};

struct UniformBufferObject {
    glm::mat4 model;
    glm::mat4 view;
    glm::mat4 proj;
};

struct RequiredFamilyIndices {
    std::optional<uint32_t> graphicsFamily;
    std::optional<uint32_t> presentFamily;

public:
    bool isComplete() { return (graphicsFamily.has_value() && presentFamily.has_value()); }
};

namespace render
{
    const uint32_t WIDTH = 1600;
    const uint32_t HEIGHT = 1200;
    const uint32_t FRAMES_IN_FLIGHT = 2;

    std::vector<VkImage> swapchainImages;
    std::vector<VkImageView> imageViews;
    std::vector<VkFramebuffer> framebuffers;
    std::vector<FrameObjects> frameObjects;

    std::vector<Frame> frames;
    uint32_t currentFrame = 0;
    bool framebufferResized;

    glm::vec3 cameraPos = glm::vec3(2.0, 2.0, 2.0);

    VkBuffer stagingBuffer;
    VkDeviceMemory stagingMemory;

    VkBuffer vertexBuffer;
    VkDeviceMemory vertexMemory;

    VkBuffer indexBuffer;
    VkDeviceMemory indexMemory;

    VkImage texture;
    VkImageView textureImgView;
    VkDeviceMemory texMemory;
    VkSampler texSampler;


    VkDeviceMemory depthMemory;
    VkSampler depthSampler;

    std::vector<VkBuffer> uniformBuffers;
    std::vector<VkDeviceMemory> uniformMemory;
    std::vector<void*> mappedMemory;
    std::vector<VkDescriptorSet> descSets;
    VkDescriptorPool descPool;

    VkFormat format;
    VkExtent2D extent;


    VkCommandBuffer commandBuffer;

    /* Vk & GLFW Objects*/
    VkInstance instance;

    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    VkDevice device;

    VkQueue graphicsQueue;

    GLFWwindow* window;
    VkSurfaceKHR surface;
    VkQueue presentQueue;

    
    VkDebugUtilsMessengerEXT debugMessenger;

    VkSwapchainKHR presentSwapchain;
    VkRenderPass renderPass;
    VkDescriptorSetLayout descSetLayout;
    VkCommandPool commandPool;

	/* Images */

    VkImage depthImage;
    VkImageView depthView;

	/* Buffers */

	/* Memory */

    /* Draw Functions */

    void draw();
    void draw(renderobject::Sprite* m);
    void draw(renderobject::Mesh* m);
    
    /* Util Functions */
    void createBuffer(VkDeviceSize size, VkBufferUsageFlags flags, VkMemoryPropertyFlags memFlags, VkBuffer& buffer, VkDeviceMemory& memory);
    void createImage(uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImage& image, VkDeviceMemory& imageMemory);
    uint32_t findMemoryType(uint32_t filter, VkMemoryPropertyFlags flags);
};

namespace renderobject {
    class Camera {
        VkSwapchainKHR target;
    };

    class Material {
        VkDescriptorSet set;
        VkPipeline pipeline;
        VkPipelineLayout layout;
    };

    class Mesh {
    public:
        VkDeviceSize vBufferSize;
        VkBuffer vertexBuffer;
        VkDeviceMemory vertexMemory;

        VkDeviceSize iBufferSize;
        VkBuffer indexBuffer;
        VkDeviceMemory indexMemory;

        void copyToDeviceLocal(const glm::vec3* v, const int* i, uint32_t vcount, uint32_t icount);
    };

    class Sprite {

    };
}