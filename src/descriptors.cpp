#include "../headers/descriptors.hpp"

namespace DescriptorPool
{
    vk::raii::DescriptorPool createDescriptorPool( const vk::raii::Device& device, std::vector<vk::DescriptorPoolSize>& total_poolSize, uint32_t maxDescriptorSets )
    {
        vk::DescriptorPoolCreateInfo poolCreateInfo {
            .flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet,
            .maxSets = maxDescriptorSets,
            .poolSizeCount = static_cast<uint32_t>( total_poolSize.size() ),
            .pPoolSizes = total_poolSize.data() };

        return vk::raii::DescriptorPool( device, poolCreateInfo );
    }
};

void Descriptor::setDescriptorsPool( vk::raii::DescriptorPool& pool )
{
    descriptorPool = &pool;
}

void Descriptor::createDescriptorSetLayout( const vk::raii::Device& device, std::vector<vk::DescriptorSetLayoutBinding>& bindings )
{
    vk::DescriptorSetLayoutCreateInfo graphicsLayoutInfo {
        .bindingCount = static_cast<uint32_t>( bindings.size() ),
        .pBindings = bindings.data()
    };

    descriptorSetLayout = vk::raii::DescriptorSetLayout( device, graphicsLayoutInfo );
}

void Descriptor::createEmptyDescriptorSets( const vk::raii::Device& device, uint32_t setCount )
{
    descriptorSets.clear();


    std::vector<vk::DescriptorSetLayout> layouts( setCount, *descriptorSetLayout );

    if ( layouts.empty() )
        throw std::runtime_error("No Descriptor Set Layouts provided!");

    vk::DescriptorSetAllocateInfo allocInfo {
        .descriptorPool = *descriptorPool,
        .descriptorSetCount = static_cast<uint32_t>( layouts.size() ),
        .pSetLayouts = layouts.data() };

    descriptorSets = device.allocateDescriptorSets( allocInfo );
}

void Descriptor::setUBOResource(
    const vk::raii::Device& device, const vk::raii::Buffer& ubo, vk::DescriptorType type, vk::DeviceSize range, size_t descriptorSetIndex, uint32_t binding )
{
    vk::DescriptorBufferInfo uboBufferInfo {
        .buffer = ubo,
        .offset = 0,
        .range  = range };

    vk::WriteDescriptorSet uboDescriptorWrite {
        .dstSet = descriptorSets[descriptorSetIndex],
        .dstBinding = binding,
        .dstArrayElement = 0,
        .descriptorCount = 1,
        .descriptorType = type,
        .pBufferInfo = &uboBufferInfo };

    device.updateDescriptorSets( uboDescriptorWrite, {} );
}

void Descriptor::setSamplerResource(
    const vk::raii::Device& device, const vk::Sampler& textureSampler, const vk::ImageView& textureImageView, size_t descriptorSetIndex, uint32_t binding )
{
    vk::DescriptorImageInfo samplerInfo {
        .sampler = textureSampler,
        .imageView = textureImageView,
        .imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal };

    vk::WriteDescriptorSet samplerDescriptorWrite {
        .dstSet = descriptorSets[descriptorSetIndex],
        .dstBinding = binding,
        .dstArrayElement = 0,
        .descriptorCount = 1,
        .descriptorType = vk::DescriptorType::eCombinedImageSampler,
        .pImageInfo = &samplerInfo };

    device.updateDescriptorSets( samplerDescriptorWrite, {} );
}