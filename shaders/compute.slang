// what a particle is
struct Particle {
    float2 position;
    float2 velocity;
    float4 color;
};

// the struct for the uniform buffer we made
struct computeUniformBuffer {
    float deltaTime;
};
[[vk::binding(0, 0)]] ConstantBuffer<computeUniformBuffer> computeUBO;

// the struct for the SSBO we made
struct ParticleSSBO {
    Particle particles;
};
// explicitly mention bindings, it'll be done implicitly if we don't specify.
[[vk::binding(2, 0)]] StructuredBuffer<ParticleSSBO> particlesIn;
[[vk::binding(1, 0)]] RWStructuredBuffer<ParticleSSBO> particlesOut;

// just to clarify, .slang code is to communicate to the GPU (not to the CPU, hence why we don't use... C++... and so we use another language)
// So I presume computeUBO.deltaTime is updated from the CPU code (hence why we don't modify the .deltaTime here)
[shader("compute")]
[numthreads(256, 1, 1)] // the number of threads defines the number of invocations within this compute shader's work group. we only work on a linear 1D array of particles, hence only X.
void compMain(uint3 threadId: SV_DispatchThreadID) // SV_DispatchThreadID is a built-in input variables, but for the compute shader -- this uniquely identifies the current invocation iteration across the entire dispatch call.
{

    // we've specified 8192 as our total particles, so thread id is [0, 8192] (we use it to index into the particle array), using this, we can specify a specific particle to work on for this iteration/invocation
    uint index = threadId.x;

    particlesOut[index].particles.position = particlesIn[index].particles.position + particlesIn[index].particles.velocity.xy * computeUBO.deltaTime;
    particlesOut[index].particles.velocity = particlesIn[index].particles.velocity;

    // Flip movement at window border
    if ((particlesOut[index].particles.position.x <= -1.0) || (particlesOut[index].particles.position.x >= 1.0)) {
        particlesOut[index].particles.velocity.x = -particlesOut[index].particles.velocity.x;
    }
    if ((particlesOut[index].particles.position.y <= -1.0) || (particlesOut[index].particles.position.y >= 1.0)) {
        particlesOut[index].particles.velocity.y = -particlesOut[index].particles.velocity.y;
    }
}