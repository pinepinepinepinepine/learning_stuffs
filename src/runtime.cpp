#include "../headers/runtime.hpp"




void RunTimeApplication::createSynchronizationObjects()
{
    drawFrameFence.clear();
    executingCommandBufferIndex = 0;

    for ( size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++ )
    {
        vk::FenceCreateInfo fenceInfo{};
        drawFrameFence.emplace_back( app->device.logicalDevice, fenceInfo );
    }

    vk::SemaphoreTypeCreateInfo semaphoreCreateInfo { .semaphoreType = vk::SemaphoreType::eTimeline, .initialValue = 0 };
    submissionTimelineSemaphore = vk::raii::Semaphore( app->device.logicalDevice, { .pNext = &semaphoreCreateInfo } );
}

void RunTimeApplication::updateMVPUBOBuffer()
{

    static auto startTime = std::chrono::high_resolution_clock::now();

	auto  currentTime = std::chrono::high_resolution_clock::now();
	float time        = std::chrono::duration<float>(currentTime - startTime).count();

    mvpUBOBuffer mvpTransformationMatrix{};

    mvpTransformationMatrix.model = glm::rotate( glm::mat4(1.0f), time* glm::radians(160.0f), glm::vec3(0.0f, 1.0f, 0.0f) );

    mvpTransformationMatrix.view = glm::lookAt(glm::vec3(-20.0f, 30.0f, 60.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));

    mvpTransformationMatrix.proj = glm::perspective(
        glm::radians(90.0f), static_cast<float>(app->swapChain.imageResolution.width) / static_cast<float>(app->swapChain.imageResolution.height), 0.1f, 350.0f );
    mvpTransformationMatrix.proj[1][1] *= -1; // Flip Y for Vulkan

    memcpy( app->mvp_uboBuffers[executingCommandBufferIndex].gpuBufferMapped, &mvpTransformationMatrix, sizeof(mvpTransformationMatrix) );
}

