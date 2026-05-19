#pragma once

#include "includes.hpp"
#include "image.hpp"
#include "gpuBuffers.hpp"

struct Texture
{
    Image textureImage;
    uint32_t mipLevels;
    vk::raii::Sampler* textureSampler = nullptr;

    void setTextureSampler( vk::raii::Sampler& sampler );
    void createTextureImage( const LogicalDevice& device, const char *filename );
};