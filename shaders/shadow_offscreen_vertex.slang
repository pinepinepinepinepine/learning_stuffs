
struct VSInput {
    [[vk::location(0)]] float3 inPosition;
};

struct VSOutput {
    float4 outPosition : SV_Position;
};

// Probably gonna cut this and multiply proj * view * matrix into a single matrix within CPU's code. fix later.
struct OffscreenBuffer
{
    float4x4 projection;
    float4x4 view;
    float4x4 model;
};
[[vk::binding(0, 0)]] ConstantBuffer<OffscreenBuffer> ubo;

[shader("vertex")]
VSOutput vertMain(VSInput input)
{
    VSOutput output;

    output.outPosition = mul(ubo.projection, mul(ubo.view, mul(ubo.model, float4(input.inPosition, 1.0))));

    return output;
}