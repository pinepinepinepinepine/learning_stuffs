#include "../headers/components/transformComponent.hpp"

void TransformComponent::SetPosition(const glm::vec3& pos)
{
    position = pos;
    transformDirty = true;
}

void TransformComponent::SetRotation(const glm::quat& rot)
{
    rotation = rot;
    transformDirty = true;
}

void TransformComponent::SetScale(const glm::vec3& s)
{
    scale = s;
    transformDirty = true;
}

glm::mat4 TransformComponent::GetTransformMatrix() const
{
    if ( transformDirty )
    {
        // Calculate transformation matrix
        glm::mat4 translationMatrix = glm::translate(glm::mat4(1.0f), position);
        glm::mat4 rotationMatrix = glm::mat4_cast(rotation);
        glm::mat4 scaleMatrix = glm::scale(glm::mat4(1.0f), scale);

        transformMatrix = translationMatrix * rotationMatrix * scaleMatrix;
        transformDirty = false;
    }
    return transformMatrix;
}