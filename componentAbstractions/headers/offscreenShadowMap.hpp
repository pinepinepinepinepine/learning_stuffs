#pragma once

#include "includes.hpp"
#include "entity.hpp"
#include "../../headers/image.hpp"
#include "../../headers/descriptors.hpp"
#include "../../headers/pipeline.hpp"
#include "../../headers/commandBuffers.hpp"


    // https://github.com/SaschaWillems/Vulkan/blob/master/examples/shadowmapping/shadowmapping.cpp
// We're using traditional render passes for now because I'm not sure how to dynamically do this. Convert after.
// Idea is we FIRST render the scene onto this image, which is entirely off screen (offscreen rendering) FROM the light's point of view.
// We see how close in the pixel is to the camera in world space and the closer it is, the more illuminated (lower depth value) is given to that pixel.
// Afterwards, on our second render pass, we pass this attachment, and the fragment shader will compare the positions of the pixels coordinates:
// First, when the fragment shader sees a pixel, it compares it to the depthShadowMapAttachment image as to see what the light originally recorded as the closest distance at that angle.
// Then, we take that pixel and see how far that pixel is from the light.


// Fix this. Redefinition. This is for offscreen's GPU buffers.
// Maybe it's a better idea to calculate the * MVP to save the GPU from doing it? IT IS. REFACTOR AFTER COMPLETION.
struct mvpBuffer {
    glm::mat4 model;
    glm::mat4 view;
    glm::mat4 proj;
};

// This is for onscreen. Copy the entity's data from Transform and Camera component -- it houses this data.
struct OnscreenBuffer
{
    glm::mat4 projection;
    glm::mat4 view;
    glm::mat4 model;
    glm::mat4 depthBiasMVP;
    glm::vec4 lightPos;
    // Used for depth map visualization
    float zNear;
    float zFar;
};

struct OffscreenPass
{
    uint32_t width, height;
    vk::raii::Framebuffer frameBuffer = nullptr;
    Image depthShadowMapAttachment;
    vk::raii::RenderPass renderPass = nullptr;
    vk::raii::Sampler depthSampler = nullptr;
    Descriptor descriptor;

    std::vector<GPUBuffer> offscreen_Buffers;
    // Make a separate pipeline. We're not re-using the same one because a depth image can skip out a bunch of stages, so it's WAY faster to just use a custom made one.
    Pipeline depthMapPipeline; // used to create the depth image
};

struct OnscreenPass
{
    Descriptor descriptor;
    std::vector<GPUBuffer> onscreen_Buffers;
    Pipeline shadowPassPipeline;
    vk::raii::RenderPass renderPass = nullptr;

    std::vector<vk::raii::Framebuffer> frameBuffer;

    // Image colourResolveAttachment is NOT needed because we are rendering onto the swap chain's image, which IS e1.
    Image colourAttachment; // 8x samples
    Image depthAttachment; // 8x samples
};

class LightingSystem
{
    Entity lightCamera {"lightCamera"};

    OffscreenPass depthRenderPass;

    OnscreenPass shadowRenderPass;

    vk::raii::PipelineLayout pipelineLayout { VK_NULL_HANDLE };

    LogicalDevice* device;

    // Depth bias (and slope) are used to avoid shadowing artifacts
    static constexpr float depthBiasConstant = 1.25f; // Constant depth bias factor (always applied)
    static constexpr float depthBiasSlope = 1.75f; // Slope depth bias factor, applied depending on polygon's slope

    // We're not reusing the dynamically used command buffers because we're pre-recording commands for traditional render passes.
    DedicatedCommandBuffers cmdBuffer; // Maybe don't static the pools? I dunno. This is pointless because we are removing this after.

    public:

    void setDevice( LogicalDevice* d ) { device = d; }

    ~LightingSystem()
    {
        // Tutorial uses a cool destructor instead of a cleanup function -- might be handy to remove the cleanup function within runtime/renderer to a destructor!
        depthRenderPass.depthShadowMapAttachment.cleanupImage( device->logicalDevice );

        shadowRenderPass.colourAttachment.cleanupImage( device->logicalDevice );
        shadowRenderPass.depthAttachment.cleanupImage( device->logicalDevice );
    }

