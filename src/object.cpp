#include "includes.hpp"

struct ModelObject
{
    // Object data
        // So to represent the object and all its vertices, we make a central "point", which defines where its position is, its rotation, and its scale (as a whole)
    glm::vec3 position = {0.0f, 0.0f, 0.0f};
    glm::vec3 rotation = {0.0f, 0.0f, 0.0f};
    glm::vec3 scale = {1.0f, 1.0f, 1.0f};

    // Uniform buffer for this object (one per frame in flight)
        // the uniform buffers have the general data of the object RIGHT above (so its accessible within the GPU)
    std::vector<vk::raii::Buffer> uniformBuffers;
    std::vector<vk::raii::DeviceMemory> uniformBuffersMemory;
    std::vector<void*> uniformBuffersMapped;

    // Descriptor sets for this object (one per frame in flight)
        // and we need the descriptor set to actually communicate with the uniform buffers in the GPU
    std::vector<vk::raii::DescriptorSet> descriptorSets;

    // Calculate model matrix based on position, rotation, and scale
        // this struct members: we get a matrix to apply this matrix to all vertices to modify our individual vertices.
    glm::mat4 getModelMatrix() const {
        glm::mat4 model = glm::mat4(1.0f);
        model = glm::translate(model, position);
        model = glm::rotate(model, rotation.x, glm::vec3(1.0f, 0.0f, 0.0f));
        model = glm::rotate(model, rotation.y, glm::vec3(0.0f, 1.0f, 0.0f));
        model = glm::rotate(model, rotation.z, glm::vec3(0.0f, 0.0f, 1.0f));
        model = glm::scale(model, scale);
        return model;
    }
};

// So, our model object just encapsulates this:
    // 1. The object’s transform (position, rotation, scale)
    // 2. Per-object uniform buffers (one for each frame in flight)
    // 3. Per-object descriptor sets (one for each frame in flight)
    // 4. A helper method to calculate the model matrix
