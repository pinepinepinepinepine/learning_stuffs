#include "../headers/renderer.hpp"

void RenderApplication::setup()
{
    window.initWindow();

    device.createVulkanInstance();
    window.createWindowSurface( device.instance );

    device.createLogicalDevice( &window.window_surface );

    createCommandPools();
    cmdBuffers.createCommandBuffers( device.logicalDevice, MAX_FRAMES_IN_FLIGHT ); // Dedicated Command Buffers

    swapChain.createSwapChain( device, window );
    createAttachmentImages();

    textureSampler = createTextureSampler();
    createTextures();
    createModels();

    createMVPUBOBuffers();
    createParticleComputeBuffers();
    createDebugBuffers();
    createWireframeMVPUBOBuffers();

    createDescriptorPool();
    createModelDescriptors();
    createParticleDescriptors();
    createWireframeDescriptors();

    createVertexGraphicsPipeline();
    createParticleGraphicsPipeline();
    createParticleComputePipeline();
    createWireframeGraphicsPipeline();

    createThreads();

    // Above is setup, below is actual stuff.
    createCatEntity();
    allEntities.push_back( &cat );
    cullSystem.setCamera( camera.GetComponent<CameraComponent>() );

    lightSystem.setDevice( &device );
    lightSystem.createOffscreenRenderPass();
    lightSystem.createOffscreenFrameBuffer();

    lightSystem.createOnscreenRenderPass( swapChain.surfaceFormat.format, graphicPipeline.depthFormat ); // getting it from the depth format is ugly asf. fix later.
    lightSystem.createOnscreenFrameBufferImages( swapChain.imageResolution, swapChain.surfaceFormat.format, graphicPipeline.depthFormat );
    lightSystem.createOnscreenFrameBuffer( swapChain );

    lightSystem.createBuffers( MAX_FRAMES_IN_FLIGHT );
    lightSystem.createDescriptors( descriptorPool, MAX_FRAMES_IN_FLIGHT );

    lightSystem.createCommandBuffer( device.logicalDevice, MAX_FRAMES_IN_FLIGHT );

    lightSystem.createLightCamera();
}

void RenderApplication::createTextures()
{
    auto catTexture = std::make_unique<Texture>();
    const char* catFilepath = "../textures/spinT.png";

    catTexture->createTextureImage( device, catFilepath );
    catTexture->setTextureSampler( textureSampler );
    catTextureHandle = textureManager.addResource( hashString(catFilepath), std::move(catTexture) );

    auto poTexture = std::make_unique<Texture>();
    const char* poFilepath = "../textures/guh.png";
    poTexture->createTextureImage( device, poFilepath );
    poTexture->setTextureSampler( textureSampler );
    poTextureHandle = textureManager.addResource( hashString(poFilepath), std::move(poTexture) );
}

void RenderApplication::createModels()
{
    auto catModel = std::make_unique<ModelData>();
    const char* catFilepath = "../models/spin.obj";

    // Probably a really good idea to ship some logic from createCatEntity (specifically about model components) into here.
    cat.addComponent<BoundingComponent>( catModel->loadModel( device, catFilepath ) ); // Maybe make loadModel return it directly instead of needing to make a separate variable?
    cat.GetComponent<BoundingComponent>()->createBoundingBuffer( device );
    catModelHandle = modelManager.addResource( hashString( catFilepath ), std::move(catModel) );
}