void RunTimeApplication::recordCatCommandBuffer( uint32_t currentImageIndex )
{
    app->swapChain.swapChainImages[currentImageIndex].changeImageLayout(
        vk::ImageLayout::eUndefined, vk::ImageLayout::eColorAttachmentOptimal,
        {}, vk::AccessFlagBits2::eColorAttachmentWrite,
        vk::PipelineStageFlagBits2::eColorAttachmentOutput, vk::PipelineStageFlagBits2::eColorAttachmentOutput,
        vk::ImageAspectFlagBits::eColor, 1, &app->cmdBuffers.commandBuffers[executingCommandBufferIndex] ); // Maybe make a #define for 1 in the context of mipImages?

    vk::ClearValue clearColour = vk::ClearColorValue( (199/255.0f), (160/255.0f), (148/255.0f), 1.0f );
    vk::ClearValue clearDepth = vk::ClearDepthStencilValue( 1.0f, 0 );

    // TODO: can we REUSE these? do we HAVE to change layout EVERY record?
    app->colourImage.changeImageLayout(
        vk::ImageLayout::eUndefined, vk::ImageLayout::eColorAttachmentOptimal,
        vk::AccessFlagBits2::eColorAttachmentWrite, vk::AccessFlagBits2::eColorAttachmentWrite,
        vk::PipelineStageFlagBits2::eColorAttachmentOutput, vk::PipelineStageFlagBits2::eColorAttachmentOutput,
        vk::ImageAspectFlagBits::eColor, 1, &app->cmdBuffers.commandBuffers[executingCommandBufferIndex] );
    app->depthImage.changeImageLayout(
        vk::ImageLayout::eUndefined, vk::ImageLayout::eDepthAttachmentOptimal,
        vk::AccessFlagBits2::eDepthStencilAttachmentWrite, vk::AccessFlagBits2::eDepthStencilAttachmentWrite,
        vk::PipelineStageFlagBits2::eEarlyFragmentTests | vk::PipelineStageFlagBits2::eLateFragmentTests,
        vk::PipelineStageFlagBits2::eEarlyFragmentTests | vk::PipelineStageFlagBits2::eLateFragmentTests,
        vk::ImageAspectFlagBits::eDepth, 1, &app->cmdBuffers.commandBuffers[executingCommandBufferIndex] );

    vk::RenderingAttachmentInfo colourAttachmentInfo {
        .imageView          = app->colourImage.imageView,
        .imageLayout        = vk::ImageLayout::eColorAttachmentOptimal,
        .resolveMode        = vk::ResolveModeFlagBits::eAverage,
        .resolveImageView   = app->swapChain.swapChainImages[currentImageIndex].imageView,
        .resolveImageLayout = vk::ImageLayout::eColorAttachmentOptimal,
        .loadOp             = vk::AttachmentLoadOp::eClear,
        .storeOp            = vk::AttachmentStoreOp::eStore,
        .clearValue         = clearColour };

    vk::RenderingAttachmentInfo depthAttachmentInfo {
        .imageView          = app->depthImage.imageView,
        .imageLayout        = vk::ImageLayout::eDepthAttachmentOptimal,
        .loadOp             = vk::AttachmentLoadOp::eClear,
        .storeOp            = vk::AttachmentStoreOp::eStore,
        .clearValue = clearDepth };

    vk::RenderingInfo renderingInfo {
        .renderArea           = { .offset = { 0, 0 }, .extent = app->swapChain.imageResolution },
        .layerCount           = 1,
        .colorAttachmentCount = 1,
        .pColorAttachments    = &colourAttachmentInfo,
        .pDepthAttachment     = &depthAttachmentInfo };

    app->cmdBuffers.commandBuffers[executingCommandBufferIndex].beginRendering( renderingInfo );

    app->cmdBuffers.commandBuffers[executingCommandBufferIndex].bindPipeline( vk::PipelineBindPoint::eGraphics, app->graphicPipeline.pipeline );

    app->cmdBuffers.commandBuffers[executingCommandBufferIndex].bindVertexBuffers(0, *app->catModel.vertexBuffer.gpuBuffer, {0} );
    app->cmdBuffers.commandBuffers[executingCommandBufferIndex].bindIndexBuffer( *app->catModel.indexBuffer.gpuBuffer, 0, vk::IndexType::eUint32 );

    app->cmdBuffers.commandBuffers[executingCommandBufferIndex].bindDescriptorSets(
       vk::PipelineBindPoint::eGraphics, app->graphicPipeline.pipelineLayout, 0, *app->descriptors.descriptorSets[executingCommandBufferIndex], nullptr );

    app->cmdBuffers.commandBuffers[executingCommandBufferIndex].drawIndexed( app->catModel.indices_count, 1, 0, 0, 0 ); // todo: is there a better alternative for indices_count?

    app->cmdBuffers.commandBuffers[executingCommandBufferIndex].endRendering();
}

void RunTimeApplication::updateParticleBuffer()
{
    ParticleTime particleTime;
    particleTime.deltaTime = static_cast<float>(lastFrameTime) * 2.0f;
    memcpy( app->particle_storageBuffers_uboMule[executingCommandBufferIndex].gpuBufferMapped, &particleTime, sizeof(particleTime) );
}

void RunTimeApplication::recordParticleComputeCommandBuffer( const vk::raii::CommandBuffer& threadCommandBuffer, const ParticleGroup& pushConstantParticleGroup )
{
    threadCommandBuffer.reset();
    vk::CommandBufferBeginInfo beginInfo{
        .flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit };
    threadCommandBuffer.begin(beginInfo);

    threadCommandBuffer.bindPipeline( vk::PipelineBindPoint::eCompute, app->particleComputePipeline.pipeline );

    threadCommandBuffer.bindDescriptorSets(
        vk::PipelineBindPoint::eCompute, app->particleComputePipeline.pipelineLayout, 0, *app->particleComputeDescriptors.descriptorSets[executingCommandBufferIndex], nullptr );
            // TODO: We're passing the whole descriptor set, including particles that aren't responsible for this thread. Maybe make it specialized? Would it be faster?

    threadCommandBuffer.pushConstants<ParticleGroup>( app->particleComputePipeline.pipelineLayout, vk::ShaderStageFlagBits::eCompute, 0, pushConstantParticleGroup );

    uint32_t groupCount = ( pushConstantParticleGroup.count + 255 ) / 256; // ensures we've enough workgroups for odd (not perfectly fitting) numbers, add by 255 to then cull its decimals via implicit conversion to int.
    threadCommandBuffer.dispatch( groupCount, 1, 1 );

    threadCommandBuffer.end();
}


