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

[shader("compute")]
[numthreads(256, 1, 1)]
void compMain(uint3 threadId: SV_DispatchThreadID)
{
    uint index = threadId.x;

    particlesOut[index].position = particlesIn[index].position + particlesIn[index].velocity * ubo.deltaTime;
    particlesOut[index].velocity = particlesIn[index].velocity;

    // Flip movement at window border
    if ((particlesOut[index].position.x <= -1.0) || (particlesOut[index].position.x >= 1.0)) {
        particlesOut[index].velocity.x = -particlesOut[index].velocity.x;
    }
    if ((particlesOut[index].position.y <= -1.0) || (particlesOut[index].position.y >= 1.0)) {
        particlesOut[index].velocity.y = -particlesOut[index].velocity.y;
    }

    debugOut[index] = particlesOut[index].position;
}