    void createOffscreenRenderPass()
    {
        vk::AttachmentDescription attachmentDescription {
            .format = vk::Format::eD16Unorm,
            .samples = vk::SampleCountFlagBits::e1,
            .loadOp = vk::AttachmentLoadOp::eClear,
            .storeOp = vk::AttachmentStoreOp::eStore, // We will read from depth, so it's important to store the depth attachment results
            .stencilLoadOp = vk::AttachmentLoadOp::eDontCare,
            .stencilStoreOp = vk::AttachmentStoreOp::eDontCare,
            .initialLayout = vk::ImageLayout::eUndefined,
            .finalLayout = vk::ImageLayout::eDepthStencilReadOnlyOptimal };

        vk::AttachmentReference depthReference {
            .attachment = 0,
            .layout = vk::ImageLayout::eDepthStencilAttachmentOptimal }; // Attachment will be used as depth/stencil during render pass

        // Subpasses are deprecated due to dynamic rendering being favoured, and so subpasses are also... GONE! Theyre essentially pipeline barriers.
        // Whenever we finish the base, switch to dynamic rendering.
        vk::SubpassDescription subpass {
            .pipelineBindPoint = vk::PipelineBindPoint::eGraphics,
            .colorAttachmentCount = 0, // No color attachments (ONLY used for storing depth)
            .pDepthStencilAttachment = &depthReference };

        // Subpass dependency are essentially pipeline barriers but for render passes.
        vk::SubpassDependency initial {
            .srcSubpass = VK_SUBPASS_EXTERNAL, // GPU needs to finish every operation happening before modifying the depth image -- ensure it's finished before starting.
            .dstSubpass = 0, // Once srcSubpass is finished, dstSubpass is allowed to begin -- subpass 0 can start drawing.

            // Everything below is specifically what needs to be waited for:
                // Just to clarify: OPERATIONS ON THIS IMAGE STILL OCCUR, WE ARE CREATING A BARRIER BETWEEN THESE
            .srcStageMask = vk::PipelineStageFlagBits::eFragmentShader, // The fragment Shader must finish executing before...
            .dstStageMask = vk::PipelineStageFlagBits::eEarlyFragmentTests, // ...before we do fragment tests to check for depth

            .srcAccessMask = vk::AccessFlagBits::eShaderRead, // We must read from the shader before...
            .dstAccessMask = vk::AccessFlagBits::eDepthStencilAttachmentWrite, // ... Before we start writing and modifying the image

            .dependencyFlags = vk::DependencyFlagBits::eByRegion }; // Don't pause the ENTIRE GPU, pause regions (so it's fast as we don't stall the entire GPU)

        vk::SubpassDependency final {
            .srcSubpass = 0,
            .dstSubpass = VK_SUBPASS_EXTERNAL,
            .srcStageMask = vk::PipelineStageFlagBits::eLateFragmentTests,
            .dstStageMask = vk::PipelineStageFlagBits::eFragmentShader,
            .srcAccessMask = vk::AccessFlagBits::eDepthStencilAttachmentWrite,
            .dstAccessMask = vk::AccessFlagBits::eShaderRead,
            .dependencyFlags = vk::DependencyFlagBits::eByRegion };

        std::array<vk::SubpassDependency, 2> dependencies { initial, final };

        // TODO: Swap this out for dynamic rendering. Deprecated as of Vulkan 1.4
        vk::RenderPassCreateInfo renderPassCreateInfo {
            .sType = vk::StructureType::eRenderPassCreateInfo,
            .attachmentCount = 1,
            .pAttachments = &attachmentDescription,
            .subpassCount = 1,
            .pSubpasses = &subpass,
            .dependencyCount = static_cast<uint32_t>(dependencies.size()),
            .pDependencies = dependencies.data() };

        depthRenderPass.renderPass = vk::raii::RenderPass(device->logicalDevice, renderPassCreateInfo );
    }
    // Anyways, even though it's gonna get axe'd whenever we're switching to dynamic rendering, a framebuffer is just the underlying image where we draw the image to.
    // In this case, it's just a vkImage that houses depth values per pixel.

