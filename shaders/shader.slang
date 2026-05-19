struct VSInput {
    float3 inPosition;
    float3 inColor;
    float2 inTexCoord;
};
//[[vk::binding(2, 0)]] RWStructuredBuffer<VSInput> debugVertexInput;

struct VSOutput
{
    float4 pos : SV_Position;
    float3 fragColor;
    float2 fragTexCoord;
};

struct modelUniformBuffer {
    float4x4 model;
    float4x4 view;
    float4x4 proj;
};
[[vk::binding(0, 0)]] ConstantBuffer<modelUniformBuffer> m_ubo;

[shader("vertex")]
VSOutput vertMain(VSInput input) {
    VSOutput output;
    //output.pos = mul(m_ubo.proj, mul(m_ubo.view, mul(m_ubo.model, float4(input.inPosition, 1.0))));
    output.pos = { input.inPosition, 1 };
    output.fragColor = input.inColor;
    output.fragTexCoord = input.inTexCoord;
    return output;
}

[[vk::binding(1, 0)]] Sampler2D texture;
[shader("fragment")]
float4 fragMain(VSOutput vertIn) : SV_TARGET {
    return texture.Sample(vertIn.fragTexCoord);
}