void RenderApplication::createCatEntity()
{
    cat.addComponent<ModelComponent>( catModelHandle.get() );
    cat.addComponent<TextureComponent>( poTextureHandle.get() );
    cat.addComponent<TransformComponent>( glm::vec3(0.0f), glm::quat(1.0f, 0.0f, 0.0f, 0.0f), glm::vec3(1.0f) ); // Fix this. Temporary, just using default.
    cat.addComponent<CameraComponent>();
    cat.addComponent<RenderComponent>( &graphicPipeline, &descriptors );

    camera.addComponent<TransformComponent>( glm::vec3(0.0f), glm::quat(1.0f, 0.0f, 0.0f, 0.0f), glm::vec3(1.0f) );
    camera.GetComponent<TransformComponent>()->SetPosition( {0, 35, -5} );
    //camera.GetComponent<TransformComponent>()->SetRotation( { 0.0f, 0.0f, -1.0f, 0.0f } );
    camera.addComponent<CameraComponent>();
    camera.GetComponent<CameraComponent>()->setPerspective( 50.0f, 16.0 / 9.0f, 0.1f, 350.0f );
    camera.GetComponent<CameraComponent>()->getCameraFrustum().createFrustumBuffer( device );
    camera.GetComponent<CameraComponent>()->getCameraFrustum().createFrustum();
    // Probably a WAY better idea to stop repeatedly calling getComponents and to cache it via a variable. Fix later.

    globalCamera.addComponent<TransformComponent>( glm::vec3(0.0f), glm::quat(1.0f, 0.0f, 0.0f, 0.0f), glm::vec3(1.0f) );
    globalCamera.GetComponent<TransformComponent>()->SetPosition( { 0, 35, 350 } );
    //globalCamera.GetComponent<TransformComponent>()->SetRotation( { 0.0f, 0.0f, -1.0f, 0.0f } ); // FUCK THIS KEEP IT REMOVED BECAUSE IT'S FUCKING UP OUR ROTATION DUE TO ROTATION * FORWARD
    globalCamera.addComponent<CameraComponent>();
    globalCamera.GetComponent<CameraComponent>()->setPerspective( 50.0f, 16.0 / 9.0f, 0.1f, 1000.0f );
}

void RenderApplication::cleanup()
{
    threadManager.stopThreads();

    swapChain.cleanupSwapChainViews(); // Raii EXPLICITLY wants to delete the swap chain images itself.
    catTextureHandle.get()->textureImage.cleanupImage( device.logicalDevice );
    poTextureHandle.get()->textureImage.cleanupImage( device.logicalDevice );
    colourImage.cleanupImage( device.logicalDevice );
    depthImage.cleanupImage( device.logicalDevice );

    vkDestroySurfaceKHR( *device.instance, window.window_surface, nullptr );
    glfwDestroyWindow( window.window );
    glfwTerminate();
}

void RenderApplication::createAttachmentImages()
{
    colourImage.createImage( device, swapChain.imageResolution.width, swapChain.imageResolution.height,
        1, device.msaaSamples, swapChain.surfaceFormat.format, vk::ImageTiling::eOptimal,
        vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eTransientAttachment, vk::MemoryPropertyFlagBits::eDeviceLocal, vk::ImageAspectFlagBits::eColor );

    vk::Format depthFormat = device.findSupportedFormat( // Maybe make this into a member? We re-use this, but it's not important or anything -- memory too...
        { vk::Format::eD32Sfloat, vk::Format::eD32SfloatS8Uint, vk::Format::eD24UnormS8Uint },
        vk::ImageTiling::eOptimal, vk::FormatFeatureFlagBits::eDepthStencilAttachment );
    depthImage.createImage(
        device, swapChain.imageResolution.width, swapChain.imageResolution.height,
        1, device.msaaSamples, depthFormat, vk::ImageTiling::eOptimal,
        vk::ImageUsageFlagBits::eDepthStencilAttachment, vk::MemoryPropertyFlagBits::eDeviceLocal, vk::ImageAspectFlagBits::eDepth ); // TODO: Should DepthImage also be eTransientAttachment?
}

vk::raii::Sampler RenderApplication::createTextureSampler()
{
    vk::PhysicalDeviceProperties properties = device.physicalDevice.getProperties();

    vk::SamplerCreateInfo samplerInfo {
        .magFilter = vk::Filter::eLinear,
        .minFilter = vk::Filter::eLinear,
        .addressModeU = vk::SamplerAddressMode::eRepeat,
        .addressModeV = vk::SamplerAddressMode::eRepeat,
        .addressModeW = vk::SamplerAddressMode::eRepeat,
        .anisotropyEnable = vk::True,
        .maxAnisotropy = properties.limits.maxSamplerAnisotropy,
        .compareEnable = vk::False,
        .compareOp = vk::CompareOp::eAlways,
        .borderColor = vk::BorderColor::eIntOpaqueBlack,
        .unnormalizedCoordinates = vk::False };
    samplerInfo.mipmapMode = vk::SamplerMipmapMode::eLinear;
    samplerInfo.mipLodBias = 0.0f;
    samplerInfo.minLod = 0.0f;
    samplerInfo.maxLod = vk::LodClampNone;

    return vk::raii::Sampler( device.logicalDevice, samplerInfo );
}

