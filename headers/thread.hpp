#include "includes.hpp"
#include "particle.hpp"

constexpr uint32_t THREAD_COUNT = 8;

struct ParticleGroup
{
    uint32_t startIndex;
    uint32_t count;
};

class ThreadManager
{
    private:
    std::mutex resourceMutex;
    std::vector<vk::raii::CommandPool> commandPools;
    std::vector<vk::raii::CommandBuffer> commandBuffers; // Can we use our Command Buffer wrapper? Convert it to support threads.

    public:
    std::vector<std::atomic<bool>> threadWorkReady;
	std::vector<std::atomic<bool>> threadWorkDone;
    std::vector<ParticleGroup> particleGroups;
    std::vector<std::thread> workerThreads;
    std::atomic<bool> exitAllThreads { false };
    std::mutex queueSubmitMutex;
    std::mutex workCompleteMutex;
	std::condition_variable workCompleteCv;


    void createThreadCommandPools( vk::raii::Device& device, uint32_t queueFamilyIndex, uint32_t threadCount );
    vk::raii::CommandPool& getCommandPool(uint32_t threadIndex);
    void allocateCommandBuffers(vk::raii::Device& device, uint32_t threadCount, uint32_t buffersPerThread);
    vk::raii::CommandBuffer& getCommandBuffer(uint32_t index);

    void signalThreadsToWork();
    void waitForthreadsToCompleteWork();
    void stopThreads();
};