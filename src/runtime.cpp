#include "../headers/runtime.hpp"
#include "../headers/externState.hpp"


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

// I should separate this.
void RunTimeApplication::updateMVPUBOBuffer()
{

    static auto startTime = std::chrono::high_resolution_clock::now();
	auto  currentTime = std::chrono::high_resolution_clock::now();
	float time        = std::chrono::duration<float>(currentTime - startTime).count();

    mvpUBOBuffer mvpTransformationMatrix{};

    // Camera Rotation
    static glm::vec2 last_cursor_position = right_click_lock_at;
    static bool prevFrame_rightClickLock = rightClickLock;
    TransformComponent* currentCameraTransformation = currentCamera->GetComponent<TransformComponent>();

    glm::mat4 camTransform = currentCameraTransformation->GetTransformMatrix();
    bool wasMoved = false;

    if ( rightClickLock )
    {
        if ( !prevFrame_rightClickLock )
            last_cursor_position = current_cursor_position;

        glm::vec2 movement_this_frame = ( current_cursor_position - last_cursor_position ) * -0.0015f; // Make a sensitivity option.

        // make a .Dirty variable (if its been modified for its position/rotation so that if true, when we call getCameraFrustum, it's right.)
        if ( movement_this_frame.x || movement_this_frame.y )
            wasMoved = true;

        glm::quat rotaQuat_side = glm::angleAxis( movement_this_frame.x, glm::vec3( 0.0f, 1.0f, 0.0f ) );
        glm::quat rotaQuat_up = glm::angleAxis( movement_this_frame.y, glm::vec3( 1.0f, 0.0f, 0.0f ) ) ;
        currentCameraTransformation->SetRotation( rotaQuat_side * currentCameraTransformation->GetRotation() * rotaQuat_up );
        last_cursor_position = current_cursor_position;
    }
    prevFrame_rightClickLock = rightClickLock;

    // Camera Position
    glm::vec3 movementVector(
        ( held_a ? -1 : 0 ) + ( held_d ? 1 : 0 ),
        ( held_space ? 1 : 0 ) + ( held_ctrl ? -1 : 0 ),
        ( held_w ? -1 : 0 ) + ( held_s ? 1 : 0 ) );
    if ( glm::length( movementVector ) >= 1.0f )
        movementVector = glm::normalize( movementVector ); // Normalize it to have a length of 1 so we don't get that strafe thing where speed is faster when going sideways (Counter-strike's ladders!)

    if ( wasMoved || ( movementVector.x || movementVector.y || movementVector.z ) )
       currentCamera->GetComponent<CameraComponent>()->updateCameraFrustum();


    glm::vec3 position = currentCamera->GetComponent<TransformComponent>()->GetPosition();
        // glm::vec3 z_movement = rotaQuat * glm::vec3( 0.0f, 0.0f, 1.0f );
        // glm::vec3 y_movement = rotaQuat * glm::vec3( 0.0f, 1.0f, 0.0f );
        // glm::vec3 x_movement = rotaQuat * glm::vec3( 1.0f, 0.0f, 0.0f );
        // position.x += (movementVector.z * z_movement.x) + (movementVector.x * x_movement.x) + (movementVector.y * y_movement.x);
        // position.y += (movementVector.z * z_movement.y) + (movementVector.x * x_movement.y) + (movementVector.y * y_movement.y);
        // position.z += (movementVector.z * z_movement.z) + (movementVector.x * x_movement.z) + (movementVector.y * y_movement.z);
    position += (currentCameraTransformation->GetRotation() * movementVector); // Above comment block is the MANUAL way of calculating this.
    currentCamera->GetComponent<TransformComponent>()->SetPosition( position );

    // Model Rotation
    static double last_frame_time = glfwGetTime();
    static double total_spin_time = 0;
    double curr = glfwGetTime();
    double time_passed = curr - last_frame_time;
    if ( toggle_r )
    {
        total_spin_time += time_passed;
        glm::quat cat_rotation = glm::angleAxis( glm::degrees( float( total_spin_time * ( 0.174533f ) ) ), glm::vec3( 0.0f, 1.0f, 0.0f ) ); // Just... ugly number.
        app->cat.GetComponent<TransformComponent>()->SetRotation( cat_rotation );
    }
    last_frame_time = curr;

    // this DEFINITELY does NOT need to be here. fix later. this variable is here so we don't have to perma set it while toggle'd
    static bool was_toggled = false;
    if ( !was_toggled && toggle_t )
    {
        app->cat.GetComponent<TextureComponent>()->setTexture( app->poTextureHandle.get() );

        for ( int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++ )
        {
            app->descriptors.setSamplerResource( app->device.logicalDevice, *app->poTextureHandle.get()->textureSampler, app->poTextureHandle.get()->textureImage.imageView, i, 1 );
        }
    }
    else if ( was_toggled && !toggle_t )
    {
        app->cat.GetComponent<TextureComponent>()->setTexture( app->catTextureHandle.get() );
        for ( int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++ )
        {
            app->descriptors.setSamplerResource( app->device.logicalDevice, *app->catTextureHandle.get()->textureSampler, app->catTextureHandle.get()->textureImage.imageView, i, 1 );
        }
    }
    was_toggled = toggle_t;

    mvpTransformationMatrix.model = app->cat.GetComponent<TransformComponent>()->GetTransformMatrix();
    mvpTransformationMatrix.view = currentCamera->GetComponent<CameraComponent>()->getViewMatrix(); // me: Figure out how engines do a global camera.
    mvpTransformationMatrix.proj = currentCamera->GetComponent<CameraComponent>()->getProjectionMatrix();
    mvpTransformationMatrix.proj[1][1] *= -1; // Flip Y for Vulkan

    memcpy( app->mvp_uboBuffers[executingCommandBufferIndex].gpuBufferMapped, &mvpTransformationMatrix, sizeof(mvpTransformationMatrix) );

    //std::cout << "Position of Camera: " << position.x << "x, " << position.y << "y, " << position.z << "z" << std::endl;
    //std::cout << "Quaternion Rotation of Camera: " << rotaQuat.w << ", " << rotaQuat.x << ", " << rotaQuat.y << ", " << rotaQuat.z << "\n\n";
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

    app->cmdBuffers.commandBuffers[executingCommandBufferIndex].bindVertexBuffers(0, *app->catModelHandle.get()->vertexBuffer.gpuBuffer, {0} );
    app->cmdBuffers.commandBuffers[executingCommandBufferIndex].bindIndexBuffer( *app->catModelHandle.get()->indexBuffer.gpuBuffer, 0, vk::IndexType::eUint32 );

    app->cmdBuffers.commandBuffers[executingCommandBufferIndex].bindDescriptorSets(
       vk::PipelineBindPoint::eGraphics, app->graphicPipeline.pipelineLayout, 0, *app->descriptors.descriptorSets[executingCommandBufferIndex], nullptr );

    app->cmdBuffers.commandBuffers[executingCommandBufferIndex].drawIndexed( app->catModelHandle.get()->indices_count, 1, 0, 0, 0 ); // todo: is there a better alternative for indices_count?

    app->cmdBuffers.commandBuffers[executingCommandBufferIndex].endRendering();
}

