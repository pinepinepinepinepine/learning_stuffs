#include "../headers/renderer.hpp"

void RenderApplication::setup()
{
    window.initWindow();

    device.createVulkanInstance();
    window.createWindowSurface( device.instance );

    device.createLogicalDevice( &window.window_surface );

    createCommandPools();
    createDedicatedCommandBuffers();

    swapChain.createSwapChain( device, window );
    createAttachmentImages();

    catTexture.createTextureImage( device, "../textures/spinT.png" );
    textureSampler = createTextureSampler();
    catTexture.setTextureSampler( textureSampler );

    catModel.loadModel( device, "../models/spin.obj" );

    createMVPUBOBuffers();

    createDebugBuffers();

    createDescriptors();

    createVertexGraphicsPipeline();
}

void RenderApplication::createVertexGraphicsPipeline()
{
    std::vector<vk::DescriptorSetLayout> vertexPipelineDescriptorSetLayouts { descriptors.descriptorSetLayout };
    graphicPipeline.createPipelineDescriptorLayout( device.logicalDevice, vertexPipelineDescriptorSetLayouts );

    auto bindingDescription    = Vertex::getBindingDescription();
    auto attributeDescriptions = Vertex::getAttributeDescriptions();
    vk::PipelineVertexInputStateCreateInfo vertexInputInfo {
        .vertexBindingDescriptionCount   = 1, .pVertexBindingDescriptions = &bindingDescription,
        .vertexAttributeDescriptionCount = static_cast<uint32_t>( attributeDescriptions.size() ), .pVertexAttributeDescriptions = attributeDescriptions.data() };

    vk::raii::ShaderModule shaderModules = PipelineUtils::createShaderModule( device.logicalDevice, "../shaders/slang.spv" ); // WE NEED TO CREATE A SHADER MODULE + COMPILE.BAT

    graphicPipeline.createGraphicsPipeline( device, shaderModules, swapChain, vertexInputInfo );
}

void RenderApplication::createCommandPools()
{
    dedicatedCommandPool = CommandPool::createCommandPool( device.logicalDevice, device.queueIndex, vk::CommandPoolCreateFlagBits::eResetCommandBuffer );
    transientCommandPool = CommandPool::createCommandPool( device.logicalDevice, device.queueIndex,
        vk::CommandPoolCreateFlagBits::eResetCommandBuffer | vk::CommandPoolCreateFlagBits::eTransient );

    TransientCommandBuffer::initialize( transientCommandPool, device.logicalDevice, device.queue );
    DedicatedCommandBuffers::initialize( dedicatedCommandPool );
}

void RenderApplication::createDedicatedCommandBuffers()
{
    cmdBuffers.createCommandBuffers( device.logicalDevice, MAX_FRAMES_IN_FLIGHT );
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
            vk::BufferUsageFlagBits::eUniformBuffer,
            vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
            true );

        mvp_uboBuffers.emplace_back( std::move(individualBuffer) );
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
            sizeof(Vertex) * catModel.vertices_count,
            vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eStorageBuffer,
            vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
            true );

        debug_uboBuffers.emplace_back( std::move(individualBuffer) );
    }
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

void RenderApplication::createDescriptors()
{
    vk::DescriptorPoolSize uboPoolSize( vk::DescriptorType::eUniformBuffer, MAX_FRAMES_IN_FLIGHT );
    vk::DescriptorPoolSize samplerPoolSize( vk::DescriptorType::eCombinedImageSampler, MAX_FRAMES_IN_FLIGHT );
    vk::DescriptorPoolSize debugUBOPoolSize( vk::DescriptorType::eStorageBuffer, MAX_FRAMES_IN_FLIGHT );
    std::vector<vk::DescriptorPoolSize> poolSize { uboPoolSize, samplerPoolSize, debugUBOPoolSize };
    descriptorPool = DescriptorPool::createDescriptorPool( device.logicalDevice, poolSize, MAX_FRAMES_IN_FLIGHT );
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
        descriptors.setUBOResource( device.logicalDevice, mvp_uboBuffers[i].gpuBuffer, vk::DescriptorType::eUniformBuffer, sizeof(mvpUBOBuffer), i, 0 );
        descriptors.setSamplerResource( device.logicalDevice, *catTexture.textureSampler, catTexture.textureImage.imageView, i, 1 );
        descriptors.setUBOResource( device.logicalDevice, debug_uboBuffers[i].gpuBuffer, vk::DescriptorType::eStorageBuffer, sizeof(Vertex) * catModel.vertices_count, i, 2 );
    }
}

void RenderApplication::cleanup()
{
    swapChain.cleanupSwapChainViews(); // Raii EXPLICITLY wants to delete the swap chain images itself.
    catTexture.textureImage.cleanupImage( device.logicalDevice );
    colourImage.cleanupImage( device.logicalDevice );
    depthImage.cleanupImage( device.logicalDevice );

    vkDestroySurfaceKHR( *device.instance, window.window_surface, nullptr );
    glfwDestroyWindow( window.window );
    glfwTerminate();
}
