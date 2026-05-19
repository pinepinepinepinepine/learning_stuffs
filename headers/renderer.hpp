#pragma once

#include "glfwWindow.hpp"
#include "device.hpp"
#include "descriptors.hpp"
#include "gpuBuffers.hpp"
#include "commandBuffers.hpp"
#include "texture.hpp"
#include "swapChain.hpp"
#include "pipeline.hpp"
#include "vertex.hpp"
#include "model.hpp"

constexpr int MAX_FRAMES_IN_FLIGHT = 2;

struct mvpUBOBuffer {
    glm::mat4 model;
    glm::mat4 view;
    glm::mat4 proj;
};

struct RenderApplication
{
    Window window;
    LogicalDevice device;

    vk::raii::CommandPool transientCommandPool = nullptr;
    vk::raii::CommandPool dedicatedCommandPool = nullptr;
    DedicatedCommandBuffers cmdBuffers;

    vk::raii::Sampler textureSampler = nullptr;
    Texture catTexture;

    vk::raii::DescriptorPool descriptorPool = nullptr;
    Descriptor descriptors;

    std::vector<GPUBuffer> mvp_uboBuffers;
    std::vector<GPUBuffer> debug_uboBuffers;

    SwapChain swapChain;
    Image colourImage;
    Image depthImage;

    Pipeline graphicPipeline;

    ModelData catModel;

    void setup();
    void createVertexGraphicsPipeline();
    void createCommandPools();
    void createDedicatedCommandBuffers();
    void createMVPUBOBuffers();
    void createDebugBuffers();
    vk::raii::Sampler createTextureSampler();
    void createAttachmentImages();
    void createDescriptors();
    void cleanup();
};