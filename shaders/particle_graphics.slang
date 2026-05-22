struct VSInput {
    float2 inPosition;
    float4 inColor;
};

struct VSOutput
{
    float4 pos : SV_Position;
    float pointSize : SV_PointSize;
    float3 fragColor : COLOR0;
};

struct PSInput
{
    float4 pos : SV_POSITION;
    float3 fragColor : COLOR0;
    float2 pointCoord : SV_PointCoord;
};
[[vk::binding(0, 0)]] RWStructuredBuffer<float2> debugGraphicsOut;
[shader("vertex")]
VSOutput vertMain(VSInput input, uint vid: SV_VertexID) {
    VSOutput output;
    output.pointSize = 14.0;
    output.pos = float4(input.inPosition, 1.0, 1.0);
    output.fragColor = input.inColor.rgb;
    //output.pointCoord = (input.inPosition * float2(1.0, -1.0) + float2(1.0)) / 2.0;

    debugGraphicsOut[vid] = input.inPosition.xy;

    return output;
}

[shader("fragment")]
float4 fragMain(PSInput input) : SV_TARGET {
    float2 coord = input.pointCoord - float2(0.5);
    return float4(input.fragColor, length(coord) - 0.2 );
}