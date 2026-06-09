//#include "component.hpp"
#include "../../headers/includes.hpp"
#include "../../headers/model.hpp"
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
    T* GetComponent() const {
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

struct BoundingComponent : public Component
{
    AABB_box boundingBox;

    // MAYBE make a seperate component, but WHATEVER. Idea is we're making it on the heap cause Id rather not make it like a... weird dingy pointer on the stack or own it elsewhere.
    std::unique_ptr<ModelData> visualBox;
    const glm::vec3 local_centre;
    const glm::vec3 local_half_extents;

    BoundingComponent( AABB_box box ) : boundingBox(box),
    local_centre( ( box.min + box.max ) * 0.5f ),
    local_half_extents(
        glm::abs(box.max.x - box.min.x) * 0.5f, glm::abs(box.max.y - box.min.y) * 0.5f, glm::abs(box.max.z - box.min.z) * 0.5f )
    {}

    void createBoundingBuffer( const LogicalDevice& device )
    {
        std::vector<Vertex> corners = boundingBox.getBoxCorners();

        visualBox = std::make_unique<ModelData>();

        visualBox.get()->vertexBuffer.createGPUBuffer(
            device,
            sizeof(corners[0]) * corners.size(),
            vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eTransferDst,
            vk::MemoryPropertyFlagBits::eDeviceLocal, false );
        visualBox.get()->vertexBuffer.muleBuffer( device, corners ); // make getBoxCorners return an array again, maybe overload muleBuffer to accept an array.
        visualBox.get()->vertices_count = corners.size(); // Maybe make an index buffer as well?

        // Index Buffer: We're making a 3d rectangle, hence.
        std::vector<uint32_t> indices = AABB_box::getBoxIndices();

        visualBox.get()->indexBuffer.createGPUBuffer(
            device, sizeof(indices[0]) * indices.size(),
            vk::BufferUsageFlagBits::eIndexBuffer | vk::BufferUsageFlagBits::eTransferDst,
            vk::MemoryPropertyFlagBits::eDeviceLocal, false );
        visualBox.get()->indexBuffer.muleBuffer( device, indices );
        visualBox.get()->indices_count = indices.size();
    }

    AABB_box transform( glm::mat4 transformMatrix )
    {
        AABB_box transformedBox( transformMatrix * glm::vec4(boundingBox.min, 1.0f), transformMatrix * glm::vec4(boundingBox.max, 1.0f) );

        return transformedBox;
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

class RenderComponent : public Component
{
    Descriptor* descriptor;
    Pipeline* pipeline;

    public:
    RenderComponent() = default;
    RenderComponent( Pipeline* p, Descriptor* d ) : descriptor(d), pipeline(p) {}

    void setDescriptor( Descriptor* d ) { descriptor = d; }
    void setPipeline( Pipeline* p ) { pipeline = p; }

    Descriptor* getDescriptor() { return descriptor; }
    Pipeline* getPipeline() { return pipeline; }
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
    TransformComponent() = default;
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

// Move this.
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

        // One thing to ensure: check math.txt about culling rules: ensure the p0, p1,and p2 are ALL either COUNTER CLOCKWISE or CLOCKWISE across all
        // this is because for our culling logic, we want to ensure we can use the same logic of "Greater than the plane's offset is culled or rendered"
        // The way to check is visualize if the points from p0 -> p1 -> p2 -> p0 is clockwise/counterclockwise: if it's counterclockwise, it's OUTWARD, so greater than 0 distance is culled.

        // I presume we're gonna have to recalculate the frustum on EVERY rotation/movement of the camera. So... just a heads up when we finish up within runtime.cpp -- DON'T FORGET TO KEEP IT CONTINUALLY UPDATED.
    }
    Plane( glm::vec3 n, float o ) : normal(n), offset(o) {} // Explicit
    Plane( glm::vec4 p ) : normal( glm::vec3( p.x, p.y, p.z ) ), offset(p.w) {} // Implicit

};



class Frustum
{
    const CameraComponent* camera;

    std::array<Plane, 6> frustumPlanes;

    public:
    // MAYBE make a seperate component, but WHATEVER. Idea is we're making it on the heap cause Id rather not make it like a... weird dingy pointer on the stack or own it elsewhere.
    std::unique_ptr<ModelData> visualFrustum;

    Frustum() = default;
    Frustum( const CameraComponent* cam ) : camera(cam) {}

    void createFrustumBuffer( const LogicalDevice& device )
    {
        std::vector<Vertex> corners(8);

        // Making this device coherent for now, make it local, not sure how to handle rotation of them -- do we set it once and rotate/transform it accordingly?

        visualFrustum = std::make_unique<ModelData>();

        visualFrustum.get()->vertexBuffer.createGPUBuffer(
            device,
            sizeof(corners[0]) * corners.size(),
            vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eTransferDst,
            vk::MemoryPropertyFlagBits::eHostCoherent | vk::MemoryPropertyFlagBits::eHostVisible, true );
        visualFrustum.get()->vertices_count = 8;

        // Figure out index buffer later.
        std::vector<uint32_t> indices = Frustum::getFrustumIndices();
        visualFrustum.get()->indexBuffer.createGPUBuffer(
            device, sizeof(indices[0]) * indices.size(),
            vk::BufferUsageFlagBits::eIndexBuffer | vk::BufferUsageFlagBits::eTransferDst,
            vk::MemoryPropertyFlagBits::eDeviceLocal, false );
        visualFrustum.get()->indexBuffer.muleBuffer( device, indices );
        visualFrustum.get()->indices_count = indices.size();
    }

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

    void updateFrustum();

    void updateVisualFrustumBounds( const LogicalDevice& device )
    {
        if ( !visualFrustum ) // make this an assert.
            throw std::runtime_error("Did not set the visual frustum -- call createBoundignBuffer()!");

        std::vector<Vertex> vertices = getFrustumCorners();

        std::memcpy( visualFrustum.get()->vertexBuffer.gpuBufferMapped, vertices.data(), vertices.size() * sizeof(Vertex) );
    }

    // If true: Render it; if false: Cull it. Outward Plane Normals.
            // APPARENTLY: you don't need to check EVERY corner for EVERY plane. you can just check ONE through some logic.
            // FIGURE. IT. OUT.
    // https://learnopengl.com/Guest-Articles/2021/Scene/Frustum-Culling
    bool isBoxWithinFrustum( glm::mat4 transformationMatrix, std::vector<Vertex> vertices )
    {
        AABB_box minMax { vertices, transformationMatrix };
        BoundingComponent local { minMax };

        for ( const auto& plane : frustumPlanes )
        {
            // If we want some more precision, look up SAT techniques, but this is VERY good enough. (Gets dicey around the near plane, BUT it DOESNT get culled -- it'll RENDER.)
            float rectangleRadius = local.local_half_extents.x * glm::abs( plane.normal.x ) +
                local.local_half_extents.y * glm::abs( plane.normal.y ) +
                local.local_half_extents.z * glm::abs( plane.normal.z );

            float centreToPlane = glm::dot( plane.normal, local.local_centre ) - plane.offset;

            if ( rectangleRadius >= centreToPlane && rectangleRadius < -centreToPlane )
                return false;
        }
        return true;
    }

    static std::vector<uint32_t> getFrustumIndices()
    {
        std::vector<uint32_t> indices
        {
            0, 1, 0, 2, 1, 3, 2, 3, // Near Plane
            4, 5, 4, 6, 5, 7, 6, 7, // Far Plane
            1, 5, 0, 4, // Left Plane
            2, 6, 3, 7 // Right Plane
        };
        return indices;
    }

    std::vector<Vertex> getFrustumCorners()
    {
        std::vector<Plane> leftRightPlanes = { frustumPlanes[0], frustumPlanes[2] };
        std::vector<Plane> bottomTopPlanes = { frustumPlanes[1], frustumPlanes[3] };
        std::vector<Plane> nearFarPlanes = { frustumPlanes[4], frustumPlanes[5] };

        std::vector<Vertex> vertices;

        for ( const auto& viewPlane : nearFarPlanes )
        {
            for ( const auto& xPlane : leftRightPlanes )
            {
                for (const auto& yPlane : bottomTopPlanes )
                {
                    // Using some formula found online.
                    glm::vec3 reusedCrossProduct = glm::cross( xPlane.normal, yPlane.normal );

                    glm::vec3 point =
                    ( ( viewPlane.offset * reusedCrossProduct ) +
                    ( xPlane.offset * ( glm::cross( yPlane.normal, viewPlane.normal ) ) ) +
                    ( yPlane.offset * ( glm::cross( viewPlane.normal, xPlane.normal ) ) ) )
                    / ( glm::dot( viewPlane.normal, reusedCrossProduct ) );

                    vertices.emplace_back( glm::vec3( point ) );
                }
            }
        }
        return vertices;
    }
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
    CameraComponent() { camFrustum.setCamera(this); }

    Frustum& createCameraFrustum()
    {
        camFrustum.createFrustum();
        return camFrustum;
    }

    Frustum& updateCameraFrustum()
    {
        camFrustum.updateFrustum();
        return camFrustum;
    }

    // Keeping this in but it's kinda dead. also the AABB_box's calculate 8 corners thing is useless, so remove it.
    Frustum& getCameraFrustum()
    { return camFrustum; }

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

    glm::vec3 position = glm::vec3(0.0f);
    glm::quat rotation = glm::quat();

    glm::vec3 forward = rotation * glm::vec3( 0.0f, 0.0f, -1.0f );
    glm::vec3 upwards = rotation * glm::vec3( 0.0f, 1.0f, 0.0f );
    glm::vec3 right = rotation * glm::vec3( 1.0f, 0.0f, 0.0f );

    glm::vec3 nearCenter = position + forward * camera->nearPlane;
    glm::vec3 farCenter = position + forward * camera->farPlane;

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
    // Convert the planes into world view.
    updateFrustum();
}

inline void Frustum::updateFrustum()
{
    static std::array<Plane, 6> localSpacePlanes = frustumPlanes;
    glm::mat4 invTransposeM = glm::transpose( glm::inverse( camera->GetOwner()->GetComponent<TransformComponent>()->GetTransformMatrix() ) );

    for ( size_t i = 0; i < localSpacePlanes.size(); i++ )
    {
        const auto& localPlane = localSpacePlanes[i];

        // p' = transpose(inverse(M))*p -- THE LAST PLANE'S NORMAL IS POINTING IN A DIFFERENT DIRECTION, HENCE THE ANNOYINGNESS. https://stackoverflow.com/questions/7685495/transforming-a-3d-plane-using-a-4x4-matrix
        glm::vec4 t = invTransposeM * glm::vec4( localPlane.normal, -localPlane.offset );
        t.w *= -1.0; // FOR SOME REASON, * -1.0 AND OFFSET IS NEGATIVE.
        frustumPlanes[i] = t;
    }
}