void RunTimeApplication::recordParticleGraphicCommandBuffer( uint32_t currentImageIndex )
{
    vk::RenderingAttachmentInfo colourAttachmentInfo {
        .imageView          = app->colourImage.imageView,
        .imageLayout        = vk::ImageLayout::eColorAttachmentOptimal,
        .resolveMode        = vk::ResolveModeFlagBits::eAverage,
        .resolveImageView   = app->swapChain.swapChainImages[currentImageIndex].imageView,
        .resolveImageLayout = vk::ImageLayout::eColorAttachmentOptimal,
        .loadOp             = vk::AttachmentLoadOp::eLoad,
        .storeOp            = vk::AttachmentStoreOp::eStore };

    vk::RenderingAttachmentInfo depthAttachmentInfo {
        .imageView          = app->depthImage.imageView,
        .imageLayout        = vk::ImageLayout::eDepthAttachmentOptimal,
        .loadOp             = vk::AttachmentLoadOp::eLoad,
        .storeOp            = vk::AttachmentStoreOp::eStore };

    vk::RenderingInfo renderingInfo {
        .renderArea           = { .offset = { 0, 0 }, .extent = app->swapChain.imageResolution },
        .layerCount           = 1,
        .colorAttachmentCount = 1,
        .pColorAttachments    = &colourAttachmentInfo,
        .pDepthAttachment     = &depthAttachmentInfo };

    app->cmdBuffers.commandBuffers[executingCommandBufferIndex].beginRendering( renderingInfo );
    app->cmdBuffers.commandBuffers[executingCommandBufferIndex].bindPipeline( vk::PipelineBindPoint::eGraphics, app->particleGraphicPipeline.pipeline );
    app->cmdBuffers.commandBuffers[executingCommandBufferIndex].bindVertexBuffers(
       0, { app->particle_storageBuffers_currentFrame[executingCommandBufferIndex].gpuBuffer }, {0} );
    app->cmdBuffers.commandBuffers[executingCommandBufferIndex].bindDescriptorSets(
        vk::PipelineBindPoint::eGraphics, app->particleGraphicPipeline.pipelineLayout, 0, *app->particleGraphicDescriptors.descriptorSets[executingCommandBufferIndex], nullptr );
    app->cmdBuffers.commandBuffers[executingCommandBufferIndex].draw(PARTICLE_COUNT, 1, 0, 0 );
    app->cmdBuffers.commandBuffers[executingCommandBufferIndex].endRendering();
}

void RunTimeApplication::transitionSwapChainImageToPresentOptimal( uint32_t currentImageIndex )
{
    app->swapChain.swapChainImages[currentImageIndex].changeImageLayout(
    vk::ImageLayout::eColorAttachmentOptimal, vk::ImageLayout::ePresentSrcKHR, vk::AccessFlagBits2::eColorAttachmentWrite, {}, // The destination (ePresentSrcKHR) doesn't need any access rights.
    vk::PipelineStageFlagBits2::eColorAttachmentOutput, vk::PipelineStageFlagBits2::eBottomOfPipe,
    vk::ImageAspectFlagBits::eColor, 1, &app->cmdBuffers.commandBuffers[executingCommandBufferIndex] );
}

void RunTimeApplication::submitComputeCommandBuffers( uint64_t waitForValue, uint64_t signalValue )
{
    std::vector<vk::CommandBuffer> computeCmdBuffers;
    computeCmdBuffers.reserve(THREAD_COUNT);
    for (uint32_t i = 0; i < THREAD_COUNT; i++)
    {
        try { computeCmdBuffers.push_back( app->threadManager.getCommandBuffer(i) ); }
        catch (const std::exception &) { throw std::runtime_error("Couldn't push back a thread's compute command buffer!"); }
    }

    // Set up compute submission
    vk::TimelineSemaphoreSubmitInfo computeTimelineInfo {
        .waitSemaphoreValueCount   = 1,
        .pWaitSemaphoreValues      = &waitForValue,
        .signalSemaphoreValueCount = 1,
        .pSignalSemaphoreValues    = &signalValue };

    vk::PipelineStageFlags waitStages[] = {vk::PipelineStageFlagBits::eComputeShader};

    vk::SubmitInfo computeSubmitInfo {
        .pNext                = &computeTimelineInfo,
        .waitSemaphoreCount   = 1,
        .pWaitSemaphores      = &*submissionTimelineSemaphore,
        .pWaitDstStageMask    = waitStages,
        .commandBufferCount   = static_cast<uint32_t>(computeCmdBuffers.size()),
        .pCommandBuffers      = computeCmdBuffers.data(),
        .signalSemaphoreCount = 1,
        .pSignalSemaphores    = &*submissionTimelineSemaphore };

    {
        std::lock_guard<std::mutex> lock(app->threadManager.queueSubmitMutex);
        app->device.queue.submit(computeSubmitInfo, nullptr);
    }
}

