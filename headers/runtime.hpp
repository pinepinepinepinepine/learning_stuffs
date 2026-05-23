#pragma once

#include "includes.hpp"
#include "renderer.hpp"

struct RunTimeApplication
{
    RenderApplication* app;

    std::vector<vk::raii::Fence> drawFrameFence;
    uint32_t executingCommandBufferIndex;
    vk::raii::Semaphore submissionTimelineSemaphore = nullptr;
    uint64_t submissionTimelineSemaphoreValue = 0;

    double lastFrameTime = 0.0;
    double lastTime = 0.0f;


    RunTimeApplication( RenderApplication* setupInfo ) : app(setupInfo)
    {}

    void createSynchronizationObjects();

    void updateMVPUBOBuffer();
    void updateParticleBuffer();
    void recordCatCommandBuffer( uint32_t currentImageIndex );
    void recordParticleComputeCommandBuffer( const vk::raii::CommandBuffer& threadCommandBuffer, const ParticleGroup& pushConstantParticleGroup ); // Threaded
    void recordParticleGraphicCommandBuffer( uint32_t currentImageIndex );
    void transitionSwapChainImageToPresentOptimal( uint32_t currentImageIndex );
    void submitComputeCommandBuffers( uint64_t waitForValue, uint64_t signalValue );
    void submitCommandBuffers( uint32_t currentImageIndex, vk::PipelineStageFlags pipelineWaitStage, uint64_t waitForValue, uint64_t signalValue );
    void presentToWindow( uint32_t currentImageIndex, uint64_t signalValue );
    void drawFrame();
    void mainLoop();
    void run();


    void initThreads();
    void threadWork( uint32_t threadIndex );
};