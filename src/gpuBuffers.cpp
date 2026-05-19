#include "../headers/gpuBuffers.hpp"

void GPUBuffer::copyBufferInto( const vk::raii::Buffer& srcBuffer, vk::DeviceSize size )
{
    TransientCommandBuffer cmdBuffer;
    cmdBuffer.beginSingleTimeCommands();
    cmdBuffer.commandBuffer.copyBuffer( srcBuffer, gpuBuffer, vk::BufferCopy( 0, 0, size ) );
    cmdBuffer.endSingleTimeCommands();
}

void GPUBuffer::unmapGPUMemory()
{
    if ( gpuBufferMapped )
    {
        gpuMemory.unmapMemory();
        gpuBufferMapped = nullptr;
    }
}

void GPUBuffer::createGPUBuffer( const LogicalDevice& device, vk::DeviceSize size, vk::BufferUsageFlags usage, vk::MemoryPropertyFlags properties, bool mapMemory )
{
    vk::BufferCreateInfo bufferInfo {
        .size = size,
        .usage = usage,
        .sharingMode = vk::SharingMode::eExclusive };
    gpuBuffer = vk::raii::Buffer( device.logicalDevice, bufferInfo );

    vk::MemoryRequirements memoryRequirements = gpuBuffer.getMemoryRequirements();
    vk::MemoryAllocateInfo memoryAllocateInfo {
        .allocationSize  = memoryRequirements.size,
        .memoryTypeIndex = findGPUBufferMemoryType( device.physicalDevice, memoryRequirements.memoryTypeBits, properties ) };
    gpuMemory = vk::raii::DeviceMemory( device.logicalDevice, memoryAllocateInfo );

    gpuBuffer.bindMemory( *gpuMemory, 0 );
    if ( mapMemory && ( properties & vk::MemoryPropertyFlagBits::eHostVisible ) )
        gpuBufferMapped = gpuMemory.mapMemory( 0, memoryRequirements.size );
}
