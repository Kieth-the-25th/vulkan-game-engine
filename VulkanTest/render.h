#pragma once

#include <vulkan/vulkan.h>

#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "util.h"
#include <optional>
#include <vector>

namespace render
{
    struct SwapchainSupportDetails {
        VkSurfaceCapabilitiesKHR capabilities;
        std::vector<VkSurfaceFormatKHR> formats;
        std::vector<VkPresentModeKHR> modes;
    };

    /* A struct for each image of the swapchain. Images from the swapchain could be retrieved out of order. */
    struct Frame {
    public:
        VkImage swapchainImage;
        VkImageView swapchainImageView;
        VkFramebuffer framebuffer;
    };

    /* A struct for each frame being rendered, does not correspond to the swapchain image of the same index.
     * Used for multiple frames in flight.
     */
    struct FrameOrderEntry {
    public:
        VkCommandBuffer frameCommandBuffer;
        VkSemaphore imageFinished;
        VkSemaphore imageAvailable;
        VkFence fence;
    };

    /* tutorial struct */
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

    struct PushConstant {
        glm::mat4 model;
        glm::vec4 data;
    };

    class Camera {
        VkSwapchainKHR target;
    };

    class Material {
    public:
        VkDescriptorSet set;
        VkPipeline pipeline;
        VkPipelineLayout layout;
    };

    class Drawer;

    class Mesh {
    public:
        VkDeviceSize vBufferSize;
        VkBuffer vertexBuffer;
        VkDeviceMemory vertexMemory;

        VkDeviceSize iBufferSize;
        VkBuffer indexBuffer;
        VkDeviceMemory indexMemory;

        Mesh();

        void copyToDeviceLocal(const Drawer* d, const glm::vec3* v, const int* i, uint32_t vcount, uint32_t icount);
    };

    class Sprite {

    };

    class Drawer {
    public:
        const uint32_t WIDTH = 1600;
        const uint32_t HEIGHT = 1200;
        const uint32_t FRAMES_IN_FLIGHT = 2;

        std::vector<VkImage> swapchainImages;
        std::vector<VkImageView> imageViews;
        std::vector<VkFramebuffer> framebuffers;
        std::vector<FrameObjects> frameObjects;

        std::vector<Frame> frames;
        std::vector<FrameOrderEntry> frameOrder;
        uint32_t currentFrame = 0;
        uint32_t currentSwapchainIndex = 0;
        bool framebufferResized;

        Material currentMaterial;

        glm::vec3 cameraPos = glm::vec3(2.0, 2.0, 2.0);

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

        Drawer();

        /* Images */

        VkImage depthImage;
        VkImageView depthView;

        void loadImage();

        /* Buffers */

        std::vector<Mesh> registeredMeshes;
        std::vector<render::Material> registeredMaterials;

        void loadMesh();
        void loadMaterial();

        /* Memory */

        /* Draw Functions */

        void beginFrame();

        void draw();
        void draw(Sprite* m);
        void draw(Mesh* m, Material* mat, glm::mat4 modelMatrix);

        void beginBatch();
        void submitDraws();

        void endFrame();

        /* Util Functions */
        void recreateSwapchain();
        void createBuffer(VkDeviceSize size, VkBufferUsageFlags flags, VkMemoryPropertyFlags memFlags, VkBuffer& buffer, VkDeviceMemory& memory);
        void createImage(uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImage& image, VkDeviceMemory& imageMemory);
        uint32_t findMemoryType(uint32_t filter, VkMemoryPropertyFlags flags);
    };
};