#include "../headers/pipeline.hpp"

namespace PipelineUtils
{
    vk::raii::ShaderModule createShaderModule( vk::raii::Device& device, const std::string& filename )
    {
        std::ifstream file( filename, std::ios::ate | std::ios::binary );

        if ( !file.is_open() )
            throw std::runtime_error("failed to open file!");

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
        .cullMode                = vk::CullModeFlagBits::eBack,
        .frontFace               = vk::FrontFace::eCounterClockwise,
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

vk::PipelineDepthStencilStateCreateInfo Pipeline::createDepthStencil()
{
    vk::PipelineDepthStencilStateCreateInfo depthStencilCreateInfo {
        .depthTestEnable       = vk::True,
        .depthWriteEnable      = vk::True,
        .depthCompareOp        = vk::CompareOp::eLess,
        .depthBoundsTestEnable = vk::False,
        .stencilTestEnable     = vk::False
    };
    return depthStencilCreateInfo;
}

vk::PipelineColorBlendStateCreateInfo Pipeline::createColorBlending()
{
    vk::PipelineColorBlendAttachmentState colorBlendAttachment {
        .blendEnable         = vk::True,
        .srcColorBlendFactor = vk::BlendFactor::eSrcAlpha,
        .dstColorBlendFactor = vk::BlendFactor::eOneMinusSrcAlpha,
        .colorBlendOp        = vk::BlendOp::eAdd,
        .srcAlphaBlendFactor = vk::BlendFactor::eOne,
        .dstAlphaBlendFactor = vk::BlendFactor::eZero,
        .alphaBlendOp        = vk::BlendOp::eAdd,
        .colorWriteMask      = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA };
    vk::PipelineColorBlendStateCreateInfo colorBlendingCreateInfo {
        .logicOpEnable = vk::False,
        .logicOp = vk::LogicOp::eCopy,
        .attachmentCount = 1,
        .pAttachments = &colorBlendAttachment };
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

void Pipeline::createPipelineDescriptorLayout( const vk::raii::Device& device, const std::vector<vk::DescriptorSetLayout>& descriptorSetLayouts )
{
    vk::PipelineLayoutCreateInfo pipelineLayoutInfo {
        .setLayoutCount = static_cast<uint32_t>(descriptorSetLayouts.size()),
        .pSetLayouts = descriptorSetLayouts.data(),
        .pushConstantRangeCount = 0 };

    pipelineLayout = vk::raii::PipelineLayout( device, pipelineLayoutInfo );
}

void Pipeline::createGraphicsPipeline( const LogicalDevice& device, const vk::raii::ShaderModule& shaderModule, const SwapChain& framebuffer, const vk::PipelineVertexInputStateCreateInfo& vertexInputInfo )
{
    std::vector<vk::PipelineShaderStageCreateInfo> programmableModulesCreateInfo = createProgrammableModules( shaderModule );

    vk::PipelineInputAssemblyStateCreateInfo inputAssemblyCreateInfo { .topology = vk::PrimitiveTopology::eTriangleList };

    vk::PipelineViewportStateCreateInfo viewportCreateInfo = createViewport( framebuffer.imageResolution );
    // WE'RE NOT BAKING IT INTO THE PIPELINE ANYMORE (VIEWPORT IS DYNAMIC, ABOVE LINE IS USELESS)
    vk::PipelineViewportStateCreateInfo      viewportState{.viewportCount = 1, .scissorCount = 1};
	std::vector<vk::DynamicState>      dynamicStates = {vk::DynamicState::eViewport, vk::DynamicState::eScissor};
	vk::PipelineDynamicStateCreateInfo dynamicState{.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size()), .pDynamicStates = dynamicStates.data()};

    vk::PipelineRasterizationStateCreateInfo rasterizerCreateInfo = createRasterizer();
    vk::PipelineMultisampleStateCreateInfo multisamplingCreateInfo = createMultisampling( device.msaaSamples );
    vk::PipelineDepthStencilStateCreateInfo depthStencilCreateInfo = createDepthStencil();
    vk::PipelineColorBlendStateCreateInfo colorBlendCreateInfo = createColorBlending();

    vk::GraphicsPipelineCreateInfo graphicsPipelineCreateInfo {
    .stageCount          = static_cast<uint32_t>(programmableModulesCreateInfo.size()),
    .pStages             = programmableModulesCreateInfo.data(),
    .pVertexInputState   = &vertexInputInfo, // Is there a better way or just leave it to the caller? Maybe a member or something? idk.
    .pInputAssemblyState = &inputAssemblyCreateInfo,
    .pViewportState      = &viewportState, // SWAP THIS TO VIEWPORTCREATEINFO IF BAKED, AND DELETE THE .SETVIEWPORT/SCISSOR IN SUBMISSION
    .pRasterizationState = &rasterizerCreateInfo,
    .pMultisampleState   = &multisamplingCreateInfo,
    .pDepthStencilState  = &depthStencilCreateInfo,
    .pColorBlendState    = &colorBlendCreateInfo,
    .pDynamicState       = &dynamicState, // PUT THIS BACK TO NULLPTR IF BAKED
    .layout              = pipelineLayout,
    .renderPass          = nullptr };

    vk::Format depthFormat = device.findSupportedFormat(
        { vk::Format::eD32Sfloat, vk::Format::eD32SfloatS8Uint, vk::Format::eD24UnormS8Uint },
        vk::ImageTiling::eOptimal, vk::FormatFeatureFlagBits::eDepthStencilAttachment );
    vk::PipelineRenderingCreateInfo renderingCreateInfo = createRendering( framebuffer.surfaceFormat.format, depthFormat );

    vk::StructureChain<vk::GraphicsPipelineCreateInfo, vk::PipelineRenderingCreateInfo> pipelineCreateInfo { graphicsPipelineCreateInfo, renderingCreateInfo };

    if ( !*pipelineLayout )
        std::cerr << "Error! The Pipeline's Descriptor Layout has NOT been defined!\n";

    pipeline = vk::raii::Pipeline( device.logicalDevice, nullptr, pipelineCreateInfo.get<vk::GraphicsPipelineCreateInfo>() );
}