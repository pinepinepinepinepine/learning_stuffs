#pragma once

#include "includes.hpp"

struct GPUMemoryObject
{
    vk::raii::DeviceMemory gpuMemory = nullptr;

    uint32_t findGPUBufferMemoryType( const vk::raii::PhysicalDevice& physicalDevice, uint32_t typeFilter, vk::MemoryPropertyFlags properties );
};