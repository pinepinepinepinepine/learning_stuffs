#include "includes.hpp"


// transform matrices housed inside a uniform buffer -- see BIG_NOTES for a bit of an elaboration
// see shader.slang for some additional comments relating to 'alignment requirements'
struct UniformBufferObject {
    glm::vec2 foo;
    alignas(16) glm::mat4 model; // this member's address must be divisible by 16 -- this is due to alignment (it'll add padding between foo's size and 16 to force it to an alignment of 16.)
    glm::mat4 view;
    glm::mat4 proj;
};

struct Vertex
{
    glm::vec3 pos; // position of the vertex (duh) -- now 3D w/ the addition of depth buffering
    glm::vec3 color; // colour of the vertex (duh)

    // Often called "uv coordinates", this is the actual texture coordinates for each vertex: the texture coordinates determine how the texture is actually mapped to geometry
    glm::vec2 textureCoords;

    // We need to tell Vulkan how to pass this vertex's data to the vertex shader once it's uploaded into GPU memory. We need two structs in total to do this.

    // The first struct, vk::VertexInputBindingDescription, (Binding Descriptions) describes how to load data from memory (pointer arithmetic) throughout the vertices' container.
        // it's called Binding because of it iss how each data point is "bound" together, hence why it specifies .stride for type size for pointer arithmetics, and .inputRate for what data-to-data is (vertex-container or per-vertex)
    static vk::VertexInputBindingDescription getBindingDescription()
    {
        // vulkan finds each data (Vertex or instance/container) by adding the size of a contained (within a vector/array) Vertex (or the container itself if eInstance) to its address find the next element (WITHIN MY EXAMPLE ALONE, this is referred to as pNext chain) (pointer arithmetic)
        // First parameter, .binding, is responsible for where we start our vertex chain from (essentially the first index in the "pNext chain")
        // Second paramater, .stride, gives the size of our Vertex aggregate so we perform pointer arithmetic on an array/vector of type Vertex, allowing us to begin performing the "pNext chain" (put sizeof instance/container if eInstance on third param)
        // Third parameter, .inputRate, specifies how the "pNext chain" behaves (where pNext is, either after Vertex or container)
            // eVertex: move (point) to the next data entry after every Vertex
            // eInstance: move (point) to the next data entry after every instance (array/vector/container) -- this is for instanced rendering.
        return { .binding = 0, .stride = sizeof(Vertex), .inputRate = vk::VertexInputRate::eVertex };
    }

    // The second struct, vk::VertexInputAttributeDescription, (Attribute Descriptions) describes how to extract a vertex's attributes from a chunk of vertex data that comes from the binding description above.
    // All it does is allow the GPU to process the colour and position of vertices -- otherwise, what is a vertex's position and color to the GPU? It's an array of size 2 because we've 2 attributes, so each element in the array corresponds to a different attribute.
    static std::array<vk::VertexInputAttributeDescription, 3> getAttributeDescriptions()
    {
        // .location is where to send this attribute in the vertex shader -- ensure each attribute has a unique value as otherwise Vulkan won't be able to know where to send each attribute to
        // .binding tells Vulkan where we start our attribute chain (if it's 1, we're skipping the element at 0)
        // .format is akin to float<x> in slang -- it's a little misleading, but it isn't solely used for colour formats.
            // vk::Format::eR32G32Sfloat is 32bit with (R, G), so it's a 2-component vector that could be used for, say, X and Y positions, or red and green color.
            // format also implicitly defines the byte size of the attribute data to allow for pointer arithmetics, or 'chaining'.
        // .offset specifies the start of the per-vertex data to read from
            // CURRENTLY, vertex data is just a combination of two fields in one byte format, pos and color, WITHOUT .offset, we won't know WHERE color or position begins inside vertex data
                // image we've XXXXXXXYYY: we want to specify that Y begins at 7 so we only get Y without traces of X
                // Vertex in raw memory is 2 byte-formatted attributes, in the example above, X is pos, and Y is color.
                // The way it works is that pos is 8 bytes, and color is 12 bytes, and vertex combines both of these to be 20 bytes -- just exclude the first 8 bytes if we are trying to grab color.
                // remember .format specifies the size as well, which means it knows how many bytes to read from the offset.
            // offsetof() just returns where a member begins inside an aggregate -- finds where member variable (2nd param) begins inside a struct (1st param) in bytes.

        vk::VertexInputAttributeDescription position_description    { .location = 0, .binding = 0, .format = vk::Format::eR32G32B32Sfloat, .offset = offsetof( Vertex, pos ) }; // with the addition of depth buffering, change the format to include a Z axis (remember the colour thing)
        vk::VertexInputAttributeDescription color_description       { .location = 1, .binding = 0, .format = vk::Format::eR32G32B32Sfloat, .offset = offsetof( Vertex, color ) };

        // huzzah! later added due to image sampling and projecting textures onto our swap chain images (our rectangle).
        // same logic as the previous ones for its parameters, we're just passing the texture coordinates so that our shader can actually use it.
        vk::VertexInputAttributeDescription texture_coords_description { .location = 2, .binding = 0, .format = vk::Format::eR32G32Sfloat, .offset = offsetof( Vertex, textureCoords ) };

        return { position_description, color_description, texture_coords_description };
    }

};

