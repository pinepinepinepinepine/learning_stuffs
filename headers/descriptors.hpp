#pragma once

#include "includes.hpp"

namespace DescriptorPool
{
    vk::raii::DescriptorPool createDescriptorPool( const vk::raii::Device& device, std::vector<vk::DescriptorPoolSize>& total_poolSize, uint32_t maxDescriptorSets );
};

struct Descriptor
{
    vk::raii::DescriptorPool* descriptorPool = nullptr;
    vk::raii::DescriptorSetLayout descriptorSetLayout = nullptr;
    std::vector<vk::raii::DescriptorSet> descriptorSets;

    void setDescriptorsPool( vk::raii::DescriptorPool& pool );
    void createDescriptorSetLayout( const vk::raii::Device& device, std::vector<vk::DescriptorSetLayoutBinding>& bindings );
    void createEmptyDescriptorSets( const vk::raii::Device& device, uint32_t setCount );
    void setBufferResource( const vk::raii::Device& device, const vk::raii::Buffer& ubo, vk::DescriptorType type, vk::DeviceSize range, size_t descriptorSetIndex, uint32_t binding );
    void setSamplerResource( const vk::raii::Device& device, const vk::Sampler& textureSampler, const vk::ImageView& textureImageView, size_t descriptorSetIndex, uint32_t binding, vk::ImageLayout imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal );
    void setSampledImageResource( const vk::raii::Device& device, const vk::ImageView& textureImageView, size_t descriptorSetIndex, uint32_t binding, vk::ImageLayout imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal );
};