void RenderApplication::createCommandPools()
{
    dedicatedCommandPool = CommandPool::createCommandPool( device.logicalDevice, device.queueIndex, vk::CommandPoolCreateFlagBits::eResetCommandBuffer );
    transientCommandPool = CommandPool::createCommandPool( device.logicalDevice, device.queueIndex,
        vk::CommandPoolCreateFlagBits::eResetCommandBuffer | vk::CommandPoolCreateFlagBits::eTransient );
    TransientCommandBuffer::initialize( transientCommandPool, device.logicalDevice, device.queue );
    DedicatedCommandBuffers::initialize( dedicatedCommandPool );
}

void RenderApplication::createMVPUBOBuffers()
{
    mvp_uboBuffers.clear();
    for ( size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++ )
    {
        GPUBuffer individualBuffer;
        individualBuffer.createGPUBuffer(
            device,
            sizeof(mvpUBOBuffer),
            vk::BufferUsageFlagBits::eUniformBuffer | vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eVertexBuffer,
            vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent, // TODO: make this device local probs. mule the data over.
            true );
        mvp_uboBuffers.emplace_back( std::move(individualBuffer) );
    }
}

void RenderApplication::createParticleComputeBuffers()
{
    std::default_random_engine     rndEngine(static_cast<unsigned>(time(nullptr)));
	std::uniform_real_distribution rndDist(0.0f, 1.0f);

    // Initial particle positions on a circle
	std::vector<Particle> particles(PARTICLE_COUNT);
    for (auto &particle : particles)
    {
        float r           = 0.25f * sqrtf(rndDist(rndEngine));
        float theta       = rndDist(rndEngine) * 2.0f * 3.14159265358979323846f;
        float x           = r * cosf(theta) * HEIGHT / WIDTH;
        float y           = r * sinf(theta);
        particle.position = glm::vec2(x, y);
        particle.velocity = normalize(glm::vec2(x, y)) * 0.00025f;
        particle.color    = glm::vec4(rndDist(rndEngine), rndDist(rndEngine), rndDist(rndEngine), 1.0f);
    }
    GPUBuffer computeMule;
    computeMule.createGPUBuffer( device, sizeof(Particle) * PARTICLE_COUNT, vk::BufferUsageFlagBits::eTransferSrc,
    vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent, true );
	memcpy( computeMule.gpuBufferMapped, particles.data(), (size_t) (sizeof(Particle) * PARTICLE_COUNT));
	computeMule.unmapGPUMemory();

    particle_storageBuffers_currentFrame.clear();
    particle_storageBuffers_uboMule.clear();
    for ( size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++ )
    {
        GPUBuffer individualStorageBuffer;
        individualStorageBuffer.createGPUBuffer(
            device,
            sizeof(Particle) * PARTICLE_COUNT,
            vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eVertexBuffer, // Why vertexBuffer? figure out later.
            vk::MemoryPropertyFlagBits::eDeviceLocal,
            true );
        individualStorageBuffer.copyBufferInto( computeMule.gpuBuffer, sizeof(Particle) * PARTICLE_COUNT );
        particle_storageBuffers_currentFrame.emplace_back( std::move(individualStorageBuffer) );

        GPUBuffer individualUBOBuffer;
        individualUBOBuffer.createGPUBuffer(
            device,
            sizeof(ParticleTime),
            vk::BufferUsageFlagBits::eUniformBuffer,
            vk::MemoryPropertyFlagBits::eHostCoherent | vk::MemoryPropertyFlagBits::eHostVisible,
            true );
        particle_storageBuffers_uboMule.emplace_back( std::move(individualUBOBuffer) );
    }
}

