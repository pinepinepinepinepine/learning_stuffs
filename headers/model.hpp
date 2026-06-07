#pragma once

#include "includes.hpp"
#include "gpuBuffers.hpp"
#include "vertex.hpp"


struct AABB_box
{
    glm::vec3 min;
    glm::vec3 max;

    AABB_box( std::vector<Vertex>& vertices, const glm::mat4& transformation )
    {
        float min_x = std::numeric_limits<float>::max();
        float max_x = std::numeric_limits<float>::lowest();
        float min_y = std::numeric_limits<float>::max();
        float max_y = std::numeric_limits<float>::lowest();
        float min_z = std::numeric_limits<float>::max();
        float max_z = std::numeric_limits<float>::lowest();

        for ( auto& vertex : vertices )
        {
            vertex.base.pos = glm::vec3( transformation * glm::vec4(vertex.base.pos, 1.0f) );

            if ( vertex.base.pos.x < min_x ) min_x = vertex.base.pos.x;
            if ( vertex.base.pos.x > max_x ) max_x = vertex.base.pos.x;
            if ( vertex.base.pos.y < min_y ) min_y = vertex.base.pos.y;
            if ( vertex.base.pos.y > max_y ) max_y = vertex.base.pos.y;
            if ( vertex.base.pos.z < min_z ) min_z = vertex.base.pos.z;
            if ( vertex.base.pos.z > max_z ) max_z = vertex.base.pos.z;
        }

        min = { min_x, min_y, min_z };
        max = { max_x, max_y, max_z };
    }
    AABB_box( glm::vec3 _min, glm::vec3 _max ) : min(_min), max(_max) {}

    std::vector<Vertex> getBoxCorners() const
    {
        std::vector<Vertex> corners {
            { min },
            { { max.x, min.y, min.z } },
            { { max.x, max.y, min.z } },
            { { min.x, max.y, min.z } },
            { { min.x, min.y, max.z } },
            { { max.x, min.y, max.z } },
            {max},
            { { min.x, max.y, max.z } }
        };
        return corners;
    }

    // todo: I HATE HOW THIS IS A VECTOR CAUSE IT'S 36 PERMA.
    // OVERLOAD OR SOMETHING MULEBUFFER TO ACCEPT AN ARRAY.
    static std::vector<uint32_t> getBoxIndices()
    {
        std::vector<uint32_t> indices
        {
            0, 1, 1, 2, 2, 3, 3, 0, 1, 3, // Back
            4, 5, 5, 6, 6, 7, 7, 4, 4, 6, // Front
            0, 4, 1, 5,  3, 4, // Right side
            2, 6, 3, 7, 1, 6, // Left Side
            1, 4, 3, 6 // Floor/Roof crossing lines
        };
        return indices;
    }
};

struct ModelData
{
    GPUBuffer vertexBuffer;
    GPUBuffer indexBuffer;
    uint32_t indices_count;
    uint32_t vertices_count;

    AABB_box loadModel( const LogicalDevice& device, const char *filename );
};