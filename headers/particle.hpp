#pragma once

#include "includes.hpp"

constexpr uint32_t PARTICLE_COUNT = 8192;

struct ParticleTime
{
	float deltaTime = 1.0f;
};

struct Particle
{
    glm::vec2 position;
	glm::vec2 velocity;
	glm::vec4 color;

    static vk::VertexInputBindingDescription getBindingDescription();
    static std::array<vk::VertexInputAttributeDescription, 2> getAttributeDescriptions();
};