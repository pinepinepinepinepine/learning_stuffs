#include "../headers/particle.hpp"


vk::VertexInputBindingDescription Particle::getBindingDescription()
{
    return {0, sizeof(Particle), vk::VertexInputRate::eVertex};
}

std::array<vk::VertexInputAttributeDescription, 2> Particle::getAttributeDescriptions()
{
    	return {
		    vk::VertexInputAttributeDescription(0, 0, vk::Format::eR32G32Sfloat, offsetof(Particle, position)),
		    vk::VertexInputAttributeDescription(1, 0, vk::Format::eR32G32B32A32Sfloat, offsetof(Particle, color)) };
}