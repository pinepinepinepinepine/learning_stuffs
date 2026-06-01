#include "../headers/model.hpp"

AABB_box ModelData::loadModel( const LogicalDevice& device, const char *filename )
{
    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;
    std::string err;

    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;

    float min_x = std::numeric_limits<float>::max();
    float max_x = std::numeric_limits<float>::min();
    float min_y = std::numeric_limits<float>::max();
    float max_y = std::numeric_limits<float>::min();
    float min_z = std::numeric_limits<float>::max();
    float max_z = std::numeric_limits<float>::min();

    bool obj_result = tinyobj::LoadObj( &attrib, &shapes, &materials, &err, filename );
    if ( !obj_result )
        throw std::runtime_error(err);

    std::unordered_map<Vertex, uint32_t> uniqueVertices{};

    for ( const auto& shape : shapes )
    {
        for (const auto& index : shape.mesh.indices )
        {
            Vertex vertex{};

            vertex.pos = {
                attrib.vertices[3 * index.vertex_index + 0],
                attrib.vertices[3 * index.vertex_index + 1],
                attrib.vertices[3 * index.vertex_index + 2]
            };

            vertex.textureCoords = {
                attrib.texcoords[2 * index.texcoord_index + 0],
                attrib.texcoords[2 * index.texcoord_index + 1] };

            vertex.color = {1.0f, 1.0f, 1.0f};

            if ( uniqueVertices.count( vertex ) == 0 )
            {
                uniqueVertices[vertex] = static_cast<uint32_t>( vertices.size() );
                vertices.push_back(vertex);

                if ( vertex.pos.x < min_x ) min_x = vertex.pos.x;
                if ( vertex.pos.x > max_x ) max_x = vertex.pos.x;
                if ( vertex.pos.y < min_y ) min_y = vertex.pos.y;
                if ( vertex.pos.y > max_y ) max_y = vertex.pos.y;
                if ( vertex.pos.z < min_z ) min_z = vertex.pos.z;
                if ( vertex.pos.z > max_z ) max_z = vertex.pos.z;
            }
            indices.push_back( uniqueVertices[vertex] );
        }
    }

    vertexBuffer.createGPUBuffer(
        device,
        sizeof(vertices[0]) * vertices.size(),
        vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eTransferDst,
        vk::MemoryPropertyFlagBits::eDeviceLocal,
        false );
    vertexBuffer.muleBuffer( device, vertices );
    vertices_count = vertices.size();

    indexBuffer.createGPUBuffer(
        device,
        sizeof(indices[0]) * indices.size(),
        vk::BufferUsageFlagBits::eIndexBuffer | vk::BufferUsageFlagBits::eTransferDst,
        vk::MemoryPropertyFlagBits::eDeviceLocal,
        false );
    indexBuffer.muleBuffer( device, indices );
    indices_count = indices.size();

    return AABB_box( glm::vec3(min_x, min_y, min_z), glm::vec3(max_x, max_y, max_z) );
}
