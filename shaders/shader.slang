struct UniformBuffer {
    float4x4 model;
    float4x4 view;
    float4x4 proj;
};
ConstantBuffer<UniformBuffer> ubo;
// We’re going to update the model, view and projection matrices every frame to make the rectangle from the previous chapter spin around in 3D.
// SEE BIG_NOTES_MATH.

// a 1D vector is just a point on a line (only one dimension, X or Y)
// a 2D vector has X and Y axis positions
// a 3D vector has X and Y axis positions, as well as Z (Depth)
// a 4D vector has X and Y axis positions, as well as Z (Depth), but also W for "HOMOGENOUS/PERSPECTIVE TRANSFORMATIONS"
    // float4 means it's a 4-component (4D) vertex. z is depth (we're doing a 2D image, so 0.0),  w is "PERSPECTIVE TRANSFORMATION" (USED FOR PERSPECTIVE CALCULATION)
    // (you know how when you're far away from an object, it's smaller? yeah. that thing. essentially it scales the size accordingly depending on distance to give the same illusion. future problem.)... keep it 1.0.

struct VertexInput {
    float2 inPosition;
    float3 inColor;
};

struct VertexOutput {
    float3 color; // float3, in this context, isn't xyz position, it's just RGB.
    float4 sv_position : SV_Position; // 'SV_Position' (not the member variable) tells the GPU this sv_position variable is for the vertex's position.
};

// This function is called upon for EVERY vertex. Calculates the position and per-vector data, like color.
    // Slang supports multiple entry points in one file, hence why we're calling it "vertMain" instead of "main"
[shader("vertex")] // Identifies the function as a Vertex Shader; if left out, the GPU won't know what function to call upon to act as the Vertex Shader.
VertexOutput vertMain( VertexInput input )
{
    VertexOutput output;

    // SEE BIG_NOTES_MATH FOR A MUCH NEEDED ELABORATION. mul() just multiplies two matrices.
    // this ENTIRE line is JUST: ( projection * ( view * model ) ) -- SEE BIG_NOTES_MATH I BEG
    output.sv_position = mul( ubo.proj, mul( ubo.view, mul( ubo.model, float4(input.inPosition, 0.0, 1.0) ) ) );
    // AFTER it executes, output.sv_position are coords in CLIP SPACE, and W will NOT guaranteed to be 1.0 because we're simulating DEPTH/FoV
    // w will used in perspective projection as the perspective division and is essential for making closer objects look larger than objects that are further away.

    output.color = input.inColor;
    return output;
}

// This function is called upon for EVERY fragment (similarly to how vertMain is called upon for EVERY vertex)
[shader("fragment")] // Identifies the function as a Fragment Shader; if left out, the GPU won't know what function to call upon to act as the Fragment Shader.
float4 fragMain(VertexOutput inVert) : SV_Target // SV_Target specifies the index of the specific framebuffer we're outputting/displaying to.
{
    float3 color = inVert.color; // GPU automatically sets inVert.color, hence why we randomly have a gradient: https://community.khronos.org/t/how-does-one-interpolate-opengl-colors-yourself/75403 (specify nointerpolation for the struct here if we dont want)
    return float4(color, 1.0); // in this context, float4 (as opposed to output.sv_position=float4) is the base RGB colors (color as the first param) and the alpha channel (transparency) as 'w' (range: 1.0 = opaque, 0.0 = fully transparent)
}