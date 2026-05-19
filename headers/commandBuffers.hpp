#pragma once

#include "includes.hpp"

namespace CommandPool
{
    vk::raii::CommandPool createCommandPool( const vk::raii::Device& device, uint32_t deviceQueueIndex, vk::Flags<vk::CommandPoolCreateFlagBits> flags );
};

struct DedicatedCommandBuffers
{
    static vk::raii::CommandPool* commandPool;
    std::vector<vk::raii::CommandBuffer> commandBuffers;

    static void initialize( vk::raii::CommandPool& pool );
    void createCommandBuffers( const vk::raii::Device& device, uint32_t cmdBufferCount );
};

struct TransientCommandBuffer
{
    static vk::raii::CommandPool* commandPool;
    static vk::raii::Device* device;
    static vk::raii::Queue* queue;
    vk::raii::CommandBuffer commandBuffer = nullptr;

    static void initialize( vk::raii::CommandPool& pool, vk::raii::Device& logicalDevice, vk::raii::Queue& deviceQueue );
    void beginSingleTimeCommands(); // might be a better idea to just make a dedicated commandBuffer always in memory instead of continually creating a new one.
    void endSingleTimeCommands();
};