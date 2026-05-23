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
#include "particle.hpp"
#include "thread.hpp"

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
    Descriptor particleComputeDescriptors;
    Descriptor particleGraphicDescriptors;

    std::vector<GPUBuffer> mvp_uboBuffers;
    std::vector<GPUBuffer> debug_uboBuffers;
    std::vector<GPUBuffer> particle_storageBuffers_currentFrame;
    std::vector<GPUBuffer> particle_storageBuffers_uboMule;
    std::vector<GPUBuffer> particle_debugComputeBuffers;
    std::vector<GPUBuffer> particle_debugGraphicsBuffers;

    SwapChain swapChain;
    Image colourImage;
    Image depthImage;

    Pipeline graphicPipeline;
    Pipeline particleGraphicPipeline;
    Pipeline particleComputePipeline;

    ModelData catModel;

    ThreadManager threadManager;

    void setup();
    void createVertexGraphicsPipeline();
    void createParticleGraphicsPipeline();
    void createParticleComputePipeline();
    void createCommandPools();
    void createMVPUBOBuffers();
    void createDebugBuffers();
    void createParticleComputeBuffers();
    vk::raii::Sampler createTextureSampler();
    void createAttachmentImages();
    void createDescriptorPool();
    void createModelDescriptors();
    void createParticleDescriptors();
    void cleanup();

    void createThreads();
};

// TODO: Maybe move the functions to particle.cpp/vertex.hpp for their pipelines?