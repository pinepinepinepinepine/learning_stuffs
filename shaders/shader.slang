// a 1D vector is just a point on a line (only one dimension, X or Y)
// a 2D vector has X and Y axis positions
// a 3D vector has X and Y axis positions, as well as Z (Depth)
// a 4D vector has X and Y axis positions, as well as Z (Depth), but also W for "HOMOGENOUS/PERSPECTIVE TRANSFORMATIONS"

// We're just hardcoding available vertex positions.
static float2 positions[3] = float2[](
    float2(0.0, -0.5),
    float2(0.5, 0.5),
    float2(-0.5, 0.5)
);

// We're just hardcoding available vertex colors. (RGB)
static float3 colors[3] = float3[](
    float3(1.0, 0.0, 0.0),  // red
    float3(0.0, 1.0, 0.0),  // green
    float3(0.0, 0.0, 1.0)   // blue
);

struct VertexOutput {
    float3 color; // float3, in this context, isn't xyz position, it's just RGB.
    float4 sv_position : SV_Position; // 'SV_Position' (not the member variable) tells the GPU this is sv_position variable is for the vertex's position.
};

// This function is called upon for EVERY vertex. Calculates the position and per-vector data, like color.
    // Slang supports multiple entry points in one file, hence why we're calling it "vertMain" instead of "main"
[shader("vertex")] // Identifies the function as a Vertex Shader; if left out, the GPU won't know what function to call upon to act as the Vertex Shader.
VertexOutput vertMain(uint vid: SV_VertexID)
// SV_VertexID is built in: it contains the index of the current vertex, which is usually an index into the original vertex buffer.
// For our case, since we're just hard-coding the vertices directly above, it's just the vertex's index within the array of vertex data ( positions[vid] ).
// This function isn't called by us, it's called by the GPU, and the SV_VertexID tells the GPU to "pass the vertex index", giving uint 'vid' value equal to the vertex index.
{
    VertexOutput output;
    output.sv_position = float4(positions[vid], 0.0, 1.0); // chooses the position for this vertex from the positions[] array.
    // float4 means it's a 4-component (4D) vertex. z is depth (we're doing a 2D image, so 0.0),  w is "PERSPECTIVE TRANSFORMATION" (USED FOR PERSPECTIVE CALCULATION)
    // (you know how when you're far away from an object, it's smaller? yeah. that thing. essentially it scales the size accordingly depending on distance to give the same illusion. future problem.)... keep it 1.0.

    output.color = colors[vid]; // just chooses the color for this vertex from the colors[] array.
    return output;
}

// This function is called upon for EVERY fragment (similarly to how vertMain is called upon for EVERY vertex)
[shader("fragment")] // Identifies the function as a Fragment Shader; if left out, the GPU won't know what function to call upon to act as the Fragment Shader.
float4 fragMain(VertexOutput inVert) : SV_Target // SV_Target specifies the index of the specific framebuffer we're outputting/displaying to.
{
    float3 color = inVert.color; // GPU automatically sets inVert.color, hence why we randomly have a gradient: https://community.khronos.org/t/how-does-one-interpolate-opengl-colors-yourself/75403 (specify nointerpolation for the struct here if we dont want)
    return float4(color, 1.0); // in this context, float4 (as opposed to output.sv_position=float4) is the base RGB colors (color as the first param) and the alpha channel (transparency) as 'w' (range: 1.0 = opaque, 0.0 = fully transparent)
}