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
    glm::mat4 proj;
    glm::mat4 view;
    glm::mat4 model;
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
    Image depthShadowMapAttachment;
    vk::raii::Sampler depthSampler = nullptr;
    Descriptor descriptor;

    std::vector<GPUBuffer> offscreen_Buffers;
    // Make a separate pipeline. We're not re-using the same one because a depth image can skip out a bunch of stages, so it's WAY faster to just use a custom made one.
    Pipeline depthMapPipeline; // used to create the depth image

    DedicatedCommandBuffers cmdBuffer;
};

struct OnscreenPass
{
    Descriptor descriptor;
    std::vector<GPUBuffer> onscreen_Buffers;
    Pipeline shadowPassPipeline;

    uint32_t width, height;

    DedicatedCommandBuffers cmdBuffer;
};

// a framebuffer is just the underlying image where we draw the image to
class LightingSystem
{
    Entity lightCamera {"lightCamera"};

    vk::raii::PipelineLayout pipelineLayout { VK_NULL_HANDLE };

    LogicalDevice* device;

    // Depth bias (and slope) are used to avoid shadowing artifacts
    static constexpr float depthBiasConstant = 1.25f; // Constant depth bias factor (always applied)
    static constexpr float depthBiasSlope = 1.75f; // Slope depth bias factor, applied depending on polygon's slope


    vk::Format colourFormat;
    vk::Format depthFormat;

    public:
    OnscreenPass shadowRenderPass;
    OffscreenPass depthRenderPass;

    void setDevice( LogicalDevice* d ) { device = d; }
    void setFormats( const vk::Format& colour, const vk::Format& depth )
    {
        colourFormat = colour;
        depthFormat = depth;
    }

    ~LightingSystem()
    {
        // Tutorial uses a cool destructor instead of a cleanup function -- might be handy to remove the cleanup function within runtime/renderer to a destructor!
        depthRenderPass.depthShadowMapAttachment.cleanupImage( device->logicalDevice );
    }

    // We can just use Image::createImage, but whatever, just being explicit -- this depthMapAttachment gets sampled by the shader to generated a final "darkened" fragment.
    void createDepthMapAttachment()
    {
        // Image
        vk::ImageCreateInfo imageCreateInfo {
            .imageType = vk::ImageType::e2D,
            .format = depthFormat,
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
            .format = depthFormat,
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
    }

    void setViewportExtent( const vk::Extent2D& toSwapChainResolution )
    {
        // Standard is to make the depth map attachment a square: 1024x1024, 2048x2048, 4096x4096
        depthRenderPass.width = 2048;
        depthRenderPass.height = 2048;

        shadowRenderPass.width = toSwapChainResolution.width;
        shadowRenderPass.height = toSwapChainResolution.height;
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
            shadowRenderPass.descriptor.setSamplerResource( device->logicalDevice, depthRenderPass.depthSampler, depthRenderPass.depthShadowMapAttachment.imageView, i, 1, vk::ImageLayout::eDepthStencilReadOnlyOptimal );
            shadowRenderPass.descriptor.setSampledImageResource( device->logicalDevice, depthRenderPass.depthShadowMapAttachment.imageView, i, 2, vk::ImageLayout::eDepthStencilReadOnlyOptimal );
        }
    }