void RenderApplication::createDebugBuffers()
{
    debug_uboBuffers.clear();
    for ( size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++ )
    {
        GPUBuffer individualBuffer;
        individualBuffer.createGPUBuffer(
            device,
            sizeof(Vertex) * catModelHandle.get()->vertices_count,
            vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eStorageBuffer,
            vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
            true );
        debug_uboBuffers.emplace_back( std::move(individualBuffer) );
    }

    particle_debugComputeBuffers.clear();
    for ( size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++ )
    {
        GPUBuffer individualBuffer;
        individualBuffer.createGPUBuffer(
            device,
            sizeof(glm::vec2) * PARTICLE_COUNT,
            vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eStorageBuffer,
            vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
            true );
        particle_debugComputeBuffers.emplace_back( std::move(individualBuffer) );
    }

    particle_debugGraphicsBuffers.clear();
    for ( size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++ )
    {
        GPUBuffer individualBuffer;
        individualBuffer.createGPUBuffer(
            device,
            sizeof(glm::vec2) * PARTICLE_COUNT,
            vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eStorageBuffer,
            vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
            true );
        particle_debugGraphicsBuffers.emplace_back( std::move(individualBuffer) );
    }
}

// Abstract this. It's all a repeat of already existing code.
void RenderApplication::createWireframeMVPUBOBuffers()
{
    wireframe_vp_uboBuffers.clear();
    for ( size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++ )
    {
        GPUBuffer individualBuffer;
        individualBuffer.createGPUBuffer(
            device,
            sizeof(vpUBOBuffer),
            vk::BufferUsageFlagBits::eUniformBuffer | vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eVertexBuffer,
            vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
            true );
        wireframe_vp_uboBuffers.emplace_back( std::move(individualBuffer) );
    }
}

void RenderApplication::createDescriptorPool()
{
    // Models
    vk::DescriptorPoolSize uboPoolSize( vk::DescriptorType::eUniformBuffer, MAX_FRAMES_IN_FLIGHT );
    vk::DescriptorPoolSize samplerPoolSize( vk::DescriptorType::eCombinedImageSampler, MAX_FRAMES_IN_FLIGHT );
    vk::DescriptorPoolSize debugUBOPoolSize( vk::DescriptorType::eStorageBuffer, MAX_FRAMES_IN_FLIGHT );

    // Particles
    vk::DescriptorPoolSize particleStoragePoolSize( vk::DescriptorType::eStorageBuffer, MAX_FRAMES_IN_FLIGHT * 2 );
    vk::DescriptorPoolSize particleUBOPoolSize( vk::DescriptorType::eUniformBuffer, MAX_FRAMES_IN_FLIGHT );
    vk::DescriptorPoolSize particleDebugPoolSize( vk::DescriptorType::eStorageBuffer, MAX_FRAMES_IN_FLIGHT );
    // Particles Graphics
    vk::DescriptorPoolSize particleGraphicsDebugPoolSize( vk::DescriptorType::eStorageBuffer, MAX_FRAMES_IN_FLIGHT );

    // Wireframe
    vk::DescriptorPoolSize wire_uboPoolSize( vk::DescriptorType::eUniformBuffer, MAX_FRAMES_IN_FLIGHT );

    // Shadow
    vk::DescriptorPoolSize shadow_uboPoolSize( vk::DescriptorType::eUniformBuffer, MAX_FRAMES_IN_FLIGHT * 2 );
    vk::DescriptorPoolSize shadow_samplerPoolSize( vk::DescriptorType::eCombinedImageSampler, MAX_FRAMES_IN_FLIGHT * 2 );
    vk::DescriptorPoolSize shadow_sampledImagePoolSize( vk::DescriptorType::eSampledImage, MAX_FRAMES_IN_FLIGHT * 2 );

    std::vector<vk::DescriptorPoolSize> poolSize {
        uboPoolSize, samplerPoolSize, debugUBOPoolSize, particleStoragePoolSize, particleUBOPoolSize, particleDebugPoolSize, particleGraphicsDebugPoolSize, wire_uboPoolSize,
        shadow_uboPoolSize, shadow_samplerPoolSize, shadow_sampledImagePoolSize };
    descriptorPool = DescriptorPool::createDescriptorPool( device.logicalDevice, poolSize, MAX_FRAMES_IN_FLIGHT * 6 ); // *2 because of models + particles, a distinct set per. check if right.
}

