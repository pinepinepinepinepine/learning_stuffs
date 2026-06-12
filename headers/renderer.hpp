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
#include "../componentAbstractions/headers/resourceManager.hpp"
#include "../componentAbstractions/headers/cullingSystem.hpp"
#include "../componentAbstractions/headers/offscreenShadowMap.hpp"

constexpr int MAX_FRAMES_IN_FLIGHT = 2;

struct mvpUBOBuffer {
    glm::mat4 model;
    glm::mat4 view;
    glm::mat4 proj;
};

// Model component is being passed as a push constant for the wireframe.
struct vpUBOBuffer {
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

    vk::raii::DescriptorPool descriptorPool = nullptr;
    Descriptor descriptors;
    Descriptor particleComputeDescriptors;
    Descriptor particleGraphicDescriptors;
    Descriptor wireframeDescriptors;

    std::vector<GPUBuffer> mvp_uboBuffers;
    std::vector<GPUBuffer> debug_uboBuffers;
    std::vector<GPUBuffer> particle_storageBuffers_currentFrame;
    std::vector<GPUBuffer> particle_storageBuffers_uboMule;
    std::vector<GPUBuffer> particle_debugComputeBuffers;
    std::vector<GPUBuffer> particle_debugGraphicsBuffers;
    std::vector<GPUBuffer> wireframe_vp_uboBuffers;

    SwapChain swapChain;
    Image colourImage;
    Image depthImage;

    Pipeline graphicPipeline;
    Pipeline particleGraphicPipeline;
    Pipeline particleComputePipeline;
    Pipeline wireframePipeline;

    ThreadManager threadManager;

    ResourceManager<ModelData> modelManager;
    ResourceHandle<ModelData> catModelHandle;

    ResourceManager<Texture> textureManager;
    ResourceHandle<Texture> catTextureHandle;
    ResourceHandle<Texture> poTextureHandle;

    Entity cat {"Cat"};
    Entity camera {"Camera"};
    Entity globalCamera {"GlobalCamera"};
    std::vector<Entity*> allEntities;

    CullingSystem cullSystem;

    LightingSystem lightSystem;

    void setup();
    void createVertexGraphicsPipeline();
    void createParticleGraphicsPipeline();
    void createParticleComputePipeline();
    void createWireframeGraphicsPipeline();
    void createCommandPools();
    void createMVPUBOBuffers();
    void createDebugBuffers();
    void createWireframeMVPUBOBuffers();
    void createParticleComputeBuffers();
    vk::raii::Sampler createTextureSampler();
    void createAttachmentImages();
    void createDescriptorPool();
    void createModelDescriptors();
    void createParticleDescriptors();
    void createWireframeDescriptors();
    void cleanup();

    void createThreads();
    void createModels();
    void createTextures();

    void createCatEntity();
};

// TODO: Maybe move the functions to particle.cpp/vertex.hpp for their pipelines?