void RunTimeApplication::recordBoundingCommandBuffer( uint32_t currentImageIndex )
{
    // turn this this copy and pasted big block into a function.
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

    BoundingComponent* bound = app->cat.GetComponent<BoundingComponent>();
    app->cmdBuffers.commandBuffers[executingCommandBufferIndex].beginRendering( renderingInfo );
    app->cmdBuffers.commandBuffers[executingCommandBufferIndex].bindPipeline( vk::PipelineBindPoint::eGraphics, app->wireframePipeline.pipeline );
    app->cmdBuffers.commandBuffers[executingCommandBufferIndex].bindVertexBuffers(0, *bound->visualBox.get()->vertexBuffer.gpuBuffer, {0} ); // maybe make a resource handle for the bounding?
    app->cmdBuffers.commandBuffers[executingCommandBufferIndex].bindIndexBuffer( *bound->visualBox.get()->indexBuffer.gpuBuffer, 0, vk::IndexType::eUint32 );

    app->cmdBuffers.commandBuffers[executingCommandBufferIndex].pushConstants<glm::mat4>(
        app->wireframePipeline.pipelineLayout, vk::ShaderStageFlagBits::eVertex, 0,
        app->cat.GetComponent<TransformComponent>()->GetTransformMatrix() );

    app->cmdBuffers.commandBuffers[executingCommandBufferIndex].bindDescriptorSets(
       vk::PipelineBindPoint::eGraphics, app->wireframePipeline.pipelineLayout, 0, *app->wireframeDescriptors.descriptorSets[executingCommandBufferIndex], nullptr );
    app->cmdBuffers.commandBuffers[executingCommandBufferIndex].drawIndexed( bound->visualBox.get()->indices_count, 1, 0, 0, 0 );
    app->cmdBuffers.commandBuffers[executingCommandBufferIndex].endRendering();
}