void RenderApplication::createModelDescriptors()
{
    descriptors.setDescriptorsPool( descriptorPool );

    vk::DescriptorSetLayoutBinding uboLayoutBinding(
        0,
        vk::DescriptorType::eUniformBuffer,
        1,
        vk::ShaderStageFlagBits::eVertex,
        nullptr );
    vk::DescriptorSetLayoutBinding samplerLayoutBinding(
        1,
        vk::DescriptorType::eCombinedImageSampler,
        1,
        vk::ShaderStageFlagBits::eFragment,
        nullptr );
    vk::DescriptorSetLayoutBinding debugUBOLayoutBinding(
        2,
        vk::DescriptorType::eStorageBuffer,
        1,
        vk::ShaderStageFlagBits::eVertex,
        nullptr );
    std::vector<vk::DescriptorSetLayoutBinding> layoutBindings { uboLayoutBinding, samplerLayoutBinding, debugUBOLayoutBinding };
    descriptors.createDescriptorSetLayout( device.logicalDevice, layoutBindings );

    descriptors.createEmptyDescriptorSets( device.logicalDevice, MAX_FRAMES_IN_FLIGHT );

    for ( int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++ )
    {
        descriptors.setBufferResource( device.logicalDevice, mvp_uboBuffers[i].gpuBuffer, vk::DescriptorType::eUniformBuffer, sizeof(mvpUBOBuffer), i, 0 );
        descriptors.setSamplerResource( device.logicalDevice, *catTextureHandle.get()->textureSampler, catTextureHandle.get()->textureImage.imageView, i, 1 );
        descriptors.setBufferResource( device.logicalDevice, debug_uboBuffers[i].gpuBuffer, vk::DescriptorType::eStorageBuffer, sizeof(Vertex) * catModelHandle.get()->vertices_count, i, 2 );
    }
}

void RenderApplication::createParticleDescriptors()
{
    particleComputeDescriptors.setDescriptorsPool( descriptorPool );

    vk::DescriptorSetLayoutBinding particleStorageLayoutBinding_previousFrame(
        0,
        vk::DescriptorType::eStorageBuffer,
        1,
        vk::ShaderStageFlagBits::eCompute,
        nullptr );
    vk::DescriptorSetLayoutBinding particleStorageLayoutBinding_currentFrame(
        1,
        vk::DescriptorType::eStorageBuffer,
        1, // TODO: We can use this instead (make it a 2, bundle it with prevFrame), but it'd require a change-up for our compute shader code.
        vk::ShaderStageFlagBits::eCompute,
        nullptr );
    vk::DescriptorSetLayoutBinding particleStorageLayoutBinding_uboMule(
        2,
        vk::DescriptorType::eUniformBuffer,
        1,
        vk::ShaderStageFlagBits::eCompute,
        nullptr );
    vk::DescriptorSetLayoutBinding particleStorageLayoutBinding_debug(
        3,
        vk::DescriptorType::eStorageBuffer,
        1,
        vk::ShaderStageFlagBits::eCompute,
        nullptr );
    std::vector<vk::DescriptorSetLayoutBinding> layoutBindings { particleStorageLayoutBinding_previousFrame, particleStorageLayoutBinding_currentFrame, particleStorageLayoutBinding_uboMule, particleStorageLayoutBinding_debug };
    particleComputeDescriptors.createDescriptorSetLayout( device.logicalDevice, layoutBindings );

    particleComputeDescriptors.createEmptyDescriptorSets( device.logicalDevice, MAX_FRAMES_IN_FLIGHT );

    for ( int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++ )
    {
        particleComputeDescriptors.setBufferResource( device.logicalDevice, particle_storageBuffers_currentFrame[(i + MAX_FRAMES_IN_FLIGHT - 1) % MAX_FRAMES_IN_FLIGHT].gpuBuffer, vk::DescriptorType::eStorageBuffer, sizeof(Particle) * PARTICLE_COUNT, i, 0 ); // TODO: fix the name, it's a general purpose setResource, not UBO exclusive.
        particleComputeDescriptors.setBufferResource( device.logicalDevice, particle_storageBuffers_currentFrame[i].gpuBuffer, vk::DescriptorType::eStorageBuffer, sizeof(Particle) * PARTICLE_COUNT, i, 1 );
        particleComputeDescriptors.setBufferResource( device.logicalDevice, particle_storageBuffers_uboMule[i].gpuBuffer, vk::DescriptorType::eUniformBuffer, sizeof(ParticleTime), i, 2 );
        particleComputeDescriptors.setBufferResource( device.logicalDevice, particle_debugComputeBuffers[i].gpuBuffer, vk::DescriptorType::eStorageBuffer, sizeof(glm::vec2) * PARTICLE_COUNT, i, 3 );
    }

    particleGraphicDescriptors.setDescriptorsPool( descriptorPool );
    vk::DescriptorSetLayoutBinding particleStorageLayoutBinding_debugGraphics(
        0,
        vk::DescriptorType::eStorageBuffer,
        1,
        vk::ShaderStageFlagBits::eVertex,
        nullptr );
    std::vector<vk::DescriptorSetLayoutBinding> graphicsLayoutBindings {particleStorageLayoutBinding_debugGraphics};
    particleGraphicDescriptors.createDescriptorSetLayout( device.logicalDevice, graphicsLayoutBindings );
    particleGraphicDescriptors.createEmptyDescriptorSets( device.logicalDevice, MAX_FRAMES_IN_FLIGHT );
    for ( int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++ )
    {
        particleGraphicDescriptors.setBufferResource( device.logicalDevice, particle_debugGraphicsBuffers[i].gpuBuffer, vk::DescriptorType::eStorageBuffer, sizeof(glm::vec2) * PARTICLE_COUNT, i, 0 );
    }
}

