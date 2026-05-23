#include "../headers/thread.hpp"


vk::raii::CommandPool& ThreadManager::getCommandPool(uint32_t threadIndex)
{
    std::lock_guard<std::mutex> lock(resourceMutex);
    return commandPools[threadIndex];
}

vk::raii::CommandBuffer& ThreadManager::getCommandBuffer(uint32_t index)
{
    std::lock_guard<std::mutex> lock(resourceMutex);
    return commandBuffers[index];
}

void ThreadManager::createThreadCommandPools( vk::raii::Device& device, uint32_t queueFamilyIndex, uint32_t threadCount )
{
    std::lock_guard<std::mutex> lock( resourceMutex );

    commandPools.clear();
    for (uint32_t i = 0; i < threadCount; i++) {
        vk::CommandPoolCreateInfo poolInfo { .flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer, .queueFamilyIndex = queueFamilyIndex };
        commandPools.emplace_back(device, poolInfo);
    }
}

void ThreadManager::allocateCommandBuffers(vk::raii::Device& device, uint32_t threadCount, uint32_t buffersPerThread)
{
    std::lock_guard<std::mutex> lock(resourceMutex);

    commandBuffers.clear();
    for (uint32_t i = 0; i < threadCount; i++) {
        vk::CommandBufferAllocateInfo allocInfo{
            .commandPool = *commandPools[i],
            .level = vk::CommandBufferLevel::ePrimary,
            .commandBufferCount = buffersPerThread
        };
        auto threadBuffers = device.allocateCommandBuffers(allocInfo);
        for (auto& buffer : threadBuffers) {
            commandBuffers.emplace_back(std::move(buffer));
        }
    }
}


void ThreadManager::signalThreadsToWork()
{
    for (uint32_t i = 0; i < THREAD_COUNT; i++)
	{
		threadWorkDone[i].store( false, std::memory_order_release );
	}

    // Memory barrier to ensure all threads see the updated threadWorkDone values -- see NOTES.
    std::atomic_thread_fence(std::memory_order_seq_cst);

    // Only signal the first thread to start work -- due to us setting workReady to true, when the thread gets awaken up on its .wait(), its lambda function'll return true, allowing it to pass
        // ONLY the first thread is currently allowed to run: all the other threads WILL wake up due to the notify_all() call below, HOWEVER, their respective lambdas return false, leading them to immediately go to sleep
    threadWorkReady[0].store(true, std::memory_order_release);

    // Notify all threads in case they're waiting on the condition variable
    {
        std::lock_guard<std::mutex> lock(workCompleteMutex);
        workCompleteCv.notify_all();
    }
}

void ThreadManager::waitForthreadsToCompleteWork()
{
    std::unique_lock<std::mutex> lock(workCompleteMutex);

    // Wait for the last thread to complete with a timeout. Wait result is...
        // True, if the predicate returned true.
        // False, if the timeout occured BEFORE the predicate returned true.
    auto waitResult = workCompleteCv.wait_for(lock, std::chrono::milliseconds(3000), [this]() {
        return threadWorkDone[THREAD_COUNT - 1].load(std::memory_order_acquire); });

        // TODO: check if threads are executed sequentially: checking for the last one AND NOT FOR ALL OF THEM seems like a PRETTY big problem since it assumes threads finish sequentially.

    // If we timed out, force completion
    if ( waitResult == false )
    {
        std::cout << "Threads completed by TIMEOUT! INVESTIGATE IT.\n";

        for (uint32_t i = 0; i < THREAD_COUNT; i++)
        {
            threadWorkDone[i].store(true, std::memory_order_release);
            threadWorkReady[i].store(false, std::memory_order_release);
        }

        // Notify all threads
        workCompleteCv.notify_all();
        lock.unlock();

        // Give threads a chance to respond to the forced completion
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}


void ThreadManager::stopThreads()
{
    exitAllThreads.store( true, std::memory_order_release );

    for (uint32_t i = 0; i < THREAD_COUNT; i++)
    {
        threadWorkDone[i].store(true, std::memory_order_release);
        threadWorkReady[i].store(false, std::memory_order_release);
    }

    // Notify all threads in case they're waiting on the condition variable
    {
        std::lock_guard<std::mutex> lock(workCompleteMutex);
        workCompleteCv.notify_all();
    }

    for (auto &thread : workerThreads)
    {
        if ( thread.joinable() ) // a thread is joinable when it's in its zombie state -- it finished and exited its threadWork() function
        {
            thread.join();
        }
    }

    workerThreads.clear();
}