void RunTimeApplication::recordCameraFrustumBoundingCommandBuffer( uint32_t currentImageIndex )
{
    // turn this this copy and pasted big block into a function.
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

    app->camera.GetComponent<CameraComponent>()->getCameraFrustum().updateVisualFrustumBounds( app->device );

    // TODO: have to also appropriately update the MVP.
    Frustum* localCameraBounds = &app->camera.GetComponent<CameraComponent>()->getCameraFrustum();

    Vertex* ptr = static_cast<Vertex*>(localCameraBounds->visualFrustum.get()->vertexBuffer.gpuBufferMapped);

    app->cmdBuffers.commandBuffers[executingCommandBufferIndex].beginRendering( renderingInfo );
    app->cmdBuffers.commandBuffers[executingCommandBufferIndex].bindPipeline( vk::PipelineBindPoint::eGraphics, app->wireframePipeline.pipeline );
    app->cmdBuffers.commandBuffers[executingCommandBufferIndex].bindVertexBuffers(0, *localCameraBounds->visualFrustum.get()->vertexBuffer.gpuBuffer, {0} ); // maybe make a resource handle for the bounding?
    app->cmdBuffers.commandBuffers[executingCommandBufferIndex].bindIndexBuffer( *localCameraBounds->visualFrustum.get()->indexBuffer.gpuBuffer, 0, vk::IndexType::eUint32 );

    app->cmdBuffers.commandBuffers[executingCommandBufferIndex].pushConstants<glm::mat4>(
        app->wireframePipeline.pipelineLayout, vk::ShaderStageFlagBits::eVertex, 0,
        glm::mat4(1) ); // Vertices are already in world space. We do not need to transform them within the shader.
        // TODO: It WOULD be better to transform it onto the GPU because it IS faster, but we MIGHT have to compute them into world space because of our frustum culling logic. Look into it maybe?

    app->cmdBuffers.commandBuffers[executingCommandBufferIndex].bindDescriptorSets(
       vk::PipelineBindPoint::eGraphics, app->wireframePipeline.pipelineLayout, 0, *app->wireframeDescriptors.descriptorSets[executingCommandBufferIndex], nullptr );
    app->cmdBuffers.commandBuffers[executingCommandBufferIndex].drawIndexed( localCameraBounds->visualFrustum.get()->indices_count, 1, 0, 0, 0 );
    app->cmdBuffers.commandBuffers[executingCommandBufferIndex].endRendering();
}

void RunTimeApplication::updateParticleBuffer()
{
    ParticleTime particleTime;
    particleTime.deltaTime = static_cast<float>(lastFrameTime) * 2.0f;
    memcpy( app->particle_storageBuffers_uboMule[executingCommandBufferIndex].gpuBufferMapped, &particleTime, sizeof(particleTime) );
}

void RunTimeApplication::updateWireMVPUBOBuffer()
{
    vpUBOBuffer vpTransformationMatrix{};

    //TODO: We need to separate the components. This causes the wireframe of the camera's frustum to also spin -- bad.
    vpTransformationMatrix.view = currentCamera->GetComponent<CameraComponent>()->getViewMatrix();
    vpTransformationMatrix.proj = currentCamera->GetComponent<CameraComponent>()->getProjectionMatrix();
    vpTransformationMatrix.proj[1][1] *= -1;

    memcpy( app->wireframe_vp_uboBuffers[executingCommandBufferIndex].gpuBufferMapped, &vpTransformationMatrix, sizeof(vpTransformationMatrix) );
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
    //app->cmdBuffers.commandBuffers[executingCommandBufferIndex].draw(PARTICLE_COUNT, 1, 0, 0 );
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
    updateWireMVPUBOBuffer();

    glm::vec3 guh = app->camera.GetComponent<TransformComponent>()->GetPosition();

    app->cullSystem.CullScene( app->allEntities );

    app->cmdBuffers.commandBuffers[executingCommandBufferIndex].begin({});

    app->threadManager.signalThreadsToWork();

    recordCatCommandBuffer( imageIndex );

    recordBoundingCommandBuffer( imageIndex );

    if ( toggle_c )
    {
        recordCameraFrustumBoundingCommandBuffer( imageIndex );
    }

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
        moving_cursor = false;
        glfwPollEvents();

        if ( moving_cursor == false && held_click )
        {
            cursor_clicked_at = glm::vec2( current_cursor_position.x, current_cursor_position.y ); // Fix rotation on multiple cameras.
        }

        if ( toggle_c )
            currentCamera = &app->globalCamera; // Global Camera (debugging)
        else
            currentCamera = &app->camera; // Local Camera (user view)

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