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


    // we need to specify how to compare vertex objects to one another for our unordered_map's usage in loadModel():
        // If the vertex is within the map, reuse the initial index (don't create a unique entry for the vertex);
        // If the vertex is not within the map, create a new index by adding +1 to the highest index (and do create a unique entry for the vertex).
    bool operator==( const Vertex& other ) const {
        return pos == other.pos && color == other.color && textureCoords == other.textureCoords;
    }
};



// WE ARE NOW LOADING A MODEL (WE ARE NOT HARD-CODING VERTICES, IT'S GIVEN FROM THE MODEL'S .OBJ FILE)
    // see BIG_NOTES for the old hard-coded vertex/index data.
std::vector<Vertex> vertices;
std::vector<uint32_t> indices;

// see loadmodel() for some additional information in main.cpp, and
// we're template specializing the std::hash struct (it's a struct, not REALLY a function -- its referred to as a 'functor') to 'accept' a vertex.
// c++ allows template specializations of std::hash only within the std namespace, hence namespace std{}
// if you're interested, check out hash functions, as they're apparently another huge can of worms.
namespace std
{
    // how we specialize hash -- setting it to be compatible with a Vertex object.
    template<> struct hash<Vertex>
    {
        // we're overloading the operator() to accept a vertex with hash
        size_t operator()(Vertex const& vertex) const
        {
            // these are 'struct function objects' (referred to as functors);
            // in c++, std::hash is a callable object -- you use these like functions onto whatever corresponding type w/ operator()
            hash<glm::vec3> vec3_hash;
            hash<glm::vec2> vec2_hash;

            // get 3 separate hashes for each member -- we shift it lightly before combining.
            size_t vertex_position_hash = vec3_hash( vertex.pos );
            size_t vertex_color_hash = vec3_hash( vertex.color ) << 1;
            size_t vertex_textureCoords_hash = vec2_hash( vertex.textureCoords ) << 1;

            // combine the three hashes into a central, vertex hash.
            size_t vertex_hash = ( ( vertex_position_hash ^ vertex_color_hash ) >> 1 ) ^ ( vertex_textureCoords_hash );

            // hashes are just long integers, and XOR only operates on binary values.
            // So, XOR'll converts these hashes to binary to then operator^ (XOR)'s return:
            // XOR's return is simple:
                // if the specific bit at the same location between the two binary data are equal to one another: set the output bit to 0.
                // if the specific bit at the same location between the two binary data are not equal to one another: set the output bit to 1.
            // Check the comment on operator<< within main.cpp: all it does is just shift the bits by 1 (which means it won't be the same hash)
            // the reason we're using << and >> is literally just to prevent hash collisions. It's unbelievably unlikely, but we'd like to be safe.
                // research more about mixing functions w/ hash to find out more, but yeah -- it's just VERY unlikely, but not zero.
            // the idea with this entire function though is to return a unique hash for a vertex, and the way we do such a thing is by combining the different member's hashes to get a final hash that we'll use.

            return vertex_hash;

            // From the tutorial site: ( ( hash<glm::vec3>()(vertex.pos) ^ (hash<glm::vec3>()(vertex.color) << 1) ) >> 1 ) ^ ( hash<glm::vec2>()(vertex.textureCoords) << 1 );
            // but the expanded version is just more clear.
        }
    };
}