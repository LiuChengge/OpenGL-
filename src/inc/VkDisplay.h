/**
 * @brief Vulkan显示管理类
 *
 * 该类负责Vulkan实例、设备和窗口管理，为Vulkan渲染提供基础架构。
 */
#ifndef VKDISPLAY_H
#define VKDISPLAY_H

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <vulkan/vulkan.h>

#include <vector>
#include <optional>
#include <stdexcept>
#include <iostream>
#include <cstring>
#include <set>
#include <algorithm>
#include <string>
#include <limits>
#include <fstream>
#include <chrono>

class VkDisplay {
public:
    VkDisplay();
    ~VkDisplay();

    /**
     * @brief 初始化Vulkan显示环境
     * @param width 窗口宽度
     * @param height 窗口高度
     * @param title 窗口标题
     * @return 初始化是否成功
     */
    bool init(int width, int height, std::string title);

    /**
     * @brief 检查窗口是否应该关闭
     * @return true如果窗口应该关闭
     */
    bool shouldClose() { return glfwWindowShouldClose(window); }

    /**
     * @brief 处理窗口事件
     */
    void pollEvents() { glfwPollEvents(); }

    /**
     * @brief 执行渲染操作
     */
    void draw();

    /**
     * @brief 更新双目视频纹理数据
     * @param leftData 左眼图像数据（BGR格式）
     * @param rightData 右眼图像数据（BGR格式）
     * @param width 图像宽度
     * @param height 图像高度
     */
    void updateVideo(unsigned char* leftData, unsigned char* rightData, int width, int height);

    /**
     * @brief 清理Vulkan资源
     */
    void cleanup();

private:
    // GLFW窗口
    GLFWwindow* window = nullptr;

    // Vulkan核心对象
    VkInstance instance;
    VkDebugUtilsMessengerEXT debugMessenger;
    VkSurfaceKHR surface;
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    VkDevice device;
    VkQueue graphicsQueue;
    VkQueue presentQueue;

    // 交换链相关
    VkSwapchainKHR swapChain;
    std::vector<VkImage> swapChainImages;
    VkFormat swapChainImageFormat;
    VkExtent2D swapChainExtent;
    std::vector<VkImageView> swapChainImageViews;

    // 渲染管线相关
    VkRenderPass renderPass;
    VkDescriptorSetLayout descriptorSetLayout;
    VkPipelineLayout pipelineLayout;
    VkPipeline graphicsPipeline;
    std::vector<VkFramebuffer> swapChainFramebuffers;
    VkCommandPool commandPool;

    // 纹理资源（左眼和右眼）
    VkImage leftTextureImage, rightTextureImage;
    VkDeviceMemory leftTextureImageMemory, rightTextureImageMemory;
    VkImageView leftTextureImageView, rightTextureImageView;
    VkSampler textureSampler;
    VkDescriptorPool descriptorPool;
    VkDescriptorSet descriptorSet;

    // Staging Buffer（支持多缓冲以实现 CPU/GPU 并行）
    std::vector<VkBuffer> stagingBuffers;
    std::vector<VkDeviceMemory> stagingBufferMemories;
    std::vector<void*> stagingBuffersMapped;

    // 命令缓冲区和同步对象
    std::vector<VkCommandBuffer> commandBuffers;
    std::vector<VkSemaphore> imageAvailableSemaphores;
    std::vector<VkSemaphore> renderFinishedSemaphores;
    std::vector<VkFence> inFlightFences;
    uint32_t currentFrame = 0;

    // 配置常量
    static constexpr int MAX_FRAMES_IN_FLIGHT = 1;  // 改为 1 启用低延迟模式
    bool framebufferResized = false;

    // Vulkan初始化辅助函数
    void initGLFW(int width, int height, std::string title);
    void createInstance();
    void setupDebugMessenger();
    void createSurface();
    void pickPhysicalDevice();
    void createLogicalDevice();
    void createSwapChain();
    void createImageViews();
    void createRenderPass();
    void createDescriptorSetLayout();
    void createGraphicsPipeline();
    void createFramebuffers();
    void createCommandPool();
    void createTextureResources();
    void createStagingBuffer();
    void createDescriptors();
    void createSyncObjects();
    void createCommandBuffers();
    VkShaderModule createShaderModule(const std::vector<char>& code);

    // 文件读取辅助函数
    static std::vector<char> readFile(const std::string& filename);

    // 渲染和同步函数
    void recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex);
    void recreateSwapChain();
    // 辅助函数（主要用于调试或特殊情况）
    void transitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout);
    void copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height, uint32_t offset);
    VkCommandBuffer beginSingleTimeCommands();
    void endSingleTimeCommands(VkCommandBuffer commandBuffer);

    // 辅助函数
    uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);

    // Vulkan辅助函数
    bool checkValidationLayerSupport();
    std::vector<const char*> getRequiredExtensions();
    bool isDeviceSuitable(VkPhysicalDevice device);
    bool checkDeviceExtensionSupport(VkPhysicalDevice device);
    struct QueueFamilyIndices {
        std::optional<uint32_t> graphicsFamily;
        std::optional<uint32_t> presentFamily;

        bool isComplete() {
            return graphicsFamily.has_value() && presentFamily.has_value();
        }
    };
    QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device);

    struct SwapChainSupportDetails {
        VkSurfaceCapabilitiesKHR capabilities;
        std::vector<VkSurfaceFormatKHR> formats;
        std::vector<VkPresentModeKHR> presentModes;
    };
    SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device);
    VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats);
    VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes);
    VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities);

    // Vulkan调试回调
    static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
        VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
        VkDebugUtilsMessageTypeFlagsEXT messageType,
        const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
        void* pUserData);

    // Vulkan扩展函数加载
    VkResult CreateDebugUtilsMessengerEXT(VkInstance instance,
                                          const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo,
                                          const VkAllocationCallbacks* pAllocator,
                                          VkDebugUtilsMessengerEXT* pDebugMessenger);
    void DestroyDebugUtilsMessengerEXT(VkInstance instance,
                                       VkDebugUtilsMessengerEXT debugMessenger,
                                       const VkAllocationCallbacks* pAllocator);
};

#endif // VKDISPLAY_H
