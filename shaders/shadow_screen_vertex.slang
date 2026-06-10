// https://github.com/SaschaWillems/Vulkan/blob/master/shaders/glsl/shadowmapping/scene.vert

struct VSInput {
    float3 inPosition;
    float3 inColor;
    float2 inTexCoord;
    float3 inNormal;
};

// Binding 0: used to create shadow data.
struct OnscreenBuffer
{
    float4x4 projection;
    float4x4 view;
    float4x4 model;
    float4x4 depthBiasMVP;
    float4 lightPos;
    // Used for depth map visualization
    float zNear;
    float zFar;
};
[[vk::binding(0, 0)]] ConstantBuffer<OnscreenBuffer> ubo;

struct VSOutput
{
    float4 outPosition : SV_Position; // gl_position is SV_Position but for openGL's shading language.
    float3 outNormal;
    float3 outColor;
    float3 outViewVec;
    float3 outLightVec;
    float4 outShadowCoord;
};

// This is used to convert GPU outputted coordinates from CLIP SPACE (Window: [-1.0 to 1.0]) into TEXTURE COORDINATES, where depth maps have values ranging from [0.0 to 1.0]
    // Static in slang is required because it's telling slang that this variable belongs to the shader file, and is used as a normal variable -- it's not input or output.
const static float4x4 biasMatrix = float4x4(0.5, 0.0, 0.0, 0.0,     // Multiply X by 0.5
                                            0.0, 0.5, 0.0, 0.0,     // Multiply Y by 0.5
                                            0.0, 0.0, 1.0, 0.0,     // Multiply Z by 1.0 (keeps as it is)
                                            0.5, 0.5, 0.0, 1.0);    // Adds 0.5 to X and Y

[shader("vertex")]
VSOutput vertMain(VSInput input)
{
    VSOutput output;

    output.outColor = input.inColor;

    output.outPosition = mul(ubo.projection, mul(ubo.view, mul(ubo.model, float4(input.inPosition, 1.0))));

    output.outNormal = mul( float3x3(ubo.model), input.inNormal );

    output.outLightVec = normalize(ubo.lightPos.xyz - input.inPosition);

    float4 pos = mul(ubo.model, float4(input.inPosition, 1.0));
    output.outViewVec = -pos.xyz;

    output.outShadowCoord = (mul( mul(biasMatrix, ubo.depthBiasMVP), float4(input.inPosition, 1.0) ));
    // In the code snippet, he multiplies the biasMatrix * uboDepth by an identity matrix, 1x1, which is PROBABLY an error.
    // So, we just multiply everything as he does, minus the ubo.model, which in his case is an identity matrix.
    // He also renames depthBiasMVP to LightSpace.

    return output;
};