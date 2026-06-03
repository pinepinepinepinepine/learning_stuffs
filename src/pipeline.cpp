#include "../headers/pipeline.hpp"

namespace PipelineUtils
{
    vk::raii::ShaderModule createShaderModule( vk::raii::Device& device, const std::string& filename )
    {
        std::ifstream file( filename, std::ios::ate | std::ios::binary );

        if ( !file.is_open() )
            throw std::runtime_error("failed to open shader module file!");

        std::vector<char> codeSPV( file.tellg() );
        file.seekg( 0, std::ios::beg );
        file.read( codeSPV.data(), static_cast<std::streamsize>( codeSPV.size() ) );
        file.close();

        vk::ShaderModuleCreateInfo ShaderModuleCreateInfo {
            .codeSize = codeSPV.size() * sizeof(char),
            .pCode = reinterpret_cast<const uint32_t*>( codeSPV.data() ) };

        return vk::raii::ShaderModule{ device, ShaderModuleCreateInfo };
    }
}

std::vector<vk::PipelineShaderStageCreateInfo> Pipeline::createProgrammableModules( const vk::raii::ShaderModule& shaderModule )
{
    vk::PipelineShaderStageCreateInfo vertexShader_StageInfo {
        .stage = vk::ShaderStageFlagBits::eVertex,
        .module = shaderModule,
        .pName = "vertMain"
    };

    vk::PipelineShaderStageCreateInfo fragmentShader_StageInfo {
        .stage = vk::ShaderStageFlagBits::eFragment,
        .module = shaderModule,
        .pName = "fragMain"
    };

    return std::vector<vk::PipelineShaderStageCreateInfo>{ vertexShader_StageInfo, fragmentShader_StageInfo };
}

vk::PipelineViewportStateCreateInfo Pipeline::createViewport( vk::Extent2D dimensions )
{
    viewport = vk::Viewport{
        0.0f, 0.0f,
        static_cast<float>( dimensions.width ), static_cast<float>( dimensions.height ),
        0.0f, 1.0f };

    scissor = vk::Rect2D{ vk::Offset2D{ 0, 0 }, dimensions };

    // Baked into the pipeline instead of at drawing w/ cmdBuffer.setViewport/setScissor.
    return vk::PipelineViewportStateCreateInfo{ .viewportCount = 1, .pViewports = &viewport, .scissorCount = 1, .pScissors = &scissor };
}

vk::PipelineRasterizationStateCreateInfo Pipeline::createRasterizer()
{
    vk::PipelineRasterizationStateCreateInfo rasterizerCreateInfo {
        .depthClampEnable        = vk::False,
        .rasterizerDiscardEnable = vk::False,
        .polygonMode             = vk::PolygonMode::eFill,
        .cullMode                = vk::CullModeFlagBits::eBack, // fucking add back culling whenever we're finished. eNone for none, eBack for default.
        .frontFace               = vk::FrontFace::eCounterClockwise, // CLOCKWISE OR COUNTERCLOCKWISE?
        .depthBiasEnable         = vk::False,
        .lineWidth               = 1.0f };
    return rasterizerCreateInfo;
}

vk::PipelineMultisampleStateCreateInfo Pipeline::createMultisampling( vk::SampleCountFlagBits samplesPer )
{
    vk::PipelineMultisampleStateCreateInfo multisamplingCreateInfo {
        .rasterizationSamples   = samplesPer,
        .sampleShadingEnable    = vk::True,
        .minSampleShading       = 0.2f
    };
    return multisamplingCreateInfo;
}

// Make this customizable per pipeline instead of hardcoded like this.
vk::PipelineDepthStencilStateCreateInfo Pipeline::createDepthStencil()
{
    // ADD THIS GARBAGE BACK
    vk::PipelineDepthStencilStateCreateInfo depthStencilCreateInfo {
        .depthTestEnable       = vk::True,
        .depthWriteEnable      = vk::True,
        .depthCompareOp        = vk::CompareOp::eLess,
        .depthBoundsTestEnable = vk::False,
        .stencilTestEnable     = vk::False
    };


    // vk::PipelineDepthStencilStateCreateInfo depthStencilCreateInfo {
    //     .depthTestEnable       = vk::False,
    //     .depthWriteEnable      = vk::False,
    //     .depthCompareOp        = vk::CompareOp::eAlways,
    //     .depthBoundsTestEnable = vk::False,
    //     .stencilTestEnable     = vk::False
    // };
    return depthStencilCreateInfo;
}

vk::PipelineColorBlendStateCreateInfo Pipeline::createColorBlending( vk::PipelineColorBlendAttachmentState& colorAttachmentInfo )
{
    vk::PipelineColorBlendStateCreateInfo colorBlendingCreateInfo {
        .logicOpEnable = vk::False,
        .logicOp = vk::LogicOp::eCopy,
        .attachmentCount = 1,
        .pAttachments = &colorAttachmentInfo };
    return colorBlendingCreateInfo;
}

