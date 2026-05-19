#include "../headers/gpuMemoryObjects.hpp"

uint32_t GPUMemoryObject::findGPUBufferMemoryType( const vk::raii::PhysicalDevice& physicalDevice, uint32_t typeFilter, vk::MemoryPropertyFlags properties )
{
    vk::PhysicalDeviceMemoryProperties memProperties = physicalDevice.getMemoryProperties();

    for ( uint32_t i = 0; i < memProperties.memoryTypeCount; i++ )
    {
        if ( ( typeFilter & ( 1 << i ) ) && ( ( memProperties.memoryTypes[i].propertyFlags & properties ) == properties ) )
            return i;
    }
    throw std::runtime_error("failed to find suitable GPU memory type!");
}