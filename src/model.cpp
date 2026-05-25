#include "../headers/model.hpp"

void ModelData::loadModel( const LogicalDevice& device, const char *filename )
{
    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;
    std::string err;

    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;

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
}
