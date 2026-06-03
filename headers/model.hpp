#pragma once

#include "includes.hpp"
#include "gpuBuffers.hpp"
#include "vertex.hpp"


struct AABB_box
{
    glm::vec3 min;
    glm::vec3 max;

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