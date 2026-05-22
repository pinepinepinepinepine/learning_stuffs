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

void RunTimeApplication::recordParticleCommandBuffer( uint32_t currentImageIndex )
{
    app->cmdBuffers.commandBuffers[executingCommandBufferIndex].bindPipeline( vk::PipelineBindPoint::eCompute, app->particleComputePipeline.pipeline );
    app->cmdBuffers.commandBuffers[executingCommandBufferIndex].bindDescriptorSets(
        vk::PipelineBindPoint::eCompute, app->particleComputePipeline.pipelineLayout, 0, *app->particleComputeDescriptors.descriptorSets[executingCommandBufferIndex], nullptr );
    app->cmdBuffers.commandBuffers[executingCommandBufferIndex].dispatch( PARTICLE_COUNT / 256, 1, 1 );

    vk::BufferMemoryBarrier2 computeBarrier_currentFrame {
        .srcStageMask = vk::PipelineStageFlagBits2::eComputeShader,
        .srcAccessMask = vk::AccessFlagBits2::eShaderWrite,
        .dstStageMask = vk::PipelineStageFlagBits2::eVertexShader,
        .dstAccessMask = vk::AccessFlagBits2::eShaderRead,

        .buffer = app->particle_storageBuffers_currentFrame[executingCommandBufferIndex].gpuBuffer,
        .offset = 0,
        .size = VK_WHOLE_SIZE };

    std::vector<vk::BufferMemoryBarrier2> computeBarriers { computeBarrier_currentFrame };

    vk::DependencyInfo dependencyInfo {
        .dependencyFlags = {},
        .bufferMemoryBarrierCount = static_cast<uint32_t>( computeBarriers.size() ),
        .pBufferMemoryBarriers = computeBarriers.data() };

    app->cmdBuffers.commandBuffers[executingCommandBufferIndex].pipelineBarrier2( dependencyInfo ); // we MIGHT need 2 commandBuffers if we can't allow catModel commandBuffer to run until this is completed.

        // Graphic Stuff
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

void RunTimeApplication::presentToWindow( uint32_t currentImageIndex, vk::PipelineStageFlags pipelineWaitStage, uint64_t waitForValue, uint64_t signalValue )
{
    vk::TimelineSemaphoreSubmitInfo TimelineSemaphoreSubmitInfo {
        .waitSemaphoreValueCount   = 1,
        .pWaitSemaphoreValues      = &waitForValue,
        .signalSemaphoreValueCount = 1,
        .pSignalSemaphoreValues    = &signalValue };

    vk::SubmitInfo submitInfo {
        .pNext                = &TimelineSemaphoreSubmitInfo, // Tells Vulkan that there's added data related to this semaphore.
        .waitSemaphoreCount   = 1,
        .pWaitSemaphores      = &*submissionTimelineSemaphore,
        .pWaitDstStageMask    = &pipelineWaitStage,
        .commandBufferCount   = 1,
        .pCommandBuffers      = &*app->cmdBuffers.commandBuffers[executingCommandBufferIndex],
        .signalSemaphoreCount = 1,
        .pSignalSemaphores    = &*submissionTimelineSemaphore }; // vk::TimelineSemaphoreSubmitInfo only provides the values associated with the semaphore, NOT the semaphores themselves.

    app->swapChain.swapChainImages[currentImageIndex].changeImageLayout(
        vk::ImageLayout::eColorAttachmentOptimal, vk::ImageLayout::ePresentSrcKHR, vk::AccessFlagBits2::eColorAttachmentWrite, {}, // The destination (ePresentSrcKHR) doesn't need any access rights.
        vk::PipelineStageFlagBits2::eColorAttachmentOutput, vk::PipelineStageFlagBits2::eBottomOfPipe,
        vk::ImageAspectFlagBits::eColor, 1, &app->cmdBuffers.commandBuffers[executingCommandBufferIndex] );

    app->cmdBuffers.commandBuffers[executingCommandBufferIndex].end();

    app->device.queue.submit( submitInfo, nullptr ); // During the submit call, pointed data remains alive.

    vk::SemaphoreWaitInfo beforePresentWaitInfo {
		.semaphoreCount = 1, .pSemaphores = &*submissionTimelineSemaphore, .pValues = &signalValue };
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

    updateMVPUBOBuffer();
    updateParticleBuffer();

    app->cmdBuffers.commandBuffers[executingCommandBufferIndex].begin({}); // .end() is located within presentToWindow

    recordCatCommandBuffer( imageIndex );
    recordParticleCommandBuffer( imageIndex );

    uint64_t graphicsWaitForValue = submissionTimelineSemaphoreValue;
    uint64_t graphicsFinishValue = ++submissionTimelineSemaphoreValue;
    presentToWindow( imageIndex, vk::PipelineStageFlagBits::eColorAttachmentOutput, graphicsWaitForValue, graphicsFinishValue );
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
    mainLoop();
}