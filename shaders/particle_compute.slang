struct Particle {
    float2 position;
    float2 velocity;
    float4 color;
};
[[vk::binding(0, 0)]] StructuredBuffer<Particle> particlesIn; // Can we just ditch this and use Particle struct directly? instead of "particleSSBO"
[[vk::binding(1, 0)]] RWStructuredBuffer<Particle> particlesOut;

struct ParticleTimeUniformBuffer {
    float deltaTime;
};
[[vk::binding(2, 0)]] ConstantBuffer<ParticleTimeUniformBuffer> ubo;

[[vk::binding(3, 0)]] RWStructuredBuffer<float2> debugOut;


struct PushConstants {
    uint startIndex;
    uint count;
};
[[vk::push_constant]] PushConstants pushConstants;

[shader("compute")]
[numthreads(256, 1, 1)]
void compMain(uint3 threadId: SV_DispatchThreadID)
{
    uint index = threadId.x;

    // Only process particles within our assigned range
    if (index >= pushConstants.count) {
        return;
    }

    // Adjust index to start from our assigned start index
    uint globalIndex = pushConstants.startIndex + index;

    particlesOut[globalIndex].position = particlesIn[globalIndex].position + particlesIn[globalIndex].velocity * ubo.deltaTime;
    particlesOut[globalIndex].velocity = particlesIn[globalIndex].velocity;

    // Flip movement at window border
    if ((particlesOut[globalIndex].position.x <= -1.0) || (particlesOut[globalIndex].position.x >= 1.0)) {
        particlesOut[globalIndex].velocity.x = -particlesOut[globalIndex].velocity.x;
    }
    if ((particlesOut[globalIndex].position.y <= -1.0) || (particlesOut[globalIndex].position.y >= 1.0)) {
        particlesOut[globalIndex].velocity.y = -particlesOut[globalIndex].velocity.y;
    }

    debugOut[globalIndex] = particlesOut[globalIndex].position;
}