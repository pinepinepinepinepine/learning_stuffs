#pragma once

#include "includes.hpp"

// Inheritance is weird: so, bit of a weird hack. but whatever. idea is every vertex has to have these basic attributes. yeah its kinda like taped-up inheritance.
    // Honestly: it's probably a better idea to just... not wrap it.
struct BaseVertexAttributes
{
    glm::vec3 pos;

    BaseVertexAttributes() = default;
    BaseVertexAttributes( glm::vec3 position ) : pos(position) {}

    bool operator==( const BaseVertexAttributes& other ) const
    { return this->pos == other.pos; }
};

struct Vertex
{
    BaseVertexAttributes base;

    Vertex() = default;
    Vertex( glm::vec3 position ) : base( position ) {}

    static vk::VertexInputBindingDescription getBindingDescription();
    static std::array<vk::VertexInputAttributeDescription, 1> getAttributeDescriptions();
};

struct TextureVertex
{
    BaseVertexAttributes base;
    glm::vec3 color;
    glm::vec2 textureCoords;

    TextureVertex() = default;

    static vk::VertexInputBindingDescription getBindingDescription();
    static std::array<vk::VertexInputAttributeDescription, 3> getAttributeDescriptions();

    bool operator==( const TextureVertex& other ) const
    { return base == other.base && color == other.color && textureCoords == other.textureCoords; }
};

struct ShadowVertex
{
    BaseVertexAttributes base;
    glm::vec3 color;
    glm::vec3 normal; // This is how shadows are created.


    ShadowVertex() = default;

    static vk::VertexInputBindingDescription getBindingDescription();
    static std::array<vk::VertexInputAttributeDescription, 3> getAttributeDescriptions();
};

// god i hate the lack of inheritance
namespace std
{
    template<> struct hash<TextureVertex>
    {
        size_t operator()( TextureVertex const& vertex ) const
        {
            hash<glm::vec3> vec3_hash;
            hash<glm::vec2> vec2_hash;

            size_t vertex_position_hash = vec3_hash( vertex.base.pos );
            size_t vertex_color_hash = vec3_hash( vertex.color ) << 1;
            size_t vertex_textureCoords_hash = vec2_hash( vertex.textureCoords ) << 1;
            size_t vertex_hash = ( ( vertex_position_hash ^ vertex_color_hash ) >> 1 ) ^ ( vertex_textureCoords_hash );
            return vertex_hash;
        }
    };
}