vk::PipelineRenderingCreateInfo Pipeline::createRendering( const vk::Format& colorFormat, const vk::Format& depthFormat )
{
    vk::PipelineRenderingCreateInfo renderingPipelineCreateInfo {
        .colorAttachmentCount = 1,
        .pColorAttachmentFormats = &colorFormat,
        .depthAttachmentFormat = depthFormat };
    return renderingPipelineCreateInfo;
}

void Pipeline::createPipelineDescriptorLayout( const vk::raii::Device& device, const std::vector<vk::DescriptorSetLayout>& descriptorSetLayouts, const std::vector<vk::PushConstantRange>& pushConstants )
{
    vk::PipelineLayoutCreateInfo pipelineLayoutInfo {
        .setLayoutCount = static_cast<uint32_t>(descriptorSetLayouts.size()),
        .pSetLayouts = descriptorSetLayouts.data(),
        .pushConstantRangeCount = static_cast<uint32_t>(pushConstants.size()),
        .pPushConstantRanges = pushConstants.data() };

    pipelineLayout = vk::raii::PipelineLayout( device, pipelineLayoutInfo );
}

void Pipeline::createGraphicsPipeline( const LogicalDevice& device, const vk::raii::ShaderModule& shaderModule, const SwapChain& framebufferProperties, vk::PrimitiveTopology topology, const vk::PipelineVertexInputStateCreateInfo& vertexInputInfo )
{
    std::vector<vk::PipelineShaderStageCreateInfo> programmableModulesCreateInfo = createProgrammableModules( shaderModule );

    vk::PipelineInputAssemblyStateCreateInfo inputAssemblyCreateInfo { .topology = topology };

    vk::PipelineViewportStateCreateInfo viewportCreateInfo = createViewport( framebufferProperties.imageResolution );
    vk::PipelineRasterizationStateCreateInfo rasterizerCreateInfo = createRasterizer();
    vk::PipelineMultisampleStateCreateInfo multisamplingCreateInfo = createMultisampling( device.msaaSamples );
    vk::PipelineDepthStencilStateCreateInfo depthStencilCreateInfo = createDepthStencil();

    vk::PipelineColorBlendAttachmentState colorBlendAttachment { // Need to pass this because otherwise createInfo'll house a dangling reference.
        .blendEnable         = vk::True,
        .srcColorBlendFactor = vk::BlendFactor::eSrcAlpha,
        .dstColorBlendFactor = vk::BlendFactor::eOneMinusSrcAlpha,
        .colorBlendOp        = vk::BlendOp::eAdd,
        .srcAlphaBlendFactor = vk::BlendFactor::eOne,
        .dstAlphaBlendFactor = vk::BlendFactor::eZero,
        .alphaBlendOp        = vk::BlendOp::eAdd,
        .colorWriteMask      = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA };
    vk::PipelineColorBlendStateCreateInfo colorBlendCreateInfo = createColorBlending( colorBlendAttachment );

    vk::GraphicsPipelineCreateInfo graphicsPipelineCreateInfo {
    .stageCount          = static_cast<uint32_t>(programmableModulesCreateInfo.size()),
    .pStages             = programmableModulesCreateInfo.data(),
    .pVertexInputState   = &vertexInputInfo, // Is there a better way or just leave it to the caller? Maybe a member or something? idk.
    .pInputAssemblyState = &inputAssemblyCreateInfo,
    .pViewportState      = &viewportCreateInfo,
    .pRasterizationState = &rasterizerCreateInfo,
    .pMultisampleState   = &multisamplingCreateInfo,
    .pDepthStencilState  = &depthStencilCreateInfo,
    .pColorBlendState    = &colorBlendCreateInfo,
    .pDynamicState       = nullptr,
    .layout              = pipelineLayout,
    .renderPass          = nullptr };

    vk::Format depthFormat = device.findSupportedFormat(
        { vk::Format::eD32Sfloat, vk::Format::eD32SfloatS8Uint, vk::Format::eD24UnormS8Uint },
        vk::ImageTiling::eOptimal, vk::FormatFeatureFlagBits::eDepthStencilAttachment );
    vk::PipelineRenderingCreateInfo renderingCreateInfo = createRendering( framebufferProperties.surfaceFormat.format, depthFormat );

    vk::StructureChain<vk::GraphicsPipelineCreateInfo, vk::PipelineRenderingCreateInfo> pipelineCreateInfo { graphicsPipelineCreateInfo, renderingCreateInfo };

    if ( !*pipelineLayout )
        std::cerr << "Error! The Pipeline's Descriptor Layout has NOT been defined!\n";

    pipeline = vk::raii::Pipeline( device.logicalDevice, nullptr, pipelineCreateInfo.get<vk::GraphicsPipelineCreateInfo>() );
}


void Pipeline::createComputePipeline( const vk::raii::Device& device, const vk::raii::ShaderModule& shaderModule )
{
    vk::PipelineShaderStageCreateInfo computeShaderStageInfo {
        .stage = vk::ShaderStageFlagBits::eCompute,
        .module = shaderModule,
        .pName = "compMain" };

    vk::ComputePipelineCreateInfo computePipelineCreateInfo {
        .stage = computeShaderStageInfo,
        .layout = pipelineLayout
    };

    pipeline = vk::raii::Pipeline( device, nullptr, computePipelineCreateInfo );
}