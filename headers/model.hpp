#pragma once

#include "includes.hpp"
#include "gpuBuffers.hpp"
#include "vertex.hpp"


struct AABB_box
{
    glm::vec3 min;
    glm::vec3 max;

    AABB_box( glm::vec3 _min, glm::vec3 _max ) : min(_min), max(_max) {}
};

struct ModelData
{
    GPUBuffer vertexBuffer;
    GPUBuffer indexBuffer;
    uint32_t indices_count;
    uint32_t vertices_count;

    AABB_box loadModel( const LogicalDevice& device, const char *filename );
};