#include "../headers/image.hpp"

void Image::changeImageLayout( vk::ImageLayout old_layout, vk::ImageLayout new_layout, vk::AccessFlags2 src_access_mask, vk::AccessFlags2 dst_access_mask,
     vk::PipelineStageFlags2 src_stage_mask, vk::PipelineStageFlags2 dst_stage_mask, vk::ImageAspectFlags image_aspect_flags, uint32_t mipLevels, vk::raii::CommandBuffer* dedicatedBuffer )
{
    vk::ImageMemoryBarrier2 barrier = {
        .srcStageMask        = src_stage_mask,
        .srcAccessMask       = src_access_mask,
        .dstStageMask        = dst_stage_mask,
        .dstAccessMask       = dst_access_mask,
        .oldLayout           = old_layout,
        .newLayout           = new_layout,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image               = image,
        .subresourceRange    = {
            .aspectMask     = image_aspect_flags,
            .baseMipLevel   = 0,
            .levelCount     = mipLevels,
            .baseArrayLayer = 0,
            .layerCount     = 1 } };

    vk::DependencyInfo dependency_info = {
        .dependencyFlags         = {},
        .imageMemoryBarrierCount = 1,
        .pImageMemoryBarriers    = &barrier };

    if ( dedicatedBuffer == nullptr )
    {
        TransientCommandBuffer submitCmdBuffer;
        submitCmdBuffer.beginSingleTimeCommands();
        submitCmdBuffer.commandBuffer.pipelineBarrier2( dependency_info );
        submitCmdBuffer.endSingleTimeCommands();
    }
    else { dedicatedBuffer->pipelineBarrier2( dependency_info ); }
}

void Image::copyBufferDataToImage( const vk::raii::Buffer& buffer, uint32_t width, uint32_t height )
{
    vk::BufferImageCopy region {
        .bufferOffset = 0,
        .bufferRowLength = 0,
        .bufferImageHeight = 0,
        .imageSubresource = { vk::ImageAspectFlagBits::eColor, 0, 0, 1 },
        .imageOffset = { 0, 0, 0 },
        .imageExtent = { width, height, 1 } };

    TransientCommandBuffer submitCmdBuffer;
    submitCmdBuffer.beginSingleTimeCommands();
    submitCmdBuffer.commandBuffer.copyBufferToImage( buffer, image, vk::ImageLayout::eTransferDstOptimal, {region} );
    submitCmdBuffer.endSingleTimeCommands();
}

