#pragma once

#include "includes.hpp"
#include "gpuMemoryObjects.hpp"
#include "device.hpp"
#include "commandBuffers.hpp"

struct Image : GPUMemoryObject // a vkImage is stored within GPU memory.
{
    VkImage image = nullptr;
    vk::raii::ImageView imageView = nullptr;

    void changeImageLayout(
        vk::ImageLayout old_layout, vk::ImageLayout new_layout, vk::AccessFlags2 src_access_mask, vk::AccessFlags2 dst_access_mask, vk::PipelineStageFlags2 src_stage_mask,
        vk::PipelineStageFlags2 dst_stage_mask, vk::ImageAspectFlags image_aspect_flags, uint32_t mipLevels, vk::raii::CommandBuffer* dedicatedBuffer = nullptr );
    void copyBufferDataToImage( const vk::raii::Buffer& buffer, uint32_t width, uint32_t height );
    void generateMipmaps( const vk::raii::PhysicalDevice& device, vk::Format imageFormat, int32_t texWidth, int32_t texHeight, uint32_t mipLevels );
    void createImageView( const vk::raii::Device& device, vk::Format format, vk::ImageAspectFlagBits aspectFlags, uint32_t mipLevels );
    void createImage( const LogicalDevice& device, uint32_t width, uint32_t height, uint32_t mipLevels, vk::SampleCountFlagBits numSamples,
        vk::Format format, vk::ImageTiling tiling, vk::ImageUsageFlags usage, vk::MemoryPropertyFlags properties, vk::ImageAspectFlagBits imageViewAspectFlags_type );
    void cleanupImage( const vk::raii::Device& device );
};
