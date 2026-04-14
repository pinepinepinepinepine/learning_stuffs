
// Vulkan uses something resembling pointer arithmetics (formally, byte 'offsets' to find the next member) to find the next data member in a struct.
// If we break this balance, it causes an error as expected because the pointer arithmetics isn't lining up, so we throw it off entirely.
// It's why if we add float2 foo; to this (and the its matching struct in the C++ code), our rectangle doesn't pop up -- it can't read the model/view/proj fields properly as its reading the wrong bytes!
struct UniformBuffer {
    float2 foo;
    float4x4 model;
    float4x4 view;
    float4x4 proj;
};
ConstantBuffer<UniformBuffer> ubo;
// the mental model here is that: at this byte offset, read the member's size. the problem is "at this byte offset".
// hypothetically, if the next member to read is of size 16, it's offset (beginning byte) is expected to be at byte 16.
// HOWEVER, due to float2 foo being 8 bytes (stored at 0-8 bytes), the CPU stores the model member at offset 8, not 16.
// Then, the GPU will be beginning reading from offset byte 16 (so the GPU reads 16-32bytes, 16 bytes) where we expect it to read model, but model is at 8-24, hence error.
// The answer to this is to align it within the CPU: in our case, force the model member to begin/be divisible/be aligned by 16, by storing it at offset 16, hence alignas(16)
// It's also good practice to explicitly alignas(size) every member even if we're not mixing -- i'm opting to just leave foo present because I like the mental model.

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
    float2 inTextureCoords; // for our presenting of a texture image
};

struct VertexOutput {
    float3 color; // float3, in this context, isn't xyz position, it's just RGB.
    float4 sv_position : SV_Position; // 'SV_Position' (not the member variable) tells the GPU this sv_position variable is for the vertex's position.
    float2 fragmentTextureCoords;
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

    // RELATING TO ALIGNMENT: while we are calling ubo.proj, ubo.proj is referring to a BYTE OFFSET from our UNIFORM BUFFER -- it's essentially pointing to a region of 0 to 64bytes (alignment number) for a single field, like .proj.
    // we are not LITERALLY calling ubo.proj, ubo.proj is the calculated alignenment to reach that value:
    // think of it like this: UBO. is the the base of addressing (SPECIFIED IN THE DESCRIPTOR'S/RESOURCE'S BINDING: In our case, BY WHAT SPECIFIC UNIFORM BUFFER TO READ FROM),
    // and .PROJ is the BYTE OFFSET (64 bytes in our case). Then, the GPU reads the type size of .proj from the starting position of ubo.proj to find how many to bytes to read.
    // If we add a float2 for instance (anything to break the arithmetics), it expects to read offsets of 16* (if we've been using float4s), so the +8 in the mix throws it off:
    // with float2: 0 -> 8 -> 24; with float4; 0 -> 16 -> 32. notice how the float2 version, if using an offset/alignment of 16, reads the wrong byte by reading into the next data
    // this isn't literally how vulkan does it (it uses offsets to dictate the starting position) instead of just using the previous, but it's just a rough idea.
    // *Technically, it reads by 16byes, float4x4 is just 4 float4s, where a float4 is 16 bytes, hence 16x4 = 64, so 64 bytes from the offset is one float4x4, aka our one data member.
    // https://docs.vulkan.org/spec/latest/chapters/interfaces.html#interfaces-resources-layout

    output.color = input.inColor;

    output.fragmentTextureCoords = input.inTextureCoords;

    return output;
}

Sampler2D texture; // A sampler represents a combined image sampler descriptor in Slang -- there's also an equivalent sampler1D and sampler3D for other types of images.
// This function is called upon for EVERY fragment (similarly to how vertMain is called upon for EVERY vertex)
[shader("fragment")] // Identifies the function as a Fragment Shader; if left out, the GPU won't know what function to call upon to act as the Fragment Shader.
float4 fragMain(VertexOutput inVert) : SV_Target // SV_Target specifies the index of the specific framebuffer we're outputting/displaying to.
{
    // float3 color = inVert.color; // GPU automatically sets inVert.color, hence why we randomly have a gradient: https://community.khronos.org/t/how-does-one-interpolate-opengl-colors-yourself/75403 (specify nointerpolation for the struct here if we dont want)
    // return float4(color, 1.0); // in this context, float4 (as opposed to output.sv_position=float4) is the base RGB colors (color as the first param) and the alpha channel (transparency) as 'w' (range: 1.0 = opaque, 0.0 = fully transparent)

    // this fragment's colour is equal to the texel's colour at fragmentTextureCoords
    return texture.Sample(inVert.fragmentTextureCoords);

    // texture colours are manipulated using the vertex colors
    //return float4( inVert.color * texture.Sample(inVert.fragmentTextureCoords).rgb, 1.0f);
}