        // We can just use Image::createImage, but whatever, just being explicit -- this is getting removed anyway.
    void createOffscreenFrameBuffer()
    {
        depthRenderPass.width = 2048;
        depthRenderPass.height = 2048;

        // Image
        vk::ImageCreateInfo imageCreateInfo {
            .imageType = vk::ImageType::e2D,
            .format = vk::Format::eD16Unorm,
            .extent = { depthRenderPass.width, depthRenderPass.height, 1 },
            .mipLevels = 1,
            .arrayLayers = 1,
            .samples = vk::SampleCountFlagBits::e1,
            .tiling = vk::ImageTiling::eOptimal,
            .usage = vk::ImageUsageFlagBits::eDepthStencilAttachment | vk::ImageUsageFlagBits::eSampled };
        vkCreateImage( *device->logicalDevice, imageCreateInfo, nullptr, &depthRenderPass.depthShadowMapAttachment.image );

        // Underlying Memory of Image
        vk::MemoryRequirements memRequirements {};
        vkGetImageMemoryRequirements( *device->logicalDevice, depthRenderPass.depthShadowMapAttachment.image, memRequirements );
        vk::MemoryAllocateInfo allocInfo {
            .allocationSize = memRequirements.size,
            .memoryTypeIndex = GPUMemoryObject::findGPUBufferMemoryType( device->physicalDevice, memRequirements.memoryTypeBits, vk::MemoryPropertyFlagBits::eDeviceLocal ) };
        depthRenderPass.depthShadowMapAttachment.gpuMemory = vk::raii::DeviceMemory( device->logicalDevice, allocInfo );
        vkBindImageMemory( *device->logicalDevice, depthRenderPass.depthShadowMapAttachment.image, *depthRenderPass.depthShadowMapAttachment.gpuMemory, 0 );

        // Image View to Access it
        vk::ImageViewCreateInfo depthStencilImageViewCreateInfo {
            .image = depthRenderPass.depthShadowMapAttachment.image,
            .viewType = vk::ImageViewType::e2D,
            .format = vk::Format::eD16Unorm,
            .subresourceRange = {
                .aspectMask = vk::ImageAspectFlagBits::eDepth,
                .baseMipLevel = 0,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = 1 }
        };
        depthRenderPass.depthShadowMapAttachment.imageView = vk::raii::ImageView( device->logicalDevice, depthStencilImageViewCreateInfo );

        // RenderApplication::createTextureSampler() -- Creates the sampler of the depth image so we can actually do something with it -- tells the GPU how to "SAMPLE" the depth image's depth values.
        vk::SamplerCreateInfo samplerInfo {
            .magFilter = vk::Filter::eLinear,
            .minFilter = vk::Filter::eLinear,
            .addressModeU = vk::SamplerAddressMode::eClampToEdge,
            .addressModeV = vk::SamplerAddressMode::eClampToEdge,
            .addressModeW = vk::SamplerAddressMode::eClampToEdge,
            .maxAnisotropy = 1.0f,
            .borderColor = vk::BorderColor::eFloatOpaqueWhite };
        samplerInfo.mipmapMode = vk::SamplerMipmapMode::eLinear;
        samplerInfo.mipLodBias = 0.0f;
        samplerInfo.minLod = 0.0f;
        samplerInfo.maxLod = 1.0f;
        depthRenderPass.depthSampler = vk::raii::Sampler( device->logicalDevice, samplerInfo );

        vk::ImageView imageView = *depthRenderPass.depthShadowMapAttachment.imageView;
        // Creates the framebuffer for where our depth image is actually stored
        vk::FramebufferCreateInfo framebufferCreateInfo {
            // It's a bit weird, but for a framebuffer, we have to specify which render pass
            // This framebuffer is thus ONLY compatible with this specific render pass (technically, the render passes' creation info) -- this does NOT mean you can only use it in a single render pass
            // If a render pass utilizes the exact same render pass creation details for its beginInfo, this framebuffer can be written/read to.
                // BeginInfo is how you actually begin a render pass, it's like dynamic rendering's .BeginRendering() function call.
            .renderPass = depthRenderPass.renderPass,
            .attachmentCount = 1,
            .pAttachments = &imageView,
            .width = depthRenderPass.width,
            .height = depthRenderPass.height,
            .layers = 1 };
        depthRenderPass.frameBuffer = vk::raii::Framebuffer( device->logicalDevice, framebufferCreateInfo );
    }
    // Dynamic Rendering ELIMINATES having to create framebuffers and render passes manually -- WHENEVER WE SWITCH TO DYNAMIC, AXE EM!

    void createOnscreenFrameBufferImages( vk::Extent2D swapChainSize, const vk::Format& colorFormat, const vk::Format& depthFormat )
    {
        shadowRenderPass.colourAttachment.createImage(
             *device, swapChainSize.width, swapChainSize.height, 1, vk::SampleCountFlagBits::e8, colorFormat, vk::ImageTiling::eOptimal,
            vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eTransientAttachment,
        vk::MemoryPropertyFlagBits::eDeviceLocal, vk::ImageAspectFlagBits::eColor );

        shadowRenderPass.depthAttachment.createImage(
             *device, swapChainSize.width, swapChainSize.height, 1, vk::SampleCountFlagBits::e8, depthFormat, vk::ImageTiling::eOptimal,
            vk::ImageUsageFlagBits::eDepthStencilAttachment | vk::ImageUsageFlagBits::eTransientAttachment,
        vk::MemoryPropertyFlagBits::eDeviceLocal, vk::ImageAspectFlagBits::eDepth );
    }

        // https://github.com/SaschaWillems/Vulkan/blob/master/base/vulkanexamplebase.cpp -- Onscreen Renderpass: setupRenderPass()

