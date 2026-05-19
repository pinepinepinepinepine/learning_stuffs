#pragma once

#include "includes.hpp"
#include "gpuBuffers.hpp"
#include "vertex.hpp"

struct ModelData
{
    GPUBuffer vertexBuffer;
    GPUBuffer indexBuffer;
    uint32_t indices_count;
    uint32_t vertices_count;

    void loadModel( const LogicalDevice& device, const char *filename );
};