void RenderApplication::createWireframeDescriptors()
{
    wireframeDescriptors.setDescriptorsPool( descriptorPool );

    vk::DescriptorSetLayoutBinding wire_uboLayoutBinding(
        0,
        vk::DescriptorType::eUniformBuffer,
        1,
        vk::ShaderStageFlagBits::eVertex,
        nullptr );

    std::vector<vk::DescriptorSetLayoutBinding> layoutBindings { wire_uboLayoutBinding };
    wireframeDescriptors.createDescriptorSetLayout( device.logicalDevice, layoutBindings );

    wireframeDescriptors.createEmptyDescriptorSets( device.logicalDevice, MAX_FRAMES_IN_FLIGHT );

    for ( int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++ )
    {
        wireframeDescriptors.setBufferResource( device.logicalDevice, wireframe_vp_uboBuffers[i].gpuBuffer, vk::DescriptorType::eUniformBuffer, sizeof(vpUBOBuffer), i, 0 );
    }
}



void RenderApplication::createVertexGraphicsPipeline()
{
    std::vector<vk::DescriptorSetLayout> vertexPipelineDescriptorSetLayouts { descriptors.descriptorSetLayout };
    std::vector<vk::PushConstantRange> vertexPushConstants{};
    graphicPipeline.createPipelineDescriptorLayout( device.logicalDevice, vertexPipelineDescriptorSetLayouts, vertexPushConstants );

    auto bindingDescription    = TextureVertex::getBindingDescription();
    auto attributeDescriptions = TextureVertex::getAttributeDescriptions();
    vk::PipelineVertexInputStateCreateInfo vertexInputInfo {
        .vertexBindingDescriptionCount   = 1, .pVertexBindingDescriptions = &bindingDescription,
        .vertexAttributeDescriptionCount = static_cast<uint32_t>( attributeDescriptions.size() ), .pVertexAttributeDescriptions = attributeDescriptions.data() };

    vk::raii::ShaderModule shaderModules = PipelineUtils::createShaderModule( device.logicalDevice, "../shaders/slang.spv" );

    graphicPipeline.createGraphicsPipeline( device, shaderModules, swapChain, vk::PrimitiveTopology::eTriangleList, vertexInputInfo );
}