void RunTimeApplication::submitCommandBuffers( uint32_t currentImageIndex, vk::PipelineStageFlags pipelineWaitStage, uint64_t waitForValue, uint64_t signalValue )
{
    vk::TimelineSemaphoreSubmitInfo TimelineSemaphoreSubmitInfo {
        .waitSemaphoreValueCount   = 1,
        .pWaitSemaphoreValues      = &waitForValue,
        .signalSemaphoreValueCount = 1,
        .pSignalSemaphoreValues    = &signalValue };

    vk::SubmitInfo submitInfo {
        .pNext                  = &TimelineSemaphoreSubmitInfo, // Tells Vulkan that there's added data related to this semaphore.
        .waitSemaphoreCount     = 1,
        .pWaitSemaphores        = &*submissionTimelineSemaphore,
        .pWaitDstStageMask      = &pipelineWaitStage,
        .commandBufferCount     = 1,
        .pCommandBuffers        = &*app->cmdBuffers.commandBuffers[executingCommandBufferIndex],
        .signalSemaphoreCount   = 1,
        .pSignalSemaphores      = &*submissionTimelineSemaphore };

    {
        std::lock_guard<std::mutex> lock(app->threadManager.queueSubmitMutex);
        app->device.queue.submit( submitInfo, nullptr ); // During the submit call, pointed data remains alive.
    }
}

void RunTimeApplication::presentToWindow( uint32_t currentImageIndex, uint64_t signalToBeginValue )
{
    vk::SemaphoreWaitInfo beforePresentWaitInfo {
		.semaphoreCount = 1, .pSemaphores = &*submissionTimelineSemaphore, .pValues = &signalToBeginValue };
    if ( app->device.logicalDevice.waitSemaphores( beforePresentWaitInfo, UINT64_MAX ) != vk::Result::eSuccess)
		{ throw std::runtime_error("failed to wait for semaphore!"); }

    const vk::PresentInfoKHR presentInfoKHR {
        .waitSemaphoreCount = 0, .pWaitSemaphores = nullptr,
        .swapchainCount = 1, .pSwapchains = &*app->swapChain.swapChain, .pImageIndices = &currentImageIndex };
    vk::Result result = app->device.queue.presentKHR( presentInfoKHR );
    if ( (result == vk::Result::eSuboptimalKHR) || (result == vk::Result::eErrorOutOfDateKHR) )
        throw std::runtime_error("Couldn't present the image to the window surface properly!");

    executingCommandBufferIndex = (executingCommandBufferIndex + 1) % MAX_FRAMES_IN_FLIGHT;
}

void RunTimeApplication::drawFrame()
{
    if ( drawFrameFence.empty() ) throw std::runtime_error("The Draw Frame's Fences were NOT created!");

    auto [result, imageIndex] = app->swapChain.swapChain.acquireNextImage( UINT64_MAX, nullptr, drawFrameFence[executingCommandBufferIndex] );
    if ( app->device.logicalDevice.waitForFences( *drawFrameFence[executingCommandBufferIndex], vk::True, UINT64_MAX ) != vk::Result::eSuccess )
        throw std::runtime_error("Failed to wait for the Draw Frame's fence!");
    app->device.logicalDevice.resetFences( *drawFrameFence[executingCommandBufferIndex] );

    uint64_t computeWaitForValue = submissionTimelineSemaphoreValue;
    uint64_t computeFinishValue = ++submissionTimelineSemaphoreValue;
    uint64_t graphicsWaitForValue = submissionTimelineSemaphoreValue;
    uint64_t graphicsFinishValue = ++submissionTimelineSemaphoreValue;

    updateMVPUBOBuffer();
    updateParticleBuffer();

    app->cmdBuffers.commandBuffers[executingCommandBufferIndex].begin({});

    app->threadManager.signalThreadsToWork();

    recordCatCommandBuffer( imageIndex );

    app->threadManager.waitForthreadsToCompleteWork();

    submitComputeCommandBuffers( computeWaitForValue, computeFinishValue );

    recordParticleGraphicCommandBuffer( imageIndex ); // check if this is fine? barrier specifically.

    transitionSwapChainImageToPresentOptimal( imageIndex );

    app->cmdBuffers.commandBuffers[executingCommandBufferIndex].end();

    submitCommandBuffers( imageIndex, vk::PipelineStageFlagBits::eColorAttachmentOutput, graphicsWaitForValue, graphicsFinishValue );
    presentToWindow( imageIndex, graphicsFinishValue );
}

