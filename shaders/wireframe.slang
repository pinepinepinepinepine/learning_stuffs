struct VSInput {
    float3 inPosition;
};

struct VSOutput
{
    float4 pos : SV_Position;
};

// for some reason, this is required. I cannot change the type of pushConstants to float4x4. wtf?
struct PushConstants {
    float4x4 model;
};
[[vk::push_constant]] PushConstants pushConstants;

struct modelUniformBuffer {
    float4x4 view;
    float4x4 proj;
};
[[vk::binding(0, 0)]] ConstantBuffer<modelUniformBuffer> m_ubo;

[shader("vertex")]
VSOutput vertMain(VSInput input) {
    VSOutput output;
    output.pos = mul(m_ubo.proj, mul(m_ubo.view, mul(pushConstants.model, float4(input.inPosition, 1.0))));
    return output;
}

[shader("fragment")]
float4 fragMain(VSOutput vertIn) : SV_TARGET {
    return float4( 1.0, 1.0, 1.0, 1.0 );
}