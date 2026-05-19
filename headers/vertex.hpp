#pragma once

#include "includes.hpp"

struct Vertex
{
    glm::vec3 pos;
    glm::vec3 color;
    glm::vec2 textureCoords;

    static vk::VertexInputBindingDescription getBindingDescription();
    static std::array<vk::VertexInputAttributeDescription, 3> getAttributeDescriptions();

    bool operator==( const Vertex& other ) const
    { return pos == other.pos && color == other.color && textureCoords == other.textureCoords; }
};

namespace std
{
    template<> struct hash<Vertex>
    {
        size_t operator()( Vertex const& vertex ) const
        {
            hash<glm::vec3> vec3_hash;
            hash<glm::vec2> vec2_hash;

            size_t vertex_position_hash = vec3_hash( vertex.pos );
            size_t vertex_color_hash = vec3_hash( vertex.color ) << 1;
            size_t vertex_textureCoords_hash = vec2_hash( vertex.textureCoords ) << 1;
            size_t vertex_hash = ( ( vertex_position_hash ^ vertex_color_hash ) >> 1 ) ^ ( vertex_textureCoords_hash );
            return vertex_hash;
        }
    };
}