    void createPipelines()
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
            .setLayoutCount = 1,
            .pSetLayouts = &*depthRenderPass.descriptor.descriptorSetLayout };

        pipelineLayout = vk::raii::PipelineLayout( device->logicalDevice, pipelineLayoutCreateInfo );
        // scuffed but whatever
        depthRenderPass.depthMapPipeline.pipelineLayout = vk::raii::PipelineLayout( device->logicalDevice, pipelineLayoutCreateInfo );
        shadowRenderPass.shadowPassPipeline.pipelineLayout = vk::raii::PipelineLayout( device->logicalDevice, pipelineLayoutCreateInfo );

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
            .renderPass          = nullptr }; // We're switching to dynamic rendering.




        // On screen:
        std::cout << "beginning onscreen creation!\n";
        auto bindingDescription_onPos           = Vertex::getBindingDescription();
        auto bindingDescription_onNormals       = ShadowVertex::getBindingDescription();
        bindingDescription_onNormals.binding    = 1;
        std::array<vk::VertexInputBindingDescription, 2> bindingDescriptions_on { bindingDescription_onPos, bindingDescription_onNormals };

        auto attributeDescriptions_onPos = Vertex::getAttributeDescriptions();
        auto attributeDescriptions_onNormals = ShadowVertex::getAttributeDescriptions();
        attributeDescriptions_onNormals[0].binding = 1;
        attributeDescriptions_onNormals[1].binding = 1;
        std::array<vk::VertexInputAttributeDescription, 3> attributeDescriptions_on
        {
            attributeDescriptions_onPos[0], attributeDescriptions_onNormals[0], attributeDescriptions_onNormals[1]
        };
        attributeDescriptions_on[1].location = 1;
        attributeDescriptions_on[2].location = 2;

        vk::PipelineVertexInputStateCreateInfo vertexInputInfo {
            .vertexBindingDescriptionCount   = static_cast<uint32_t>( bindingDescriptions_on.size() ), .pVertexBindingDescriptions = bindingDescriptions_on.data(),
            .vertexAttributeDescriptionCount = static_cast<uint32_t>( attributeDescriptions_on.size() ), .pVertexAttributeDescriptions = attributeDescriptions_on.data() };
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


        // FOR THE SPECIFIC IMAGE FORMATS WE'LL BE WORKING ON:
        vk::PipelineRenderingCreateInfo renderingCreateInfo = Pipeline::createRendering( colourFormat, depthFormat );


        // instead of doing .pNext inside graphicsCreateInfo, we can always just use struct chains, which are equivalent -- see the pipeline creation process in pipeline.cpp
        // however, it's just easier to use .pNext inside vk::GraphicsPipelineCreateInfo
        graphicsCreateInfo.pNext = &renderingCreateInfo;

        // IF WE WANT TO CREATE OPTIONS, WE NEED TO CREATE 2 PIPELINES, HENCE WHY SASCHA CREATES 2 OF THE SAME PIPELINES, BUT ONE OF THEM HAS IT ENABLED
        // SINCE WE CANT CHANGE IT ON THE FLY (ITS IMMUTABLE), WE CREATE 2
        // PipelineCache (second param) is already compiled .spv pipeline/shader files -- it reuses the old, already existing pipelines so we don't have to recreate it whenever we are switching between pipelines
        // We can just use the already existing one.
        shadowRenderPass.shadowPassPipeline.pipeline = vk::raii::Pipeline( device->logicalDevice, nullptr, graphicsCreateInfo );



        // Offscreen:
        std::cout << "beginning offscreen creation!\n";

        auto bindingDescription_off    = Vertex::getBindingDescription();
        auto attributeDescriptions_off = Vertex::getAttributeDescriptions();
        vk::PipelineVertexInputStateCreateInfo vertexInputInfo_off {
            .vertexBindingDescriptionCount   = 1, .pVertexBindingDescriptions = &bindingDescription_off,
            .vertexAttributeDescriptionCount = static_cast<uint32_t>( attributeDescriptions_off.size() ), .pVertexAttributeDescriptions = attributeDescriptions_off.data() };
        graphicsCreateInfo.pVertexInputState = &vertexInputInfo_off;

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

        // graphicsCreateInfo.renderPass = depthRenderPass.renderPass; // Old render pass, needed for traditional rendering, we're doing dynamic now.

        renderingCreateInfo.colorAttachmentCount = 0;
        renderingCreateInfo.pColorAttachmentFormats = nullptr;

        depthRenderPass.depthMapPipeline.pipeline = vk::raii::Pipeline( device->logicalDevice, nullptr, graphicsCreateInfo );
    }

    void createCommandBuffers( const vk::raii::Device& device, uint32_t cmdBufferCount )
    {
        shadowRenderPass.cmdBuffer.createCommandBuffers( device, cmdBufferCount, vk::CommandBufferLevel::eSecondary );
        depthRenderPass.cmdBuffer.createCommandBuffers( device, cmdBufferCount, vk::CommandBufferLevel::eSecondary );
    }


    void RecordOffScreenCommandBuffers( uint32_t executingCmdBufferIndex, const std::vector<Entity*>& drawableEntities )
    {
        vk::CommandBufferInheritanceRenderingInfo dynamicRendering_inheritanceInfo {
            .sType = vk::StructureType::eCommandBufferInheritanceRenderingInfo,
            .pNext = nullptr,
            .flags = {},
            .viewMask = 0,

            .colorAttachmentCount = 0,
            .pColorAttachmentFormats = nullptr,

            .depthAttachmentFormat = depthFormat,

            .stencilAttachmentFormat = vk::Format::eUndefined,

            .rasterizationSamples = vk::SampleCountFlagBits::e1 // TODO: Don't make this hardcoded, pass it from device creation.
        };

        vk::CommandBufferInheritanceInfo inheritanceInfo {
            .sType = vk::StructureType::eCommandBufferInheritanceInfo,
            .pNext = &dynamicRendering_inheritanceInfo, // next (link) the dynamic rendering info so vulkan has knowledge about the actual inheritance.

            .renderPass = nullptr, // set to nullptr because we are using dynamic rendering -- there isn't a render pass to point to.
            .subpass = 0, // An index: it's being ignored because .renderPass is nullptr
            .framebuffer = nullptr // Dynamic Rendering doesn't specify a framebuffer on creation (unlike with how traditional render passes have to specify what framebuffer will be worked upon) -- set nullptr for dynamic rendering.
        };


        vk::CommandBufferBeginInfo beginInfo {
            .sType = vk::StructureType::eCommandBufferBeginInfo,
            .pNext = nullptr,
            .flags = vk::CommandBufferUsageFlagBits::eRenderPassContinue, // MANDATORY for secondary command buffers -- specifies that this command buffer is to be executed inside another render pass (the dynamic render in our case)
            .pInheritanceInfo = &inheritanceInfo
        };

        vk::raii::CommandBuffer& executingBuffer = depthRenderPass.cmdBuffer.commandBuffers[executingCmdBufferIndex];

        executingBuffer.begin( beginInfo );

        // MANDATORY TODO: CLEAR THE DEPTH AND COLOUR BEFORE WE EXECUTE THE SECONDARY COMMAND BUFFER.

        // First "PASS": populates the depth map.
            // The Dynamic States specified in the depthRenderPass (and shadowRenderPass) pipeline
        vk::Viewport viewport {
            .width = static_cast<float>(depthRenderPass.width),
            .height = static_cast<float>(depthRenderPass.height),
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

            executingBuffer.bindVertexBuffers(0, *model->vertexBuffer_Positions.gpuBuffer, {0} );
            executingBuffer.bindIndexBuffer( *model->indexBuffer.gpuBuffer, 0, vk::IndexType::eUint32 );
            executingBuffer.drawIndexed( model->indices_count, 1, 0, 0, 0 );
        }

        executingBuffer.end();
    }

    void RecordOnScreenCommandBuffers( uint32_t executingCmdBufferIndex, const std::vector<Entity*>& drawableEntities )
    {
        vk::CommandBufferInheritanceRenderingInfo dynamicRendering_inheritanceInfo {
            .sType = vk::StructureType::eCommandBufferInheritanceRenderingInfo,
            .pNext = nullptr,
            .flags = {},
            .viewMask = 0,

            .colorAttachmentCount = 1,
            .pColorAttachmentFormats = &colourFormat,

            .depthAttachmentFormat = depthFormat,

            .stencilAttachmentFormat = vk::Format::eUndefined,

            .rasterizationSamples = vk::SampleCountFlagBits::e8 // TODO: Don't make this hardcoded, pass it from device creation.
        };

        vk::CommandBufferInheritanceInfo inheritanceInfo {
            .sType = vk::StructureType::eCommandBufferInheritanceInfo,
            .pNext = &dynamicRendering_inheritanceInfo, // next (link) the dynamic rendering info so vulkan has knowledge about the actual inheritance.

            .renderPass = nullptr, // set to nullptr because we are using dynamic rendering -- there isn't a render pass to point to.
            .subpass = 0, // An index: it's being ignored because .renderPass is nullptr
            .framebuffer = nullptr // Dynamic Rendering doesn't specify a framebuffer on creation (unlike with how traditional render passes have to specify what framebuffer will be worked upon) -- set nullptr for dynamic rendering.
        };


        vk::CommandBufferBeginInfo beginInfo {
            .sType = vk::StructureType::eCommandBufferBeginInfo,
            .pNext = nullptr,
            .flags = vk::CommandBufferUsageFlagBits::eRenderPassContinue, // MANDATORY for secondary command buffers -- specifies that this command buffer is to be executed inside another render pass (the dynamic render in our case)
            .pInheritanceInfo = &inheritanceInfo
        };

        vk::raii::CommandBuffer& executingBuffer = shadowRenderPass.cmdBuffer.commandBuffers[executingCmdBufferIndex];

        executingBuffer.begin( beginInfo );

        // The "SECOND PASS" (for presenting the images onto the screen)
        vk::Viewport on_viewport {
            .width = static_cast<float>(shadowRenderPass.width),
            .height = static_cast<float>(shadowRenderPass.height),
            .minDepth = 0.0,
            .maxDepth = 1.0f };
        vk::Rect2D on_scissor {
            .offset = { 0,0 },
            .extent = { shadowRenderPass.width, shadowRenderPass.height } };

        executingBuffer.setViewport(0, on_viewport);
        executingBuffer.setScissor(0, on_scissor);
        executingBuffer.bindPipeline( vk::PipelineBindPoint::eGraphics, shadowRenderPass.shadowPassPipeline.pipeline );
        executingBuffer.bindDescriptorSets(
            vk::PipelineBindPoint::eGraphics, shadowRenderPass.shadowPassPipeline.pipelineLayout, 0, *shadowRenderPass.descriptor.descriptorSets[executingCmdBufferIndex], nullptr );

        for ( Entity* entity : drawableEntities )
        {
            ModelData* model = entity->GetComponent<ModelComponent>()->getModel();

            executingBuffer.bindVertexBuffers(0, *model->vertexBuffer_Positions.gpuBuffer, {0} );
            executingBuffer.bindVertexBuffers(1, *model->vertexBuffer_Normals.gpuBuffer, {0} );
            executingBuffer.bindIndexBuffer( *model->indexBuffer.gpuBuffer, 0, vk::IndexType::eUint32 );
            executingBuffer.drawIndexed( model->indices_count, 1, 0, 0, 0 );
        }

        executingBuffer.end();
    }

    // fuck this. probs a better way.
    void createLightCamera()
    {
        lightCamera.addComponent<TransformComponent>( glm::vec3(-35.0f, 30.0f, 15.0f), glm::quat(1.0f, 0.0f, 0.0f, 0.0f), glm::vec3(1.0f) );
        lightCamera.addComponent<CameraComponent>();
    }

    void updateLightCamera( const float time )
    {
        glm::vec3 position;

        position.x = cos( glm::radians(time * 360.0f) ) * 40.0f;
        position.y = 50.0f + sin( glm::radians(time * 360.0f) ) * 20.0f;
        position.z = 25.0f + sin( glm::radians(time * 360.0f) ) * 5.0f;

        lightCamera.GetComponent<TransformComponent>()->SetPosition( position );
    }

    void updateUniformBuffers( const uint32_t bufferIndex, const CameraComponent* userCamera )
    {
        // Make sure to set these to actually hold value.
        TransformComponent* transform = lightCamera.GetComponent<TransformComponent>();
        CameraComponent* camera = lightCamera.GetComponent<CameraComponent>();

        mvpBuffer offscreen {
            .proj = glm::perspective(glm::radians(camera->fieldOfView), 1.0f, camera->nearPlane, camera->farPlane),
            .view = glm::lookAt(transform->GetPosition(), glm::vec3(0.0f), glm::vec3(0, 1, 0)),
            .model = glm::mat4(1.0f) };

        offscreen.proj[1][1] *= -1; // Flip Y for Vulkan
        memcpy( depthRenderPass.offscreen_Buffers[bufferIndex].gpuBufferMapped, &offscreen, sizeof(mvpBuffer) ); // Multiply it and then send the multiplied transformation matrix.

        OnscreenBuffer buffer;
        buffer.projection = userCamera->getProjectionMatrix();
        buffer.view = userCamera->getViewMatrix();
        buffer.model = glm::mat4(1.0f);
        buffer.depthBiasMVP = offscreen.proj * offscreen.view * offscreen.model;
        buffer.lightPos = glm::vec4( transform->GetPosition(), 1.0f );
        buffer.zNear = camera->nearPlane;
        buffer.zFar = camera->farPlane;
        buffer.projection[1][1] *= -1; // Flip Y for Vulkan
        memcpy( shadowRenderPass.onscreen_Buffers[bufferIndex].gpuBufferMapped, &buffer, sizeof(OnscreenBuffer) );
    }
};