    // A renderpass is basically an introduction to what the GPU can expect when some resource runs thru the shader/pipeline
    // It specifies the kind of images itll write to, what those image transitions may be, and dependencies it needs
    // It's essentially saying "Hey, this job will do this"
    void createOnscreenRenderPass( const vk::Format& colorFormat, const vk::Format& depthFormat )
    {
        // Sample count is hardcoded, but it should be from device.msaaSamples, so...
        vk::AttachmentDescription colourAttachment {
            .format = colorFormat,
            .samples = vk::SampleCountFlagBits::e8,
            .loadOp = vk::AttachmentLoadOp::eClear,
            .storeOp = vk::AttachmentStoreOp::eStore,
            .stencilLoadOp = vk::AttachmentLoadOp::eDontCare,
			.stencilStoreOp = vk::AttachmentStoreOp::eDontCare,
			.initialLayout = vk::ImageLayout::eUndefined,
			.finalLayout = vk::ImageLayout::eColorAttachmentOptimal // We need to resolve this, so we're not presenting it yet.
        };

        vk::AttachmentDescription colourResolveAttachment {
            .format = colorFormat,
            .samples = vk::SampleCountFlagBits::e1,
            .loadOp = vk::AttachmentLoadOp::eDontCare, // We don't do eLoad because we don't want the previous contents -- we're effectively copying the data manually from the previous pass, which I suppose also ties into initialLayout
            .storeOp = vk::AttachmentStoreOp::eStore,
            .initialLayout = vk::ImageLayout::eUndefined, // It is undefined, which is weird, but its some performance trick? It WAS in colorOptimal, but I suppose we don't care.
            .finalLayout = vk::ImageLayout::ePresentSrcKHR };


        vk::AttachmentDescription depthAttachment {
            .format = depthFormat,
            .samples = vk::SampleCountFlagBits::e8,
            .loadOp = vk::AttachmentLoadOp::eClear,
            .storeOp = vk::AttachmentStoreOp::eStore, // MAYBE discard it instead of storing it: eDontCare
            .stencilLoadOp = vk::AttachmentLoadOp::eClear,
			.stencilStoreOp = vk::AttachmentStoreOp::eDontCare,
			.initialLayout = vk::ImageLayout::eUndefined,
			.finalLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal
        }; // We don't resolve the depth to e1 because it gets discarded anyway. No need.

        // So instead of using .resolveImage/.resolveMode/.resolveImageView members within dynamic rendering's attachment stuff, you need to create a separate attachment whom's sole purpose is to resolve it to e1.

        // What this render pass' attachments will be
        std::array<vk::AttachmentDescription, 3> attachments { colourAttachment, colourResolveAttachment, depthAttachment };

        vk::AttachmentReference colourReference { .attachment = 0, .layout = vk::ImageLayout::eColorAttachmentOptimal };
        vk::AttachmentReference colourResolveReference { .attachment = 1, .layout = vk::ImageLayout::eColorAttachmentOptimal }; // .layout isnt the final layout, its what the layout needs to be while its executing.
        vk::AttachmentReference depthReference { .attachment = 2, .layout = vk::ImageLayout::eDepthStencilAttachmentOptimal};

        vk::SubpassDescription description {
            .pipelineBindPoint = vk::PipelineBindPoint::eGraphics,
            .colorAttachmentCount = 1,
            .pColorAttachments = &colourReference,
            .pResolveAttachments = &colourResolveReference,
            .pDepthStencilAttachment = &depthReference };


        vk::SubpassDependency depthDependency {
            .srcSubpass = vk::SubpassExternal,
            .dstSubpass = 0,
            .srcStageMask = vk::PipelineStageFlagBits::eEarlyFragmentTests | vk::PipelineStageFlagBits::eLateFragmentTests,
            .dstStageMask = vk::PipelineStageFlagBits::eEarlyFragmentTests | vk::PipelineStageFlagBits::eLateFragmentTests,
            .srcAccessMask = vk::AccessFlagBits::eDepthStencilAttachmentWrite,
            .dstAccessMask = vk::AccessFlagBits::eDepthStencilAttachmentWrite | vk::AccessFlagBits::eDepthStencilAttachmentRead
        };

        vk::SubpassDependency colourDependency {
            .srcSubpass = vk::SubpassExternal,
            .dstSubpass = 0,
            .srcStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput,
            .dstStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput,
            .srcAccessMask = vk::AccessFlagBits::eNone,
            .dstAccessMask = vk::AccessFlagBits::eColorAttachmentWrite }; // We don't care for eColorAttachmentRead, we just want to finish writing to it -- the resolve does NOT require reading.

        vk::SubpassDependency colourResolveDependency {
            .srcSubpass = 0,
            .dstSubpass = vk::SubpassExternal,
            .srcStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput,
            .dstStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput | vk::PipelineStageFlagBits::eFragmentShader,
            .srcAccessMask = vk::AccessFlagBits::eColorAttachmentWrite, // Previously, it was  | vk::AccessFlagBits::eColorAttachmentRead, but we eDontCare'd the loadOp, so we're not reading from the previous.
            .dstAccessMask = vk::AccessFlagBits::eColorAttachmentWrite | vk::AccessFlagBits::eColorAttachmentRead };

        std::array<vk::SubpassDependency, 3> layoutTransitions_dependencies { depthDependency, colourDependency, colourResolveDependency };

        vk::RenderPassCreateInfo renderPassCreateInfo {
            .sType = vk::StructureType::eRenderPassCreateInfo, // What type we're creating (always gonna be this for a vk::RenderPassCreateInfo object)

            // The attachments specify the format of the images we intend on working on (but does NOT say the literal exact framebuffer we'll be executing upon -- just the details/description of it)
            .attachmentCount = static_cast<uint32_t>(attachments.size()),
            .pAttachments = attachments.data(),

            // Specifies the attachments and where they are located -- attachment 0 is colour, attachment 1 is depth.
            // DURING FRAME BUFFER CREATION, YOU HAVE TO MATCH THESE EXACT INDICES: FIRST SLOT IS COLOUR, SECOND IS DEPTH.
            .subpassCount = 1,
            .pSubpasses = &description,

            // The image transitions we'll be doing
            .dependencyCount = static_cast<uint32_t>(layoutTransitions_dependencies.size()),
            .pDependencies = layoutTransitions_dependencies.data()
        };

        shadowRenderPass.renderPass = vk::raii::RenderPass( device->logicalDevice, renderPassCreateInfo );
    }

