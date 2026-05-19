#pragma once

#include "includes.hpp"

#ifdef NDEBUG
constexpr bool enableValidationLayers = false;
#else
constexpr bool enableValidationLayers = true;
#endif

struct VulkanInstance
{
    vk::raii::Instance instance = nullptr; // Handle to the vulkan instance (like win32's HWND!)
    vk::raii::Context context;
    std::vector<const char *> vulkanLayers;
    std::vector<const char *> vulkanExtensions;
    vk::raii::DebugUtilsMessengerEXT debugMessenger = nullptr;

    void setRequiredInstance_Extensions();
    void setRequiredInstance_Layers();
    void createVulkanInstanceHandle();

    void setupDebugMessenger();
    static VKAPI_ATTR vk::Bool32 VKAPI_CALL debugCallbackFunction(
        vk::DebugUtilsMessageSeverityFlagBitsEXT       severity,      // This specifies the severity of the message -- see big_notes.
        vk::DebugUtilsMessageTypeFlagsEXT              type,          // The message type of the error -- see big_notes.
        const vk::DebugUtilsMessengerCallbackDataEXT*  pCallbackData, // What data/object caused this error -- see big_notes.
        void*                                          pUserData );

    void createVulkanInstance();
};

struct PhysicalDevice : VulkanInstance
{
    vk::raii::PhysicalDevice physicalDevice = nullptr;
    vk::SampleCountFlagBits msaaSamples;
    std::vector<const char*> requiredGPUDeviceExtension { vk::KHRSwapchainExtensionName };

    bool deviceSupportsReq_Queues( const vk::raii::PhysicalDevice& graphicDevice );
    bool deviceSupportsReq_GPUExtensions( const vk::raii::PhysicalDevice& graphicDevice );
    bool deviceSupportsReq_GPUFeatures( const vk::raii::PhysicalDevice& graphicDevice );
    bool isDeviceSuitable( const vk::raii::PhysicalDevice& graphicDevice );
    vk::Format findSupportedFormat( const std::vector<vk::Format>& candidates, vk::ImageTiling tiling, vk::FormatFeatureFlags features ) const;
    void setMaxUsableSampleCount();
    void setPhysicalGPUDevice();
};

struct LogicalDevice : PhysicalDevice
{
    vk::raii::Device logicalDevice = nullptr;
    uint32_t queueIndex = ~0;
    vk::raii::Queue queue = nullptr;

    void createLogicalDevice( const VkSurfaceKHR* window_surface );
};