#include "../headers/vertex.hpp"

vk::VertexInputBindingDescription Vertex::getBindingDescription()
{
    return { .binding = 0, .stride = sizeof(Vertex), .inputRate = vk::VertexInputRate::eVertex };
}

std::array<vk::VertexInputAttributeDescription, 1> Vertex::getAttributeDescriptions()
{
    vk::VertexInputAttributeDescription position_description { .location = 0, .binding = 0, .format = vk::Format::eR32G32B32Sfloat, .offset = offsetof( Vertex, base.pos ) };

    return std::array<vk::VertexInputAttributeDescription, 1>{ position_description};
}


vk::VertexInputBindingDescription TextureVertex::getBindingDescription()
{
    return { .binding = 0, .stride = sizeof(TextureVertex), .inputRate = vk::VertexInputRate::eVertex };
}

std::array<vk::VertexInputAttributeDescription, 3> TextureVertex::getAttributeDescriptions()
{
    vk::VertexInputAttributeDescription position_description        { .location = 0, .binding = 0, .format = vk::Format::eR32G32B32Sfloat, .offset = offsetof( TextureVertex, base.pos ) };
    vk::VertexInputAttributeDescription color_description           { .location = 1, .binding = 0, .format = vk::Format::eR32G32B32Sfloat, .offset = offsetof( TextureVertex, color ) };
    vk::VertexInputAttributeDescription texture_coords_description  { .location = 2, .binding = 0, .format = vk::Format::eR32G32Sfloat, .offset = offsetof( TextureVertex, textureCoords ) };

    return std::array<vk::VertexInputAttributeDescription, 3>{ position_description, color_description, texture_coords_description };
}