    void createOnscreenFrameBuffer( const SwapChain& swapChain )
    {
        shadowRenderPass.frameBuffer.clear();

        for ( uint32_t i = 0; i < swapChain.swapChainImages.size(); i++ )
        {
            // TODO: THIS. FIX THIS GARBAGE.
            // ATTACHMENT 0 SHOULD BE SHADOWRENDERPASS' COLOURATTACHMENT, ATTACH1 IS THE SWAP CHAIN IMAGE (WHERE WE RESOLVE IT ONTO)
            // AND ATTACH2 IS THE SHADOWRENDERPASS' DEPTHATTACHMENT
            const std::array<vk::ImageView, 3> attachments {
                shadowRenderPass.colourAttachment.imageView, // attach0
                swapChain.swapChainImages[i].imageView, // attach1
                shadowRenderPass.depthAttachment.imageView }; // attach2 -- separate from offscreen's depth map.

            vk::FramebufferCreateInfo framebufferCreateInfo {
                .sType = vk::StructureType::eFramebufferCreateInfo,
                .renderPass = shadowRenderPass.renderPass,
                .attachmentCount = 3,
                .pAttachments = attachments.data(),
                .width = swapChain.imageResolution.width,
                .height = swapChain.imageResolution.height,
                .layers = 1 };

            shadowRenderPass.frameBuffer.emplace_back( vk::raii::Framebuffer( device->logicalDevice, framebufferCreateInfo ) );
        }
    }



    // Creating custom descriptors and pipelines for creating depth images.
    void createBuffers( const int& buffersToCreate )
    {
        depthRenderPass.offscreen_Buffers.clear();
        shadowRenderPass.onscreen_Buffers.clear();

        for ( size_t i = 0; i < buffersToCreate; i++ )
        {
            // Offscreen
            GPUBuffer off_individualBuffer;
            off_individualBuffer.createGPUBuffer(
                *device,
                sizeof(mvpBuffer),
                vk::BufferUsageFlagBits::eUniformBuffer,
                vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
                true );
            depthRenderPass.offscreen_Buffers.emplace_back( std::move(off_individualBuffer) );

            // Onscreen
            GPUBuffer on_individualBuffer;
            on_individualBuffer.createGPUBuffer(
                *device,
                sizeof(OnscreenBuffer),
                vk::BufferUsageFlagBits::eUniformBuffer,
                vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
                true );
            shadowRenderPass.onscreen_Buffers.emplace_back( std::move(on_individualBuffer) );
        }
    }

