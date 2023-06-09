#pragma once

#include <vulkan/vulkan.h>

#include <GLFW/glfw3.h>

#include <glm/glm.hpp>

#include <optional>
#include <vector>

#define KHRONOS_STATIC
#include "ktxvulkan.h"

namespace render
{
    class Drawer;

    struct DeviceBuffer {
        VkBuffer buffer;
        VkDeviceMemory memory;
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
        glm::mat4 view;
        glm::mat4 proj;
    };

    struct LightBufferObject {
        glm::mat4 view;
        glm::mat4 proj;
        glm::vec4 color;
    };

    struct PushConstant {
        glm::mat4 model;
        glm::vec4 data;
    };

    class Camera {
        VkSwapchainKHR target;
    };

    struct Vertex {
        glm::vec3 pos;
        glm::vec3 color;
        glm::vec2 texCoord;

        static VkVertexInputBindingDescription getBindingDescription() {
            VkVertexInputBindingDescription bindingDescription{};
            bindingDescription.binding = 0;
            bindingDescription.stride = sizeof(Vertex);
            bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

            return bindingDescription;
        }

        static std::vector<VkVertexInputAttributeDescription> getAttributeDescriptions() {
            std::vector<VkVertexInputAttributeDescription> attributeDescriptions{};
            attributeDescriptions.resize(3);

            VkVertexInputAttributeDescription a{};

            a.binding = 0;
            a.location = 0;
            a.format = VK_FORMAT_R32G32B32_SFLOAT;
            a.offset = offsetof(Vertex, pos);
            attributeDescriptions[0] = a;

            VkVertexInputAttributeDescription b{};

            b.binding = 0;
            b.location = 1;
            b.format = VK_FORMAT_R32G32B32_SFLOAT;
            b.offset = offsetof(Vertex, color);
            attributeDescriptions[1] = b;

            VkVertexInputAttributeDescription c{};

            c.binding = 0;
            c.location = 2;
            c.format = VK_FORMAT_R32G32_SFLOAT;
            c.offset = offsetof(Vertex, texCoord);
            attributeDescriptions[2] = c;

            return attributeDescriptions;
        }
    };

    class Material {
    public:
        Material();

        VkDescriptorSet materialDescriptor;
        VkDescriptorSetLayout descriptorLayout;
        VkPipeline pipeline;
        VkPipelineLayout layout;
    };

    class AnimMaterial {
    public:
        //AnimMaterial();

        std::vector<VkDescriptorSet> materialDescriptors;
        VkDescriptorSetLayout descriptorLayout;
        VkPipeline pipeline;
        VkPipelineLayout layout;
    };

    class Texture {
    public:
        Texture(Drawer* d, const char* dir, VkImageViewType imageType);

        ktxVulkanTexture vkTexture;
        ktxTexture2 texture;
        VkImageView textureView;
        VkSampler sampler;
    };

    /* unused */
    class TextureCube {
        TextureCube(Drawer* d, const char* dir);

        ktxVulkanTexture vkTexture;
        ktxTexture texture;
    };

    class Submesh {
    public:
        VkDeviceSize iBufferSize;
        VkBuffer indexBuffer;
        VkDeviceMemory indexMemory;
        uint16_t materialIndex;

        struct SubmeshCreateInfo {
            const uint16_t* indices;
            uint32_t count;
            uint16_t materialIndex;

            SubmeshCreateInfo(const uint16_t* indices, uint32_t count, uint16_t materialIndex);
        };

        Submesh();
        Submesh(const Drawer* d, SubmeshCreateInfo info);
    };

    class Mesh {
    public:
        VkDeviceSize vBufferSize;
        VkBuffer vertexBuffer;
        VkDeviceMemory vertexMemory;

        std::vector<Submesh> submeshes;

        Mesh(const Drawer* d, const Vertex* vertices, const uint32_t vcount, const std::vector<Submesh::SubmeshCreateInfo> createInfos);
    };

    class Sprite {

    };

    class Light {
    public:
        glm::mat4 transform;
        float FOV;
        VkImage shMap;
        VkImageView shMapView;
        VkSampler shMapSampler;
        VkDeviceMemory shMapMemory;

        bool isDynamic;
        std::vector<VkFramebuffer> frames;

        std::vector<DeviceBuffer> framePositions;
        std::vector<void*> mappedMemory;
        std::vector<VkDescriptorSet> frameSets;

        Light();
        Light(Drawer* d, glm::mat4 origin, float FOV, bool isDynamic);
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
        Material* boundMaterial;
        VkSemaphore imageFinished;
        VkSemaphore imageAvailable;
        VkFence fence;

        VkBuffer uniformBuffer;
        VkDeviceMemory uniformBufferMemory;
        VkBuffer baseLightBuffer;
        VkDeviceMemory baseLightBufferMemory;
        VkDescriptorSet frameDescSet;
        void* uniformMappedMemory;
        void* lightMappedMemory;
    };


    class Drawer {
    public:
        const uint32_t WIDTH = 256;//1600;
        const uint32_t HEIGHT = 256;//900;
        const uint32_t FRAMES_IN_FLIGHT = 2;

