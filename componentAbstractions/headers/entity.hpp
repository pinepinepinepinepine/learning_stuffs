//#include "component.hpp"
#include "../../headers/includes.hpp"
#include "events.hpp"


// An entity is essentially a bundle of components:
// An entity (can) have a model, and whatever other components we give it
// hopefully, we can render entities directly by accessing their underlying components types.
// So, if the entity has a model, we render that in 3D, and if it has a texture, we apply it to that model.

class Component;

struct Entity
{
    std::string nameOfEntity; // name is really just useful to display what entities are present in a scene, kinda like an human viewable index, it's not particularily important for the engine itself.
    bool isEntityActive = false;
    std::vector<std::unique_ptr<Component>> components;

    Descriptor entityDescriptor;

    public:
    explicit Entity(const std::string& entityName ) : nameOfEntity(entityName) {}

    const std::string& GetName() const { return nameOfEntity; }
    bool IsActive() const { return isEntityActive; }
    void SetActive(bool isActive) { isEntityActive = isActive; }

    template<typename T, typename... Args>
    void addComponent( Args&&... args )
    {
        static_assert(std::is_base_of<Component, T>::value, "T must derive from Component");

        auto component = std::make_unique<T>(std::forward<Args>(args)...);
        component->SetOwner(this);

        components.push_back( std::move(component) );
    }

    template<typename T>
    T* GetComponent() {
        for (auto& component : components) {
            if (T* result = dynamic_cast<T*>(component.get())) // If the component is downcastable to <T>, result is not nullptr.
                return result;
        }
        return nullptr;
    }

    template<typename T>
    bool RemoveComponent() {
        for (auto it = components.begin(); it != components.end(); ++it) {
            if (dynamic_cast<T*>(it->get())) {
                components.erase(it);
                return true;
            }
        }
        return false;
    }
};



// Base component class
class Component
{
    protected:
        Entity* owner = nullptr;

    public:
        virtual ~Component() = default; // For down casting (within getComponent), this is necessary: for some reason, we need the base class to have a destructor and it HAS to be virtual.
            // When we get internet, figure out why.

        void SetOwner(Entity* entity) { owner = entity; }
        Entity* GetOwner() const { return owner; }
};

// raw model data: the texture and the mesh (vertex and index buffers)
class ModelComponent : public Component
{
    ModelData* model;


    public:
    // We need to create a constructor because our AddComponents creates a component object via make_unique which calls upon the constructor based on the args.
    ModelComponent( ModelData* m ) : model(m) {}

    void setModel( ModelData* m ) { model = m; }

    ModelData* getModel() { return model; }
};

class TextureComponent : public Component
{
    Texture* texture;

    public:
    // We need to create a constructor because our AddComponents creates a component object via make_unique which calls upon the constructor based on the args.
    TextureComponent( Texture* t ) : texture(t) {}

    void setTexture( Texture* t ) { texture = t; }

    Texture* getTexture() { return texture; }

    vk::DescriptorSetLayoutBinding getDescriptorSetLayoutBinding( uint32_t binding = 1 );
};


// How we transform this entity
class TransformComponent : public Component
{
    glm::vec3 position = glm::vec3(0.0f);
    glm::quat rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f); // Identity quaternion
    glm::vec3 scale = glm::vec3(1.0f);

    mutable glm::mat4 transformMatrix = glm::mat4(1.0f);

    mutable bool transformDirty = true;

    public:
    // We need to create a constructor because our AddComponents creates a component object via make_unique which calls upon the constructor based on the args.
    TransformComponent( glm::vec3 pos, glm::quat rot, glm::vec3 s ) : position(pos), rotation(rot), scale(s) {}

    void SetPosition(const glm::vec3& pos) { position = pos; transformDirty = true; }
    void SetRotation(const glm::quat& rot) { rotation = rot; transformDirty = true; }
    void SetScale(const glm::vec3& s) { scale = s; transformDirty = true; }

    const glm::vec3& GetPosition() const { return position; }
    const glm::quat& GetRotation() const { return rotation; }
    const glm::vec3& GetScale() const { return scale; }

    glm::mat4 GetTransformMatrix() const
    {
        if ( transformDirty )
        {
            glm::mat4 translationMatrix = glm::translate(glm::mat4(1.0f), position);
            glm::mat4 rotationMatrix = glm::mat4_cast(rotation);
            glm::mat4 scaleMatrix = glm::scale(glm::mat4(1.0f), scale);

            transformMatrix = translationMatrix * rotationMatrix * scaleMatrix;
            transformDirty = false;
        }

        return transformMatrix;
    };
};



// Kind of a weird component: but for our MVP matrix (to transform it into world space, we need the view and projection matrices)
// So every entity has this camera component for it to be actually rendered in 3D due to this component populating the VP of the MVP matrix
    // The Model matrix is right above.


class CameraComponent : public Component //, public EventListener
{
    float fieldOfView = 90.0f;
    float aspectRatio = 16.0 / 9.0f;
    float nearPlane = 0.1f;
    float farPlane = 1000.0f;

    glm::mat4 viewMatrix = glm::mat4(11.0f);
    mutable glm::mat4 projectionMatrix = glm::mat4(.0f);
    mutable bool projectionDirty = true; // Whether or not the camera's moved; if true, we recalculate it -- it's the fancy thing the GetTransformMatrix is missing.



    EventSystem eventSystem;



    public:

    // void addListener()
    // {
    //     eventSystem.AddListener(this);
    // }

    // void onEvent( const Event& event )
    // {
    //     if ( auto moveEvent = dynamic_cast<const MoveEvent*>(&event) )
    //     {
    //         Entity* guh = moveEvent->GetEntity();

    //         auto d = guh->GetComponent<TransformComponent>(); // Figure out how event listeners work in more depth. This isn't working. here and the other commented out block.
    //     }
    // }


    void setPerspective( float fov, float aspect, float _near, float _far )
    {
        fieldOfView = fov;
        aspectRatio = aspect;
        nearPlane = _near;
        farPlane = _far;
        projectionDirty = true;
    }

    // The VIEW matrix of the MVP matrix.
    glm::mat4 getViewMatrix() const
    {
        // The tutorial recommends using eventListeners.
        auto thisEntitysTransformComponent = GetOwner()->GetComponent<TransformComponent>(); // Have to define it here because I presume we're dealing with templates.

        if ( thisEntitysTransformComponent ) // If we don't have a transform component attached to this camera component's owner entity, it'd return nullptr due to dynamic_cast failed downcasting
        {
            glm::vec3 position = thisEntitysTransformComponent->GetPosition();
            glm::quat rotation = thisEntitysTransformComponent->GetRotation();

            glm::vec3 forward = rotation * glm::vec3( 0.0f, 0.0f, 1.0f ); // Forward vector ( facing -Z ) -- +Z NOW!
            glm::vec3 up = rotation * glm::vec3( 0.0, 1.0f, 0.0f ); // Up vector ( +Y is up )

            return glm::lookAt( position, position + forward, up );
        }

        throw std::runtime_error("No transform component attached when getting the entity's view matrix");
    }

    // the PROJECTION matrix of the MVP matrix.
    glm::mat4 getProjectionMatrix() const
    {
        if ( projectionDirty ) // Avoids having to recalculate the thing each time if we hadn't modified it -- maybe we do the same thing to viewMatrix (and also transformMatrix -- ditch calculateTransformMatrix, tie it in.)
        {
            projectionMatrix = glm::perspective(
                glm::radians(fieldOfView),
                aspectRatio,
                nearPlane,
                farPlane );
            projectionDirty = false;
        }
        return projectionMatrix;
    }
};