    // I seriously have to abstract this to not have to retype the same garbage after the 100th time -- rendergraphs are the solution?
    void createDescriptors( vk::raii::DescriptorPool& pool, const int& sets ) // Don't forget to assign more space to the pool in renderer.cpp
    {
        // Reusing the descriptor pool because it's more efficient: Hence, passing it.
        depthRenderPass.descriptor.setDescriptorsPool( pool );
        shadowRenderPass.descriptor.setDescriptorsPool( pool );

        // Shared by both offscreen and onscreen.
        vk::DescriptorSetLayoutBinding uboLayoutBinding(
            0,
            vk::DescriptorType::eUniformBuffer,
            1,
            vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment,
            nullptr );

        // Used solely by onscreen.
        vk::DescriptorSetLayoutBinding samplerLayoutBinding(
            1,
            vk::DescriptorType::eCombinedImageSampler,
            1,
            vk::ShaderStageFlagBits::eFragment,
            nullptr );
        vk::DescriptorSetLayoutBinding mapLayoutBinding(
            2,
            vk::DescriptorType::eSampledImage,
            1,
            vk::ShaderStageFlagBits::eFragment,
            nullptr );

        std::vector<vk::DescriptorSetLayoutBinding> layoutBindings { uboLayoutBinding, samplerLayoutBinding, mapLayoutBinding };
        depthRenderPass.descriptor.createDescriptorSetLayout( device->logicalDevice, layoutBindings ); // Render technically has the sampler in its blueprint, but we are ignoring it.
        shadowRenderPass.descriptor.createDescriptorSetLayout( device->logicalDevice, layoutBindings );

        depthRenderPass.descriptor.createEmptyDescriptorSets( device->logicalDevice, sets );
        shadowRenderPass.descriptor.createEmptyDescriptorSets( device->logicalDevice, sets );

        for ( int i = 0; i < sets; i++ )
        {
            // Offscreen: this chooses to ignore binding 1 (even though we specified in the blueprint) -- it's perfectly okay like this.
            depthRenderPass.descriptor.setBufferResource( device->logicalDevice, depthRenderPass.offscreen_Buffers[i].gpuBuffer, vk::DescriptorType::eUniformBuffer, sizeof(mvpBuffer), i, 0 );

            // Onscreen
            shadowRenderPass.descriptor.setBufferResource( device->logicalDevice, shadowRenderPass.onscreen_Buffers[i].gpuBuffer, vk::DescriptorType::eUniformBuffer, sizeof(OnscreenBuffer), i, 0 );
            shadowRenderPass.descriptor.setSamplerResource( device->logicalDevice, depthRenderPass.depthSampler, depthRenderPass.depthShadowMapAttachment.imageView, i, 1 );
            shadowRenderPass.descriptor.setSampledImageResource( device->logicalDevice, depthRenderPass.depthShadowMapAttachment.imageView, i, 2 );
        }

        createPipelines( { depthRenderPass.descriptor.descriptorSetLayout } );
    }

