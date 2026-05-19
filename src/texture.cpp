#include "../headers/texture.hpp"

void Texture::setTextureSampler( vk::raii::Sampler& sampler )
{
    textureSampler = &sampler;
}

void Texture::createTextureImage( const LogicalDevice& device, const char *filename )
{
    int textureWidth, textureHeight, textureChannels;

    stbi_uc* pixels = stbi_load( filename, &textureWidth, &textureHeight, &textureChannels, STBI_rgb_alpha );
    if (!pixels)
        throw std::runtime_error("failed to load texture image!");

    mipLevels = static_cast<uint32_t>( std::floor( std::log2( std::max( textureWidth, textureHeight ) ) ) ) + 1;

    vk::DeviceSize imageSize = textureWidth * textureHeight * 4; // calculates the byte size of the image. just get the area (width x height), and multiply by bytes per pixel (4 in our case due to STBI_rgb_alpha)

    GPUBuffer gpuImageBuffer;

    gpuImageBuffer.createGPUBuffer(
        device,
        imageSize,
        vk::BufferUsageFlagBits::eTransferSrc,
        vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
        true );

    memcpy( gpuImageBuffer.gpuBufferMapped, pixels, imageSize );
    gpuImageBuffer.unmapGPUMemory();
    stbi_image_free( pixels ); // We copied the pixels onto our GPU buffer (the buffer does not go out of scope when we exit this function, it is in memory), hence free it.

    textureImage.createImage(
        device,
        textureWidth,
        textureHeight,
        mipLevels,
        vk::SampleCountFlagBits::e1,
        vk::Format::eR8G8B8A8Srgb,
        vk::ImageTiling::eOptimal,
        vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferSrc,
        vk::MemoryPropertyFlagBits::eDeviceLocal,
        vk::ImageAspectFlagBits::eColor );


    textureImage.changeImageLayout(
        vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal,
        {}, vk::AccessFlagBits2::eTransferWrite,
        vk::PipelineStageFlagBits2::eTopOfPipe, vk::PipelineStageFlagBits2::eTransfer,
        vk::ImageAspectFlagBits::eColor,
        mipLevels );

    textureImage.copyBufferDataToImage( gpuImageBuffer.gpuBuffer, static_cast<uint32_t>(textureWidth), static_cast<uint32_t>(textureHeight) );

    textureImage.generateMipmaps( device.physicalDevice, vk::Format::eR8G8B8A8Srgb, textureWidth, textureHeight, mipLevels );
}