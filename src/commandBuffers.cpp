#include "../headers/commandBuffers.hpp"

vk::raii::CommandPool* DedicatedCommandBuffers::commandPool = nullptr;
vk::raii::CommandPool* TransientCommandBuffer::commandPool = nullptr;
vk::raii::Device* TransientCommandBuffer::device = nullptr;
vk::raii::Queue* TransientCommandBuffer::queue = nullptr;

namespace CommandPool
{
    vk::raii::CommandPool createCommandPool( const vk::raii::Device& device, uint32_t deviceQueueIndex, vk::Flags<vk::CommandPoolCreateFlagBits> flags )
    {
        vk::CommandPoolCreateInfo commandPoolCreateInfo {
        .flags = flags,
        .queueFamilyIndex = deviceQueueIndex };

        return vk::raii::CommandPool( device, commandPoolCreateInfo );
    }
};

void DedicatedCommandBuffers::initialize( vk::raii::CommandPool& pool )
{
    commandPool = &pool;
}

void DedicatedCommandBuffers::createCommandBuffers( const vk::raii::Device& device, uint32_t cmdBufferCount, vk::CommandBufferLevel level )
{
    commandBuffers.clear();
    vk::CommandBufferAllocateInfo commandBuffer_allocationInfo {
        .commandPool = *commandPool,
        .level = level,
        .commandBufferCount = cmdBufferCount };

    commandBuffers = vk::raii::CommandBuffers( device, commandBuffer_allocationInfo );
}

void TransientCommandBuffer::initialize( vk::raii::CommandPool& pool, vk::raii::Device& logicalDevice, vk::raii::Queue& deviceQueue )
{
    commandPool = &pool;
    device = &logicalDevice;
    queue = &deviceQueue;
}

void TransientCommandBuffer::beginSingleTimeCommands()
{
    vk::CommandBufferAllocateInfo allocInfo {
        .commandPool = *commandPool,
        .level = vk::CommandBufferLevel::ePrimary,
        .commandBufferCount = 1 };

    commandBuffer = std::move( device->allocateCommandBuffers( allocInfo ).front() );

    commandBuffer.begin( vk::CommandBufferBeginInfo { .flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit } );
}

void TransientCommandBuffer::endSingleTimeCommands()
{
    commandBuffer.end();

    vk::FenceCreateInfo fenceInfo{};
    vk::raii::Fence submissionFence( *device, fenceInfo );
    queue->submit( vk::SubmitInfo{ .commandBufferCount = 1, .pCommandBuffers = &*commandBuffer }, *submissionFence );
    if ( device->waitForFences( *submissionFence, vk::True, UINT64_MAX ) != vk::Result::eSuccess )
        throw std::runtime_error("failed to wait for fence!");
}