void RunTimeApplication::mainLoop()
{
    while (!glfwWindowShouldClose( app->window.window ))
    {
        glfwPollEvents();
        drawFrame();

        double currentTime = glfwGetTime();
		lastFrameTime      = (currentTime - lastTime) * 1000.0;
		lastTime           = currentTime;
    }
    app->device.logicalDevice.waitIdle();
}

void RunTimeApplication::run()
{
    createSynchronizationObjects();

    lastTime = glfwGetTime();
    initThreads();
    mainLoop();
}


void RunTimeApplication::initThreads()
{
    const uint32_t particlesPerThread = PARTICLE_COUNT / THREAD_COUNT;
    for (uint32_t i = 0; i < THREAD_COUNT; i++)
	{
		app->threadManager.threadWorkReady[i] = false;
		app->threadManager.threadWorkDone[i]  = true;

        if ( ( app->threadManager.particleGroups.size() - 1 ) != i )
            app->threadManager.particleGroups[i].count = particlesPerThread;
        else
            app->threadManager.particleGroups[i].count = (PARTICLE_COUNT - (i * particlesPerThread));
        app->threadManager.particleGroups[i].startIndex = i * particlesPerThread;

        app->threadManager.workerThreads.emplace_back( &threadWork, this, i );
	}
}

void RunTimeApplication::threadWork( uint32_t threadIndex )
{
    while ( app->threadManager.exitAllThreads == false )
    {
        {
            std::unique_lock<std::mutex> lock(app->threadManager.workCompleteMutex);
            app->threadManager.workCompleteCv.wait(lock, [this, threadIndex]() {
					return app->threadManager.exitAllThreads || app->threadManager.threadWorkReady[threadIndex].load(std::memory_order_acquire);
            }); // Puts the current executing thread to sleep until its got work to do.

            if ( app->threadManager.exitAllThreads == true )
                break; // If we've been prompted to exit all threads, exit the while loop

            if ( app->threadManager.threadWorkReady[threadIndex].load(std::memory_order_acquire) == false )
				continue; // if this thread's work is NOT ready to commence, go to the next iteration of the while loop and retry.
        } // Thread loses ownership of the lock after it goes out of scope (after the closing brace).

        const ParticleGroup &group = app->threadManager.particleGroups[threadIndex];
        bool workCompleted = false;

        try
		{
            recordParticleComputeCommandBuffer( app->threadManager.getCommandBuffer( threadIndex ), group );
            workCompleted = true;
		}
		catch (const std::exception &)
		{
			workCompleted = false;
		}

        app->threadManager.threadWorkDone[threadIndex].store(true, std::memory_order_release);
		app->threadManager.threadWorkReady[threadIndex].store(false, std::memory_order_release);
            // I feel like we can ditch workCompleted...

        // If this is not the last thread, signal the next thread to start
        if (threadIndex < THREAD_COUNT - 1)
        {
            app->threadManager.threadWorkReady[threadIndex + 1].store(true, std::memory_order_release);
        }

        // Notify main thread and other threads (so they can actually wake up and check if threadWorkReady w/ the .wait() above)
        {
            std::lock_guard<std::mutex> lock(app->threadManager.workCompleteMutex);
            app->threadManager.workCompleteCv.notify_all();
        }
    }
} // Threads become zombies when they exit this function -- not executing waiting to be cleaned up via .join