void Image::generateMipmaps( const vk::raii::PhysicalDevice& physDevice, vk::Format imageFormat, int32_t texWidth, int32_t texHeight, uint32_t mipLevels )
{
    vk::FormatProperties formatProperties = physDevice.getFormatProperties(imageFormat);
    if ( !( formatProperties.optimalTilingFeatures & vk::FormatFeatureFlagBits::eSampledImageFilterLinear ) )
        throw std::runtime_error("texture image format does not support linear blitting!");

    TransientCommandBuffer submitCmdBuffer;
    submitCmdBuffer.beginSingleTimeCommands();

    vk::ImageMemoryBarrier barrier {
        .srcAccessMask = vk::AccessFlagBits::eTransferWrite,
        .dstAccessMask = vk::AccessFlagBits::eTransferRead,
        .oldLayout = vk::ImageLayout::eTransferDstOptimal,
        .newLayout = vk::ImageLayout::eTransferSrcOptimal,
        .srcQueueFamilyIndex = vk::QueueFamilyIgnored,
        .dstQueueFamilyIndex = vk::QueueFamilyIgnored,
        .image = image,
        .subresourceRange {
            .aspectMask = vk::ImageAspectFlagBits::eColor,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 1 } };

    int32_t mipWidth = texWidth;
    int32_t mipHeight = texHeight;

    for ( uint32_t i = 1; i < mipLevels; i++ )
    {
        barrier.subresourceRange.baseMipLevel = i - 1;
        barrier.oldLayout = vk::ImageLayout::eTransferDstOptimal;
        barrier.newLayout = vk::ImageLayout::eTransferSrcOptimal;
        barrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
        barrier.dstAccessMask = vk::AccessFlagBits::eTransferRead;

        submitCmdBuffer.commandBuffer.pipelineBarrier( vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eTransfer, {}, {}, {}, barrier );

        vk::ArrayWrapper1D<vk::Offset3D, 2> offsets, dstOffsets;
        offsets[0] = vk::Offset3D(0, 0, 0);
        offsets[1] = vk::Offset3D(mipWidth, mipHeight, 1);
        dstOffsets[0] = vk::Offset3D(0, 0, 0);
        dstOffsets[1] = vk::Offset3D( mipWidth > 1 ? mipWidth / 2 : 1, mipHeight > 1 ? mipHeight / 2 : 1, 1 );

        vk::ImageBlit blit {
            .srcSubresource = vk::ImageSubresourceLayers( vk::ImageAspectFlagBits::eColor, i - 1, 0, 1),
            .srcOffsets = offsets,
            .dstSubresource = vk::ImageSubresourceLayers( vk::ImageAspectFlagBits::eColor, i, 0, 1),
            .dstOffsets = dstOffsets };

        submitCmdBuffer.commandBuffer.blitImage(
            image,
            vk::ImageLayout::eTransferSrcOptimal,
            image,
            vk::ImageLayout::eTransferDstOptimal,
            { blit },
            vk::Filter::eLinear );

        barrier.oldLayout = vk::ImageLayout::eTransferSrcOptimal;
        barrier.newLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
        barrier.srcAccessMask = vk::AccessFlagBits::eTransferRead;
        barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;

        submitCmdBuffer.commandBuffer.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eFragmentShader, {}, {}, {}, barrier);

        if (mipWidth > 1)
            mipWidth /= 2;
        if (mipHeight > 1)
            mipHeight /= 2;
    }

    // For the final mip level.
    barrier.subresourceRange.baseMipLevel = mipLevels - 1;
    barrier.oldLayout = vk::ImageLayout::eTransferDstOptimal;
    barrier.newLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
    barrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
    barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;
    submitCmdBuffer.commandBuffer.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eFragmentShader, {}, {}, {}, barrier);

    submitCmdBuffer.endSingleTimeCommands(); // Batched all recorded commands into one.
}

void Image::createImageView( const vk::raii::Device& device, vk::Format format, vk::ImageAspectFlagBits aspectFlags, uint32_t mipLevels )
{
    vk::ImageViewCreateInfo viewInfo {
        .image            = image,
        .viewType         = vk::ImageViewType::e2D,
        .format           = format,
        .subresourceRange = { aspectFlags, 0, mipLevels, 0, 1 } };
    imageView = vk::raii::ImageView( device, viewInfo );
}

void Image::createImage( const LogicalDevice& device, uint32_t width, uint32_t height, uint32_t mipLevels, vk::SampleCountFlagBits numSamples,
    vk::Format format, vk::ImageTiling tiling, vk::ImageUsageFlags usage, vk::MemoryPropertyFlags properties, vk::ImageAspectFlagBits imageViewAspectFlags_type )
{
    vk::ImageCreateInfo imageInfo {
        .imageType = vk::ImageType::e2D,
        .format = format,
        .extent = {width, height, 1},
        .mipLevels = mipLevels,
        .arrayLayers = 1,
        .samples = numSamples,
        .tiling = tiling,
        .usage = usage,
        .sharingMode = vk::SharingMode::eExclusive };
    vkCreateImage( *device.logicalDevice, imageInfo, nullptr, &image );

    vk::MemoryRequirements memRequirements {};
    vkGetImageMemoryRequirements( *device.logicalDevice, image, &(*memRequirements) );
    vk::MemoryAllocateInfo allocInfo {
        .allocationSize = memRequirements.size,
        .memoryTypeIndex = findGPUBufferMemoryType( device.physicalDevice, memRequirements.memoryTypeBits, properties ) };
    gpuMemory = vk::raii::DeviceMemory( device.logicalDevice, allocInfo );

    vkBindImageMemory( *device.logicalDevice, image, *gpuMemory, 0 );

    createImageView( device.logicalDevice, format, imageViewAspectFlags_type, mipLevels );
}

void Image::cleanupImage( const vk::raii::Device& device )
{
    vkDestroyImage( *device, image, nullptr );
    imageView = nullptr;
}