void RenderApplication::createParticleGraphicsPipeline()
{
    std::vector<vk::DescriptorSetLayout> vertexPipelineDescriptorSetLayouts { particleGraphicDescriptors.descriptorSetLayout };
    std::vector<vk::PushConstantRange> particleGraphicsPushConstants{};
    particleGraphicPipeline.createPipelineDescriptorLayout( device.logicalDevice, vertexPipelineDescriptorSetLayouts, particleGraphicsPushConstants );

    auto bindingDescription    = Particle::getBindingDescription();
    auto attributeDescriptions = Particle::getAttributeDescriptions();
    vk::PipelineVertexInputStateCreateInfo vertexInputInfo {
        .vertexBindingDescriptionCount   = 1, .pVertexBindingDescriptions = &bindingDescription,
        .vertexAttributeDescriptionCount = static_cast<uint32_t>( attributeDescriptions.size() ), .pVertexAttributeDescriptions = attributeDescriptions.data() };

    vk::raii::ShaderModule shaderModules = PipelineUtils::createShaderModule( device.logicalDevice, "../shaders/particle_graphics.spv" );

    particleGraphicPipeline.createGraphicsPipeline( device, shaderModules, swapChain, vk::PrimitiveTopology::eTriangleList, vertexInputInfo );
}

void RenderApplication::createParticleComputePipeline()
{
    std::vector<vk::DescriptorSetLayout> computePipelineDescriptorSetLayouts { particleComputeDescriptors.descriptorSetLayout };

    vk::PushConstantRange pushConstantRange {
		.stageFlags = vk::ShaderStageFlagBits::eCompute,
		.offset     = 0,
		.size       = sizeof(uint32_t) * 2 }; // struct PushConstants: startIndex and count

    std::vector<vk::PushConstantRange> particleComputePushConstants{ pushConstantRange };
    particleComputePipeline.createPipelineDescriptorLayout( device.logicalDevice, computePipelineDescriptorSetLayouts, particleComputePushConstants );

    vk::raii::ShaderModule shaderModules = PipelineUtils::createShaderModule( device.logicalDevice, "../shaders/particle_compute.spv" );
    particleComputePipeline.createComputePipeline( device.logicalDevice, shaderModules );
}

void RenderApplication::createWireframeGraphicsPipeline()
{
    std::vector<vk::DescriptorSetLayout> wireframePipelineDescriptorSetLayouts { wireframeDescriptors.descriptorSetLayout };

    vk::PushConstantRange pushConstantRange {
		.stageFlags = vk::ShaderStageFlagBits::eVertex,
		.offset     = 0,
		.size       = sizeof(glm::mat4) }; // Model Transformation Matrix (passing as a push constant, view and proj are descriptors.)

    std::vector<vk::PushConstantRange> wireframePushConstants{ pushConstantRange };

    wireframePipeline.createPipelineDescriptorLayout( device.logicalDevice, wireframePipelineDescriptorSetLayouts, wireframePushConstants );

    auto bindingDescription    = Vertex::getBindingDescription();
    auto attributeDescriptions = Vertex::getAttributeDescriptions();

    vk::PipelineVertexInputStateCreateInfo vertexInputInfo {
        .vertexBindingDescriptionCount   = 1, .pVertexBindingDescriptions = &bindingDescription,
        .vertexAttributeDescriptionCount = static_cast<uint32_t>( attributeDescriptions.size() ), .pVertexAttributeDescriptions = attributeDescriptions.data() };

    vk::raii::ShaderModule shaderModules = PipelineUtils::createShaderModule( device.logicalDevice, "../shaders/wireframe.spv" );
    wireframePipeline.createGraphicsPipeline( device, shaderModules, swapChain, vk::PrimitiveTopology::eLineList, vertexInputInfo );
}


void RenderApplication::createThreads()
{
    threadManager.createThreadCommandPools( device.logicalDevice, device.queueIndex, THREAD_COUNT );
    threadManager.allocateCommandBuffers( device.logicalDevice, THREAD_COUNT, 1 );

    threadManager.threadWorkReady = std::vector<std::atomic<bool>>(THREAD_COUNT);
    threadManager.threadWorkDone = std::vector<std::atomic<bool>>(THREAD_COUNT);

    threadManager.particleGroups.resize(PARTICLE_COUNT / THREAD_COUNT);
}