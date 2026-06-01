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

class BoundingComponent : public Component
{
    AABB_box boundingBox;

    public:
    BoundingComponent( AABB_box box ) : boundingBox(box)
    {
        std::cout << "Bounding Box Created: (" << boundingBox.min.x << "x, " << boundingBox.min.y << "y, " << boundingBox.min.z << "z) -> ("
        << boundingBox.max.x << "x, " << boundingBox.max.y << "y, " << boundingBox.max.z << "z)\n";
    }
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


class CameraComponent;


struct Plane
{
    glm::vec3 normal;
    float offset;

    Plane() = default;
    Plane( glm::vec3 p0, glm::vec3 p1, glm::vec3 p2 )
    {
        // A PLANE IS 2 PARTS: FIRST, WE NEED THE PLANE'S NORMAL (FANCY WORD FOR DIRECTION)
        // AND THE SECOND PART IS THE OFFSET: WHICH IS GIVEN BY DOT PRODUCT OF THE PLANE'S NORMAL AND SOME ARBITRARY (doesnt have to be p0/p1/p2) POINT ON THE PLANE.
        normal = glm::normalize(glm::cross(p1-p0, p2-p1)); // SOME arbitrary points of the plane we're creating. IT CAN BE ANYTHING.
        offset = glm::dot(normal, p0);

        std::cout << "Plane Creation: (" << normal.x << ", " << normal.y << ", " << normal.z << ")" << " Offset: " << offset << std::endl;
        // One thing to ensure: check math.txt about culling rules: ensure the p0, p1,and p2 are ALL either COUNTER CLOCKWISE or CLOCKWISE across all
        // this is because for our culling logic, we want to ensure we can use the same logic of "Greater than the plane's offset is culled or rendered"
        // The way to check is visualize if the points from p0 -> p1 -> p2 -> p0 is clockwise/counterclockwise: if it's counterclockwise, it's OUTWARD, so greater than 0 distance is culled.

        // I presume we're gonna have to recalculate the frustum on EVERY rotation/movement of the camera. So... just a heads up when we finish up within runtime.cpp -- DON'T FORGET TO KEEP IT CONTINUALLY UPDATED.
    }

};



class Frustum
{
    const CameraComponent* camera;

    std::array<Plane, 6> frustumPlanes;


    public:
    Frustum() = default;
    Frustum( const CameraComponent* cam ) : camera(cam) {}


    void setCamera( const CameraComponent* cam )
    {
        camera = cam;
    }

    void setPlanes( std::array<Plane, 6> planes )
    {
        frustumPlanes = planes;
    }

    std::array<Plane, 6>& getFrustumPlanes()
    { return frustumPlanes; }

    // https://stackoverflow.com/questions/13665932/calculating-the-viewing-frustum-in-a-3d-space
    void createFrustum();
};


class CameraComponent : public Component //, public EventListener
{
    Frustum camFrustum;

    public: // just i cba. fix this. solely for frustum's stuff instead of getters. fix it later.
    float fieldOfView = 90.0f;
    float aspectRatio = 16.0 / 9.0f;
    float nearPlane = 0.1f;
    float farPlane = 1000.0f;
    // might be a better idea to move these members into frustum class?

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

    CameraComponent() { camFrustum.setCamera(this); }

    Frustum& createCameraFrustum()
    {
        camFrustum.createFrustum();
        return camFrustum;
    }

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

            glm::vec3 forward = rotation * glm::vec3( 0.0f, 0.0f, -1.0f ); // Forward vector ( facing -Z ) -- apparently standard in engines
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



inline void Frustum::createFrustum()
{
    auto thisEntitysTransformComponent = camera->GetOwner()->GetComponent<TransformComponent>();

    if ( !thisEntitysTransformComponent )
        throw std::runtime_error("Could not create a frustum for the camera because the camera's entity does not have a transform component.");

    glm::vec3 position = thisEntitysTransformComponent->GetPosition();
    glm::quat rotation = thisEntitysTransformComponent->GetRotation();

    // If there's problems, check this out.
    glm::vec3 forward = rotation * glm::vec3( 0.0f, 0.0f, -1.0f );
    glm::vec3 upwards = rotation * glm::vec3( 0.0f, 1.0f, 0.0f );
    glm::vec3 right = rotation * glm::vec3( 1.0f, 0.0f, 0.0f );

    glm::vec3 nearCenter = position - forward * camera->nearPlane;
    glm::vec3 farCenter = position - forward * camera->farPlane;

    float nearHeight = 2 * tan( glm::radians(camera->fieldOfView) / 2) * camera->nearPlane;
    float farHeight = 2 * tan( glm::radians(camera->fieldOfView) / 2) * camera->farPlane;
    float nearWidth = nearHeight * camera->aspectRatio;
    float farWidth = farHeight * camera->aspectRatio;

    glm::vec3 farTopLeft      = farCenter + upwards * (farHeight*0.5f) - right * (farWidth*0.5f);
    glm::vec3 farTopRight     = farCenter + upwards * (farHeight*0.5f) + right * (farWidth*0.5f);
    glm::vec3 farBottomLeft   = farCenter - upwards * (farHeight*0.5f) - right * (farWidth*0.5f);
    glm::vec3 farBottomRight  = farCenter - upwards * (farHeight*0.5f) + right * (farWidth*0.5f);

    glm::vec3 nearTopLeft     = nearCenter + upwards * (nearHeight*0.5f) - right * (nearWidth*0.5f);
    glm::vec3 nearTopRight    = nearCenter + upwards * (nearHeight*0.5f) + right * (nearWidth*0.5f);
    glm::vec3 nearBottomLeft  = nearCenter - upwards * (nearHeight*0.5f) - right * (nearWidth*0.5f);
    glm::vec3 nearBottomRight = nearCenter - upwards * (nearHeight*0.5f) + right * (nearWidth*0.5f);

    setPlanes( {
        Plane( nearBottomLeft, farBottomLeft, farTopLeft ), // following 4 planes are walls
        Plane( nearTopLeft, farTopLeft, farTopRight ),
        Plane( nearTopRight, farTopRight, farBottomRight ),
        Plane( nearBottomRight, farBottomRight, farBottomLeft ),
        Plane( nearBottomRight, nearBottomLeft, nearTopLeft ), // near plane
        Plane( farBottomLeft, farBottomRight, farTopLeft ), // far plane
    } );
}