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


class CameraComponent;


class Plane
{
    glm::vec3 normal;
    float offset;
};



class Frustum
{
    const CameraComponent* camera;

    // DITCH THIS: MAKE IT A VECTOR (PROBABLY AN ARRAY CAUSE A FRUSTUM ONLY HAS 6 PLANES) OF PLANES.
    glm::vec3 farTopLeft;
    glm::vec3 farTopRight;
    glm::vec3 farBottomLeft;
    glm::vec3 farBottomRight;

    glm::vec3 nearTopLeft;
    glm::vec3 nearTopRight;
    glm::vec3 nearBottomLeft;
    glm::vec3 nearBottomRight;


    public:
    Frustum() = default;
    Frustum( const CameraComponent* cam ) : camera(cam) {}


    void setCamera( const CameraComponent* cam )
    {
        camera = cam;
    }

    // https://stackoverflow.com/questions/13665932/calculating-the-viewing-frustum-in-a-3d-space
    void getFrustum();
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

    void getCameraFrustum()
    { camFrustum.getFrustum(); }

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



inline void Frustum::getFrustum() // return frustum instead of void when finished.
{
    auto thisEntitysTransformComponent = camera->GetOwner()->GetComponent<TransformComponent>();

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

    farTopLeft      = farCenter + upwards * (farHeight*0.5f) - right * (farWidth*0.5f);
    farTopRight     = farCenter + upwards * (farHeight*0.5f) + right * (farWidth*0.5f);
    farBottomLeft   = farCenter - upwards * (farHeight*0.5f) - right * (farWidth*0.5f);
    farBottomRight  = farCenter - upwards * (farHeight*0.5f) + right * (farWidth*0.5f);

    nearTopLeft     = nearCenter + upwards * (nearHeight*0.5f) - right * (nearWidth*0.5f);
    nearTopRight    = nearCenter + upwards * (nearHeight*0.5f) + right * (nearWidth*0.5f);
    nearBottomLeft  = nearCenter - upwards * (nearHeight*0.5f) - right * (nearWidth*0.5f);
    nearBottomRight = nearCenter - upwards * (nearHeight*0.5f) + right * (nearWidth*0.5f);

    std::cout << "Near Plane Center: (" << nearCenter.x << ", " << nearCenter.y << ", " << nearCenter.z << std::endl;
    std::cout << "Far Plane Center: (" << farCenter.x << ", " << farCenter.y << ", " << farCenter.z << std::endl;

    std::cout << "farTopLeft: (" << farTopLeft.x << ", " << farTopLeft.y << ", " << farTopLeft.z << ")" << std::endl;
    std::cout << "farTopRight: (" << farTopRight.x << ", " << farTopRight.y << ", " << farTopRight.z << ")" << std::endl;
    std::cout << "farBottomLeft: (" << farBottomLeft.x << ", " << farBottomLeft.y << ", " << farBottomLeft.z << ")" << std::endl;
    std::cout << "farBottomRight: (" << farBottomRight.x << ", " << farBottomRight.y << ", " << farBottomRight.z << ")" << std::endl;

    std::cout << "nearTopLeft: (" << nearTopLeft.x << ", " << nearTopLeft.y << ", " << nearTopLeft.z << ")" << std::endl;
    std::cout << "nearTopRight: (" << nearTopRight.x << ", " << nearTopRight.y << ", " << nearTopRight.z << ")" << std::endl;
    std::cout << "nearBottomLeft: (" << nearBottomLeft.x << ", " << nearBottomLeft.y << ", " << nearBottomLeft.z << ")" << std::endl;
    std::cout << "nearBottomRight: (" << nearBottomRight.x << ", " << nearBottomRight.y << ", " << nearBottomRight.z << ")" << std::endl;


    glm::vec3 p0, p1, p2;

    // The PLANES walls (NOT THE EXACTLY THE NEAR OR FAR -- THE WALLS OF IT)
    p0 = nearBottomLeft; p1 = farBottomLeft; p2 = farTopLeft;
    glm::vec3 leftPlaneNormal = glm::normalize(glm::cross(p1-p0, p2-p1));
    float leftPlaneOffset = glm::dot(leftPlaneNormal, p0);

    p0 = nearTopLeft; p1 = farTopLeft; p2 = farTopRight;
    glm::vec3 topPlaneNormal = glm::normalize(glm::cross(p1-p0, p2-p1));
    float topPlaneOffset = glm::dot(topPlaneNormal , p0);

    p0 = nearTopRight; p1 = farTopRight; p2 = farBottomRight;
    glm::vec3 rightPlaneNormal = glm::normalize(glm::cross(p1-p0, p2-p1));
    float rightPlaneOffset = glm::dot(rightPlaneNormal , p0);

    p0 = nearBottomRight; p1 = farBottomRight; p2 = farBottomLeft;
    glm::vec3 bottomPlaneNormal = glm::normalize(glm::cross(p1-p0, p2-p1));
    float bottomPlaneOffset = glm::dot(bottomPlaneNormal , p0);


    std::cout << "leftPlaneNormal: (" << leftPlaneNormal.x << ", " << leftPlaneNormal.y << ", " << leftPlaneNormal.z << ")" << " Offset: " << leftPlaneOffset << std::endl;
    std::cout << "topPlaneNormal: (" << topPlaneNormal.x << ", " << topPlaneNormal.y << ", " << topPlaneNormal.z << ")" << " Offset: " << topPlaneOffset << std::endl;
    std::cout << "rightPlaneNormal: (" << rightPlaneNormal.x << ", " << rightPlaneNormal.y << ", " << rightPlaneNormal.z << ")" << " Offset: " << rightPlaneOffset << std::endl;
    std::cout << "bottomPlaneNormal: (" << bottomPlaneNormal.x << ", " << bottomPlaneNormal.y << ", " << bottomPlaneNormal.z << ")" << " Offset: " << bottomPlaneOffset << std::endl;


    // These are the planes of the near/far itself.
    p0 = nearBottomRight; p1 = nearBottomLeft; p2 = nearTopLeft; // SOME arbitrary points of the near. IT CAN BE ANYTHING.
    glm::vec3 nearPlaneNormal = glm::normalize(glm::cross(p1-p0, p2-p1)); // A PLANE IS 2 PARTS: FIRST, WE NEED THE PLANE'S NORMAL (FANCY WORD FOR DIRECTION)
    float nearPlaneOffset = glm::dot( nearPlaneNormal, p2);  // AND THE SECOND PART IS THE OFFSET: WHICH IS GIVEN BY DOT PRODUCT OF THE PLANE'S NORMAL AND SOME ARBITRARY POINT ON THE PLANE.
    std::cout << "nearPlaneNormal: (" << nearPlaneNormal.x << ", " << nearPlaneNormal.y << ", " << nearPlaneNormal.z << ")" << " Offset: " << nearPlaneOffset << std::endl;

    p0 = farBottomRight; p1 = farBottomLeft; p2 = farTopLeft; // SAME THING, BUT FOR FAR:
    glm::vec3 farPlaneNormal = glm::normalize(glm::cross(p1-p0, p2-p1));
    float farPlaneOffset = glm::dot(farPlaneNormal, farTopRight); // JUST to show off that it can be ANY point (technically p3 in this context!)
    std::cout << "farPlaneNormal: (" << farPlaneNormal.x << ", " << farPlaneNormal.y << ", " << farPlaneNormal.z << ")" << " Offset: " << farPlaneOffset << std::endl;


    // I presume we're gonna have to recalculate the frustum on EVERY rotation/movement of the camera. So... just a heads up when we finish up within runtime.cpp -- DON'T FORGET TO KEEP IT CONTINUALLY UPDATED.
}