    void createPipelines( const std::vector<vk::DescriptorSetLayout>& descriptorLayouts )
    {
        // Generic, shared.
        vk::PipelineInputAssemblyStateCreateInfo inputAssemblyCreateInfo { .topology = vk::PrimitiveTopology::eTriangleList };
        vk::PipelineRasterizationStateCreateInfo rasterizationCreateInfo = Pipeline::createRasterizer();

        std::array<vk::PipelineShaderStageCreateInfo, 2> programmableShaderStages{};

        vk::PipelineColorBlendAttachmentState colorBlendAttachment { // Need to pass this because otherwise createInfo'll house a dangling reference. Probably make this a static return.
            .blendEnable         = vk::True,
            .srcColorBlendFactor = vk::BlendFactor::eSrcAlpha,
            .dstColorBlendFactor = vk::BlendFactor::eOneMinusSrcAlpha,
            .colorBlendOp        = vk::BlendOp::eAdd,
            .srcAlphaBlendFactor = vk::BlendFactor::eOne,
            .dstAlphaBlendFactor = vk::BlendFactor::eZero,
            .alphaBlendOp        = vk::BlendOp::eAdd,
            .colorWriteMask      = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA };
        vk::PipelineColorBlendStateCreateInfo colorBlendCreateInfo = Pipeline::createColorBlending( colorBlendAttachment );

        vk::PipelineDepthStencilStateCreateInfo depthStencilCreateInfo = Pipeline::createDepthStencil();
        depthStencilCreateInfo.depthCompareOp = vk::CompareOp::eLessOrEqual;

        std::vector<vk::DynamicState> dynamicStates { vk::DynamicState::eViewport, vk::DynamicState::eScissor };
        vk::PipelineDynamicStateCreateInfo dynamicStatesCreateInfo {
            .sType = vk::StructureType::ePipelineDynamicStateCreateInfo,
            .dynamicStateCount = static_cast<uint32_t>(dynamicStates.size()),
            .pDynamicStates = dynamicStates.data() };
        vk::PipelineViewportStateCreateInfo viewportCreateInfo {
            .viewportCount = 1,
            .pViewports = nullptr,
            .scissorCount = 1,
            .pScissors = nullptr };

        vk::PipelineMultisampleStateCreateInfo multisamplingCreateInfo = Pipeline::createMultisampling( vk::SampleCountFlagBits::e8 );
        multisamplingCreateInfo.minSampleShading = 1.0f;
        multisamplingCreateInfo.sampleShadingEnable = vk::False;

        vk::PipelineLayoutCreateInfo pipelineLayoutCreateInfo {
            .setLayoutCount = static_cast<uint32_t>(descriptorLayouts.size()),
            .pSetLayouts = descriptorLayouts.data() };

        pipelineLayout = vk::raii::PipelineLayout( device->logicalDevice, pipelineLayoutCreateInfo );

        // We modify this later on a per pipeline basis: they're pointers, so it works.
        vk::GraphicsPipelineCreateInfo graphicsCreateInfo {
            .stageCount          = static_cast<uint32_t>(programmableShaderStages.size()),
            .pStages             = programmableShaderStages.data(),
            .pInputAssemblyState = &inputAssemblyCreateInfo,
            .pViewportState      = &viewportCreateInfo,
            .pRasterizationState = &rasterizationCreateInfo,
            .pMultisampleState   = &multisamplingCreateInfo,
            .pDepthStencilState  = &depthStencilCreateInfo,
            .pColorBlendState    = &colorBlendCreateInfo,
            .pDynamicState       = &dynamicStatesCreateInfo,
            .layout              = pipelineLayout,
            .renderPass          = shadowRenderPass.renderPass };


        // On screen:
        std::cout << "beginning onscreen creation!\n";
        auto bindingDescription    = ShadowVertex::getBindingDescription();
        auto attributeDescriptions = ShadowVertex::getAttributeDescriptions();
        vk::PipelineVertexInputStateCreateInfo vertexInputInfo {
            .vertexBindingDescriptionCount   = 1, .pVertexBindingDescriptions = &bindingDescription,
            .vertexAttributeDescriptionCount = static_cast<uint32_t>( attributeDescriptions.size() ), .pVertexAttributeDescriptions = attributeDescriptions.data() };
        graphicsCreateInfo.pVertexInputState = &vertexInputInfo;

        vk::raii::ShaderModule vertShaderModule = PipelineUtils::createShaderModule( device->logicalDevice, "../shaders/shadow_screen_vertex.spv" );
        vk::PipelineShaderStageCreateInfo vertexShader_StageInfo_onscreen {
            .stage = vk::ShaderStageFlagBits::eVertex,
            .module = vertShaderModule,
            .pName = "vertMain" };
        vk::raii::ShaderModule fragShaderModule = PipelineUtils::createShaderModule( device->logicalDevice, "../shaders/shadow_screen_fragment.spv" );
        vk::PipelineShaderStageCreateInfo fragShader_StageInfo_onscreen {
            .stage = vk::ShaderStageFlagBits::eFragment,
            .module = fragShaderModule,
            .pName = "fragMain" };

        programmableShaderStages[0] = vertexShader_StageInfo_onscreen;
        programmableShaderStages[1] = fragShader_StageInfo_onscreen;

        // To have a option to enable PCF in the CPU instead of hard coding it in the GPU -- it's like a push constant, but you set it at pipeline creation.
            // you cannot modify it after pipeline creation -- it's const as const can be. set it at pipeline creation and thats it.
        vk::Bool32 enablePCF = vk::False;
        vk::SpecializationMapEntry constantIDEntry {
            .constantID = 0,
            .offset = 0,
            .size = sizeof(vk::Bool32) };
        vk::SpecializationInfo constantIDInfo {
            .mapEntryCount = 1,
            .pMapEntries = &constantIDEntry,
            .dataSize = sizeof(vk::Bool32),
            .pData = &enablePCF };
        programmableShaderStages[1].pSpecializationInfo = &constantIDInfo;

        // IF WE WANT TO CREATE OPTIONS, WE NEED TO CREATE 2 PIPELINES, HENCE WHY SASCHA CREATES 2 OF THE SAME PIPELINES, BUT ONE OF THEM HAS IT ENABLED
        // SINCE WE CANT CHANGE IT ON THE FLY (ITS IMMUTABLE), WE CREATE 2
        // PipelineCache (second param) is already compiled .spv pipeline/shader files -- it reuses the old, already existing pipelines so we don't have to recreate it whenever we are switching between pipelines
        // We can just use the already existing one.
        shadowRenderPass.shadowPassPipeline.pipeline = vk::raii::Pipeline( device->logicalDevice, nullptr, graphicsCreateInfo );



        // Offscreen:
        std::cout << "beginning offscreen creation!\n";
        vertexInputInfo.vertexAttributeDescriptionCount = 1;

        vk::raii::ShaderModule vertShaderModule_offscreen = PipelineUtils::createShaderModule( device->logicalDevice, "../shaders/shadow_offscreen_vertex.spv" );
        vk::PipelineShaderStageCreateInfo vertexShader_StageInfo_offscreen {
            .stage = vk::ShaderStageFlagBits::eVertex,
            .module = vertShaderModule_offscreen,
            .pName = "vertMain" };
        programmableShaderStages[0] = vertexShader_StageInfo_offscreen;
        graphicsCreateInfo.stageCount = 1;

        colorBlendCreateInfo.attachmentCount = 0; // No blend attachment states (no color attachments used)

        rasterizationCreateInfo.cullMode = vk::CullModeFlagBits::eNone;
        rasterizationCreateInfo.depthBiasEnable = vk::True; // Enable depth bias
        depthStencilCreateInfo.depthCompareOp = vk::CompareOp::eLessOrEqual; // This line is useless because it's already less or equal.

        multisamplingCreateInfo.rasterizationSamples = vk::SampleCountFlagBits::e1;

        // Add depth bias to dynamic state, so we can change it at runtime
        dynamicStates.push_back( vk::DynamicState::eDepthBias );
        dynamicStatesCreateInfo = vk::PipelineDynamicStateCreateInfo {
            .sType = vk::StructureType::ePipelineDynamicStateCreateInfo,
            .dynamicStateCount = static_cast<uint32_t>(dynamicStates.size()),
            .pDynamicStates = dynamicStates.data() };

        graphicsCreateInfo.renderPass = depthRenderPass.renderPass;
        depthRenderPass.depthMapPipeline.pipeline = vk::raii::Pipeline( device->logicalDevice, nullptr, graphicsCreateInfo );
    }

