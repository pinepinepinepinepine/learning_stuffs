#include "components.hpp"

// Transform component
// Handles the position, rotation, and scale of an entity in 3D space
// AffineTransform or "Pose" matrix.
class TransformComponent : public Component
{
    glm::vec3 position = glm::vec3(0.0f);
    glm::quat rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f); // Identity quaternion
    glm::vec3 scale = glm::vec3(1.0f);

    // Cached transformation matrix
    mutable glm::mat4 transformMatrix = glm::mat4(1.0f);
    mutable bool transformDirty = true; // transformDirty is kind of like a listener in the sense that when the state changes, we must recalculate the transformation matrix: only recalculate the matrix when the state changes.

    public:
    const glm::vec3& GetPosition() const { return position; }
    const glm::quat& GetRotation() const { return rotation; }
    const glm::vec3& GetScale() const { return scale; }

    void SetPosition(const glm::vec3& pos);
    void SetRotation(const glm::quat& rot);
    void SetScale(const glm::vec3& s);
    glm::mat4 GetTransformMatrix() const;
};