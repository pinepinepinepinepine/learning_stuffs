#pragma once

#include "includes.hpp"
#include "gpuMemoryObjects.hpp"
#include "commandBuffers.hpp"
#include "device.hpp"

struct GPUBuffer : GPUMemoryObject
{
    vk::raii::Buffer gpuBuffer = nullptr;
    void* gpuBufferMapped = nullptr;

    void copyBufferInto( const vk::raii::Buffer& srcBuffer, vk::DeviceSize size );

    template <typename T>
    void muleBuffer( const LogicalDevice& device, const std::vector<T>& data )
    {
        if ( !data.size() )
            throw std::runtime_error("No data to mule over!");

        vk::DeviceSize bufferSize = sizeof(data[0]) * data.size();

        GPUBuffer muleBuffer;
        muleBuffer.createGPUBuffer(
            device,
            bufferSize,
            vk::BufferUsageFlagBits::eTransferSrc,
            vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
            true );
        memcpy( muleBuffer.gpuBufferMapped, data.data(), bufferSize );

        muleBuffer.unmapGPUMemory();

        this->copyBufferInto( muleBuffer.gpuBuffer, bufferSize );
    }

    void unmapGPUMemory();
    void createGPUBuffer( const LogicalDevice& device, vk::DeviceSize size, vk::BufferUsageFlags usage, vk::MemoryPropertyFlags properties, bool mapMemory );
};