    // fuck this. probs a better way.
    void createLightCamera()
    {
        // glm::vec3(-35.0f, 30.0f, 10.0f), glm::quat(1.0f, 0.0f, 0.0f, 0.0f), glm::vec3(1.0f);
        lightCamera.addComponent<TransformComponent>();
        lightCamera.addComponent<CameraComponent>();
    }

    void createCommandBuffer( const vk::raii::Device& device, uint32_t cmdBufferCount )
    {
        cmdBuffer.createCommandBuffers( device, cmdBufferCount );
    }

    // An annoying thing about traditional render passes is that you need to pre-record the command buffer commands instead of recording them on the fly, so it's kinda immutable (kinda... you can always modify it before literally submitting here)
    void prerecordCommandBuffer( uint32_t executingCmdBufferIndex, std::vector<Entity*>& drawableEntities ) // we ARE recording the command buffer here as well because it is a traditional render pass, so we do not reuse the old
    {
        vk::raii::CommandBuffer& executingBuffer = cmdBuffer.commandBuffers[executingCmdBufferIndex];

        vk::ClearValue depthClear;
        vk::ClearValue colourClear;

        vk::CommandBufferBeginInfo beginInfo { .sType = vk::StructureType::eCommandBufferBeginInfo };
        executingBuffer.begin( beginInfo );

            // First Render Pass: creates the shadow map by rendering from the light's point of view (offscreen)
        depthClear.depthStencil = { 1.0f, 0 };

        vk::RenderPassBeginInfo renderBeginInfo {
            .sType = vk::StructureType::eRenderPassBeginInfo,
            .renderPass = depthRenderPass.renderPass,
            .framebuffer = depthRenderPass.frameBuffer,
            .renderArea = { .extent = { depthRenderPass.width, depthRenderPass.height } },
            .clearValueCount = 1,
            .pClearValues = &depthClear };

        executingBuffer.beginRenderPass( renderBeginInfo, vk::SubpassContents::eInline ); // Subpass Contents means itll only be recorded onto this command buffer (doesnt use secondary cmd buffers)

        // Dynamic States
        vk::Viewport viewport {
            .width = depthRenderPass.width,
            .height = depthRenderPass.height,
            .minDepth = 0.0,
            .maxDepth = 1.0f };
        vk::Rect2D scissor {
            .offset = { 0,0 },
            .extent = { depthRenderPass.width, depthRenderPass.height } };
        executingBuffer.setViewport(0, viewport);
        executingBuffer.setScissor(0, scissor);

        executingBuffer.setDepthBias( depthBiasConstant, 0.0f, depthBiasSlope );

        executingBuffer.bindPipeline( vk::PipelineBindPoint::eGraphics, depthRenderPass.depthMapPipeline.pipeline );
        executingBuffer.bindDescriptorSets(
            vk::PipelineBindPoint::eGraphics, depthRenderPass.depthMapPipeline.pipelineLayout, 0, *depthRenderPass.descriptor.descriptorSets[executingCmdBufferIndex], nullptr );

        for ( Entity* entity : drawableEntities )
        {
            ModelData* model = entity->GetComponent<ModelComponent>()->getModel();

            executingBuffer.bindVertexBuffers(0, *model->vertexBuffer.gpuBuffer, {0} );
            executingBuffer.bindIndexBuffer( *model->indexBuffer.gpuBuffer, 0, vk::IndexType::eUint32 );
            executingBuffer.drawIndexed( model->indices_count, 1, 0, 0, 0 );
        }

		//Note: Explicit synchronization is not required between the render pass, as this is done implicitly via sub pass dependencies

            // Second Render Pass: actually draw the scene
    }


};