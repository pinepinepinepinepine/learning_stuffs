#include "includes.hpp"
#include "entity.hpp"
#include "../../headers/image.hpp"
#include "../../headers/descriptors.hpp"


    // https://github.com/SaschaWillems/Vulkan/blob/master/examples/shadowmapping/shadowmapping.cpp
// We're using traditional render passes for now because I'm not sure how to dynamically do this. Convert after.
// Idea is we FIRST render the scene onto this image, which is entirely off screen (offscreen rendering) FROM the light's point of view.
// We see how close in the pixel is to the camera in world space and the closer it is, the more illuminated (lower depth value) is given to that pixel.
// Afterwards, on our second render pass, we pass this attachment, and the fragment shader will compare the positions of the pixels coordinates:
// First, when the fragment shader sees a pixel, it compares it to the depthShadowMapAttachment image as to see what the light originally recorded as the closest distance at that angle.
// Then, we take that pixel and see how far that pixel is from the light.

struct OffscreenPass
{
    int32_t width, height;
    VkFramebuffer frameBuffer;
    Image depthShadowMapAttachment;
    VkRenderPass renderPass;
    VkSampler depthSampler;
    VkDescriptorImageInfo descriptor;
};


class LightingSystem
{
    Entity lightCamera {"lightCamera"};

    OffscreenPass depthRenderPass;

    void createOffscreenRenderPass( const vk::raii::Device& device )
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

        // Subpasses are deprecated due to dynamic rendering being favoured, and so subpasses are also... GONE!
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

        depthRenderPass.renderPass = *vk::raii::RenderPass(device, renderPassCreateInfo );
    }

    // Anyways, even though it's gonna get axe'd whenever we're switching to dynamic rendering, a framebuffer is just the underlying image where we draw the image to.
    // In this case, it's just a vkImage that houses depth values per pixel.

        // We can just use Image::createImage, but whatever, just being explicit -- this is getting removed anyway.
    void createOffscreenFrameBuffer( const LogicalDevice& device )
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
        vkCreateImage( *device.logicalDevice, imageCreateInfo, nullptr, &depthRenderPass.depthShadowMapAttachment.image );

        // Underlying Memory of Image
        vk::MemoryRequirements memRequirements {};
        vkGetImageMemoryRequirements( *device.logicalDevice, depthRenderPass.depthShadowMapAttachment.image, memRequirements );
        vk::MemoryAllocateInfo allocInfo {
            .allocationSize = memRequirements.size,
            .memoryTypeIndex = GPUMemoryObject::findGPUBufferMemoryType( device.physicalDevice, memRequirements.memoryTypeBits, vk::MemoryPropertyFlagBits::eDeviceLocal ) };
        depthRenderPass.depthShadowMapAttachment.gpuMemory = vk::raii::DeviceMemory( device.logicalDevice, allocInfo );
        vkBindImageMemory( *device.logicalDevice, depthRenderPass.depthShadowMapAttachment.image, *depthRenderPass.depthShadowMapAttachment.gpuMemory, 0 );

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
        depthRenderPass.depthShadowMapAttachment.imageView = vk::raii::ImageView( device.logicalDevice, depthStencilImageViewCreateInfo );

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
        depthRenderPass.depthSampler = *vk::raii::Sampler( device.logicalDevice, samplerInfo );

        createOffscreenRenderPass( device.logicalDevice ); // Probs separate the function call elsewhere; in renderer.cpp (I suppose), call createOffscreenRenderPass() -> createOffscreenFrameBuffer() manually -- neater

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
        depthRenderPass.frameBuffer = *vk::raii::Framebuffer( device.logicalDevice, framebufferCreateInfo );
    }
    // Dynamic Rendering ELIMINATES having to create framebuffers and render passes manually -- WHENEVER WE SWITCH TO DYNAMIC, AXE EM!

};