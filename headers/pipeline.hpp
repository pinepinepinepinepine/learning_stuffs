#pragma once

#include "includes.hpp"
#include "device.hpp"
#include "swapChain.hpp"

namespace PipelineUtils
{
    [[nodiscard]] vk::raii::ShaderModule createShaderModule( vk::raii::Device& device, const std::string& filename );
};

class Pipeline
{
    vk::Viewport viewport;
    vk::Rect2D scissor;

    public:
    vk::raii::PipelineLayout pipelineLayout = nullptr;
    vk::raii::Pipeline pipeline = nullptr;

    std::vector<vk::PipelineShaderStageCreateInfo> createProgrammableModules( const vk::raii::ShaderModule& shaderModule );
    vk::PipelineViewportStateCreateInfo createViewport( vk::Extent2D dimensions );
    vk::PipelineRasterizationStateCreateInfo createRasterizer();
    vk::PipelineMultisampleStateCreateInfo createMultisampling( vk::SampleCountFlagBits samplesPer );
    vk::PipelineDepthStencilStateCreateInfo createDepthStencil();
    vk::PipelineColorBlendStateCreateInfo createColorBlending( vk::PipelineColorBlendAttachmentState& colorAttachmentInfo );
    vk::PipelineRenderingCreateInfo createRendering( const vk::Format& colorFormat, const vk::Format& depthFormat );
    void createPipelineDescriptorLayout( const vk::raii::Device& device, const std::vector<vk::DescriptorSetLayout>& descriptorSetLayouts );
    void createGraphicsPipeline( const LogicalDevice& device, const vk::raii::ShaderModule& shaderModule, const SwapChain& framebuffer, const vk::PipelineVertexInputStateCreateInfo& vertexInputInfo );

};