        //std::vector<VkImage> swapchainImages;
        //std::vector<VkImageView> imageViews;
        //std::vector<VkFramebuffer> framebuffers;
        //std::vector<FrameObjects> frameObjects;

        std::vector<Frame> frames;
        std::vector<FrameOrderEntry> frameOrder;
        uint32_t currentFrame = 0;
        uint32_t currentSwapchainIndex = 0;
        bool framebufferResized;

        Material currentMaterial;

        glm::vec3 cameraPos = glm::vec3(2.0, 2.0, 2.0);

        VkDeviceMemory depthMemory;
        //VkSampler depthSampler;

        VkDescriptorSetLayout frameDependantLayout;
        VkDescriptorSetLayout defaultMaterialLayout;
        VkDescriptorSetLayout shadowMappingLayout;
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

        VkImage testImage;
        VkDeviceMemory testImageMemory;

        VkDebugUtilsMessengerEXT debugMessenger;

        VkSwapchainKHR presentSwapchain;
        VkRenderPass renderPass;
        VkRenderPass shadowPass;
        VkPipeline shadowPipeline;
        VkPipelineLayout shadowPipelineLayout;
        Light* mainLight;

        std::vector<VkRenderPass> passes;
        VkDescriptorSetLayout descSetLayout;
        VkCommandPool commandPool;

        Drawer();
        void init();

        /* Images */

        VkImage depthImage;
        VkImageView depthView;
        VkSampler depthSampler;

        /* Buffers */

        /* Registers */

        ktxVulkanDeviceInfo* ktxVulkanInfo;
        std::vector<Mesh> registeredMeshes;
        std::vector<Light> registeredLights;
        std::vector<render::Material> registeredMaterials;
        std::vector<render::Texture> registeredTextures;

        void loadMesh(const char* dir, uint16_t* index, uint16_t materialIndex = 0);
        void loadMaterial();

        /* Memory */

        /* Draw Functions */

        /*const std::vector<Vertex> defaultBox = {
            {{0.5, 0.5, 0.5}, {1, 1, 1}, {0, 0}}, {{-0.5, 0.5, 0.5}, {1, 1, 0}, {0, 0}}, {{0.5, 0.5, -0.5}, {1, 1, 0}, {0, 0}}, {{-0.5, 0.5, -0.5}, {1, 1, 0}, {0, 0}},
            {{0.5, -0.5, 0.5}, {1, 1, 0}, {0, 0}}, {{-0.5, -0.5, 0.5}, {1, 1, 0}, {0, 0}}, {{0.5, -0.5, -0.5}, {1, 1, 0}, {0, 0}}, {{-0.5, -0.5, -0.5}, {1, 1, 0}, {0, 0}}
        };

        const std::vector<uint16_t> defaultIndices = {
            0, 1, 2,  3, 2, 1,
            1, 0, 5,  4, 5, 0,
            0, 2, 4,  6, 2, 4,
        };*/

        const std::vector<Vertex> defaultBox = {
            {{0.5f, 0.5f, 0.5f}, {1.0f, 0.0f, 0.0f}, {1.0f, 1.0f}},
            {{0.5f, -0.5f, 0.5f}, {1.0f, 1.0f, 1.0f}, {1.0f, 0.0f}},
            {{-0.5f, 0.5f, 0.5f}, {0.0f, 1.0f, 0.0f}, {0.0f, 1.0f}},
            {{-0.5f, -0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}, {0.0f, 0.0f}},

            {{0.5f, 0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}, {1.0f, 1.0f}},
            {{0.5f, -0.5f, -0.5f}, {1.0f, 1.0f, 1.0f}, {1.0f, 0.0f}},
            {{-0.5f, 0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}, {0.0f, 1.0f}},
            {{-0.5f, -0.5f, -0.5f}, {0.0f, 0.0f, 1.0f}, {0.0f, 0.0f}}
        };

        const std::vector<uint16_t> defaultIndices = {
            {1, 0, 2, 1, 2, 3,
            4, 5, 6, 6, 5, 7}
        };

        void beginFrame(glm::mat4 cameraView, double FOV, std::vector<glm::mat4> lightViews, std::vector<double> lightFOVs);
        void beginPass();
        void beginPass(VkFramebuffer frame, VkRenderPass pass, VkExtent2D ext);

        void bindShadowPassPipeline();

        void draw();
        void draw(Sprite* m);
        void draw(Mesh* m, Submesh* s, Material* mat, glm::mat4 modelMatrix, bool bindMaterial);

        void endPass();
        void submitDraws();

        void imageBarrier();

        void endFrame();

        /* Util Functions */
        void recreateSwapchain();
        void cleanupSwapchain();
        void createBuffer(VkDeviceSize size, VkBufferUsageFlags flags, VkMemoryPropertyFlags memFlags, VkBuffer& buffer, VkDeviceMemory& memory);
        uint32_t findMemoryType(uint32_t filter, VkMemoryPropertyFlags flags);
    };
};