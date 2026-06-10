// https://github.com/SaschaWillems/Vulkan/blob/master/shaders/glsl/shadowmapping/scene.frag

// VSInput is the VSOutput of the vertex shader.
struct VSInput
{
    float4 inPosition : SV_Position; // gl_position is SV_Position but for openGL's shading language.
    float3 inNormal;
    float3 inColor;
    float3 inViewVec;
    float3 inLightVec;
    float4 inShadowCoord;
};
[[vk::binding(1, 0)]] Sampler2D depthSampler;
[[vk::binding(2, 0)]] Texture2D depthSampledImage;

struct VSOutput
{
    float4 fragColour;
};

const static float shadowDarkness = 0.2;

float textureProjection(float4 pixelCoordinate, float2 positionOffset)
{
    float illuminated = 1.0; // How much light this pixel has: 1.0 means its fully illuminated.

    // the view of the light's perspective: if it's > 1.0, it's outside the far plane of the view, if it's < -1.0, it's behind the near plane.
    if (pixelCoordinate.z > -1.0 && pixelCoordinate.z < 1.0) // We only want to run this check for pixels that are actually in view.
    {
        // .x is because the data is stored in the first component -- it's how the GPU works. Even though we technically want the Z -- its because the depthMap is just containing depth values, and it populates the first component since depth is just a float value.
        float distanceDepth = depthSampler.Sample(pixelCoordinate.xy + positionOffset).x;
        // This is because depthSampler was CREATED with the image view, so theyre essentially a package of both the sampler instructions, AND the image itself.
        // However, for the sake of learning, I'm passing the Texture2D as a descriptor and using that directly -- it's probably a FAR better idea to just use depthSampler.sample, though.

        // Could rewrite it like this: // float distanceDepth = depthSampledImage.Sample(depthSampler.__getSampler(), pixelCoordinate.xy + positionOffset).x;
        // this is dicey because apparently its volatile, but for the sake of learning, yeah -- normally you gotta separate it instead of callign __getSampler() as you cant naturally get it.

        // If the sampled image's depth value (from the light's point of view) isn't the same as the camera's depth value, then it is obscured and thus is in shadow.
        if ( distanceDepth < pixelCoordinate.z && pixelCoordinate.w > 0.0 )
        {
            illuminated = shadowDarkness;
        }
    }

    return illuminated;
}


// Percentage-Closer Filtring (PCF) is like antialiasing but for shadows, where it sees the neighbouring depth values to compute a pixel's final depth value
// This is useful because otherwise, similarily with antialiasing, the absence of considering neighbouring pixels makes the shadow's edges VERY sharp -- it goes from shadow to full light, making the shadow blocky.
// https://developer.nvidia.com/gpugems/gpugems/part-ii-lighting-and-shadows/chapter-11-shadow-map-antialiasing -- see Figure 11.6-8
const static bool enable_PCF = false;
float computeShadow_PCF( float4 pixelCoordinate )
{
    uint imageWidth;
    uint imageHeight;

    depthSampledImage.GetDimensions(imageWidth, imageHeight); // Just as a headsup, slang passes this by pointer technically -- it's the same as pass by reference since these 2 params are marked with 'out'
    // depthSampler.GetDimensions(imageWidth, imageHeight); // equivalent to textureSize() in gl's -- also equivalent to the line above.

    // We do this to get the scale of a SINGLE pixel.
    // We use scale because we want to scale up our pixel to be slightly bigger (50% larger), which makes the shadow neater looking
        // Test out how it looks without the scale!
    float scale = 1.5;
    float dimensionsX = (scale * 1.0) / float(imageWidth); // Dimensions of the pixels.
    float dimensionsY = (scale * 1.0) / float(imageHeight);

    float shadowAverage = 0.0;
    int count = 0;
    int range = 1;
    // The idea here is that we are testing out neighbouring pixels in a 3X3 range -- range is 1 because it is our "end point" of when to stop iterating.
    // X axis: -1 is the left, 1 is the right; Y axis: -1 is the bottom, 1 is the top -- 0 is the middle in both.

    for (int x = -range; x <= range; x++)
    {
        for (int y = -range; y <= range; y++)
        {
            shadowAverage += textureProjection(pixelCoordinate, float2(dimensionsX * x, dimensionsY * y));
                // We give it an offset because, again, we're trying to blur the pixel to give a shadow like feel -- whenever it's finished, play around with it.
            count++; // Count will always certainly be 9.
        }
    }

    return (shadowAverage / float(count));
}

[shader("fragment")]
float4 fragMain(VSInput vertIn) : SV_TARGET
{
    // Final shadow value
    // If we want to use PCF to compute it, run the PCF function. Otherwise, default it with texture projection.
    float shadow;
    if (enable_PCF)
    {
        shadow = computeShadow_PCF(vertIn.inShadowCoord / vertIn.inShadowCoord.w);
    }
    else
    {
        shadow = textureProjection(vertIn.inShadowCoord / vertIn.inShadowCoord.w, float2(0.0)); // Standard projection doesn't do the offset because we don't want the blur -- only relevant with PCF due to averages -- play around with it, though!
    }

    float3 N = normalize(vertIn.inNormal);
    float3 L = normalize(vertIn.inLightVec);
    float3 V = normalize(vertIn.inViewVec);
    float3 R = normalize(-reflect(L, N));

    float3 diffuse = mul(max(dot(N, L), shadowDarkness), vertIn.inColor); // Computes the blend of shadow to colour -- the pixel has notes of its original colour and black combined together -- play around with it later.

    return float4( (diffuse * shadow), 1.0 );
}