// Previously we just hard-coded the vertices' positions within shader.slang, but now we're combining vertices into a single vector.
// This is called interleaving vertex attributes.
const std::vector<Vertex> vertices {
    { { -0.5f, -0.5f, 0.0f }, rgb_float( { 134, 181, 242 } ), { 1.0f, 0.0f } },
    { {  0.5f, -0.5f, 0.0f }, rgb_float( { 79,  76,  237 } ), { 0.0f, 0.0f } },
    { {  0.5f,  0.5f, 0.0f }, rgb_float( { 166, 127, 245 } ), { 0.0f, 1.0f } },
    { { -0.5f,  0.5f, 0.0f }, rgb_float( { 124, 88,  196 } ), { 1.0f, 1.0f } },


    // annoying to format these nicely
    {{-0.5f, -0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},
    {{0.5f, -0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f}},
    {{0.5f, 0.5f, -0.5f}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f}},
    {{-0.5f, 0.5f, -0.5f}, {1.0f, 1.0f, 1.0f}, {0.0f, 1.0f}},

    {{-0.5f, -0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}, {0.0f, 1.0f}}, // 8
    {{0.5f, -0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}, {1.0f, 1.0f}}, // 9
    {{ -0.5f, -0.5f, 0.0f }, {0.0f, 0.0f, 1.0f}, {0.0f, 0.0f}}, // 10
    {{  0.5f, -0.5f, 0.0f }, {1.0f, 1.0f, 1.0f}, {1.0f, 0.0f}}, // 11

    {{0.5f, 0.5f, -0.5f}, {0.0f, 0.0f, 1.0f}, {0.0f, 1.0f}}, // 12
    {{-0.5f, 0.5f, -0.5f}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f}}, // 13
    {{0.5f, 0.5f, 0.0f }, {0.0f, 0.0f, 1.0f}, {0.0f, 0.0f}}, // 14
    {{-0.5f,  0.5f, 0.0f }, {0.0f, 0.0f, 1.0f}, {1.0f, 0.0f}}, // 15
};
// for the third member, textureCoords, notice how it's from a range of 0-1. This is because of .unnormalizedCoordinates = vk::False.
    // 1.0fx, 1.0fy is bottom right corner, where 0.0x, 0.0y is the top left corner.
    // Coordinates below 0, or above 1, results in seeing the addressing modes in action due to trying to read a texel out of the image's extent

// represents the index buffer's indices -- the specified indices'll be used to make a rectangle
const std::vector<uint16_t> indices = {
    // also, the GPU will make triangles (or whatever topology we specified) in sequential order, so:
    // the first 3 vertices make the first triangle; then the last 3 make the second triangle
    // (so it draws index 0 -> 1 -> 2 -> triangle one -> 2 -> 3 -> 0 -> triangle two)
    0, 1, 2, 2, 3, 0, // top of the cube
    4, 5, 7, 7, 5, 6, // beneath the cube

    8, 9, 10, 10, 9, 11,
    12, 13, 14, 14, 13, 15,
    14, 11, 12, 12, 11, 9,
    10, 15, 8, 8, 15, 13
};