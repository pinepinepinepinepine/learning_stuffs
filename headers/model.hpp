#pragma once

#include "includes.hpp"
#include "gpuBuffers.hpp"
#include "vertex.hpp"


struct AABB_box
{
    glm::vec3 min;
    glm::vec3 max;

    AABB_box( glm::vec3 _min, glm::vec3 _max ) : min(_min), max(_max) {}

    std::array<glm::vec3, 8> getBoxCorners() const
    {
        std::array<glm::vec3, 8> corners {
            min,
            { max.x, min.y, min.z },
            { min.x, max.y, min.z },
            { max.x, max.y, min.z },
            { min.x, min.y, max.z },
            { min.x, max.y, max.z },
            { max.x, min.y, max.z },
            max
        };
        return corners;
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