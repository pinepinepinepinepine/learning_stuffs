#include "../../headers/model.hpp"
#include "../../headers/texture.hpp"

class Entity;

// Base component class
class Component
{
    protected:
        Entity* owner = nullptr;

    public:
        void SetOwner(Entity* entity) { owner = entity; }
        Entity* GetOwner() const { return owner; }
};

// raw model data: the texture and the mesh (vertex and index buffers)
class ModelComponent : public Component
{
    ModelData* model;
    Texture* texture;

    public:
    // We need to create a constructor because our AddComponents creates a component object via make_unique which calls upon the constructor based on the args.
    ModelComponent( ModelData* m, Texture* t ) : model(m), texture(t) {}

    void setModel( ModelData* m ) { model = m; }
    void setTexture( Texture* t ) { texture = t; }

    ModelData* getModel() { return model; }
    Texture* getTexture() { return texture; }
};

// How we transform this entity
class TransformComponent : public Component
{
    glm::vec3 position = glm::vec3(0.0f);
    glm::quat rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f); // Identity quaternion
    glm::vec3 scale = glm::vec3(1.0f);

    glm::mat4 transformMatrix = glm::mat4(1.0f);

    public:
    // We need to create a constructor because our AddComponents creates a component object via make_unique which calls upon the constructor based on the args.
    TransformComponent( glm::vec3 pos, glm::quat rot, glm::vec3 s ) : position(pos), rotation(rot), scale(s) {}

    void SetPosition(const glm::vec3& pos) { position = pos; }
    void SetRotation(const glm::quat& rot) { rotation = rot; }
    void SetScale(const glm::vec3& s) { scale = s; }

    const glm::vec3& GetPosition() const { return position; }
    const glm::quat& GetRotation() const { return rotation; }
    const glm::vec3& GetScale() const { return scale; }
    glm::mat4 GetTransformMatrix() const { return transformMatrix; };

    void TransformComponent::calculateTransformMatrix(); // The tutorial has a fancy way with an added bool to check whether this position has been shifted, to then subsequently recalculate it.
};