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
    // MAYBE make a seperate component, but WHATEVER. Idea is we're making it on the heap cause Id rather not make it like a... weird dingy pointer on the stack or own it elsewhere.
    std::unique_ptr<ModelData> visualBox;

    BoundingComponent( AABB_box box ) : boundingBox(box)
    {
        std::cout << "Bounding Box Created: (" << boundingBox.min.x << "x, " << boundingBox.min.y << "y, " << boundingBox.min.z << "z) -> ("
        << boundingBox.max.x << "x, " << boundingBox.max.y << "y, " << boundingBox.max.z << "z)\n";
    }

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

        // matrix[column][row]
        // std::cout << "ROW 0: (" << transformMatrix[0][0] << "x, " << transformMatrix[1][0] << "y, " << transformMatrix[2][0] << "z, " << transformMatrix[3][0] << "w)" << std::endl;
        // std::cout << "ROW 1: (" << transformMatrix[0][1] << "x, " << transformMatrix[1][1] << "y, " << transformMatrix[2][1] << "z, " << transformMatrix[3][1] << "w)" << std::endl;
        // std::cout << "ROW 2: (" << transformMatrix[0][2] << "x, " << transformMatrix[1][2] << "y, " << transformMatrix[2][2] << "z, " << transformMatrix[3][2] << "w)" << std::endl;
        // std::cout << "ROW 3: (" << transformMatrix[0][3] << "x, " << transformMatrix[1][3] << "y, " << transformMatrix[2][3] << "z, " << transformMatrix[3][3] << "w)" << std::endl;

        // std::cout << "Old Min: (" << boundingBox.min.x << "x, " << boundingBox.min.y << "y, " << boundingBox.min.z << "z)" << std::endl;
        // std::cout << "Old Max: (" << boundingBox.max.x << "x, " << boundingBox.max.y << "y, " << boundingBox.max.z << "z)" << std::endl;

        AABB_box transformedBox( transformMatrix * glm::vec4(boundingBox.min, 1.0f), transformMatrix * glm::vec4(boundingBox.max, 1.0f) );

        // std::cout << "NEW Min: (" << transformedBox.min.x << "x, " << transformedBox.min.y << "y, " << transformedBox.min.z << "z)" << std::endl;
        // std::cout << "NEW Max: (" << transformedBox.max.x << "x, " << transformedBox.max.y << "y, " << transformedBox.max.z << "z)\n" << std::endl;

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

        //std::cout << normal.x << "x + " << normal.y << "y + " << normal.z << "z = " << offset << std::endl;
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

    // If true: Render it; if false: Cull it. Outward Plane Normals.
            // APPARENTLY: you don't need to check EVERY corner for EVERY plane. you can just check ONE through some logic.
            // FIGURE. IT. OUT.
    // https://learnopengl.com/Guest-Articles/2021/Scene/Frustum-Culling
    bool isBoxWithinFrustum( const AABB_box& box )
    {
        std::vector<Vertex> vertexCorners = box.getBoxCorners();

        for ( const auto& vertex : vertexCorners )
        {
            std::cout << "(" << vertex.base.pos.x << ", " << vertex.base.pos.y << ", " << vertex.base.pos.z;
            std::cout << ")\n";
        }


        for ( int i = 0; i < 6; i++ )
        {
            std::cout << frustumPlanes[i].normal.x << "x + " << frustumPlanes[i].normal.y << "y + " << frustumPlanes[i].normal.z << "z = " << frustumPlanes[i].offset << "\n";
        }

        getFrustumCorners();


        Vertex centre = glm::vec3( (box.min.x + box.max.x) / 2, (box.min.y + box.max.y) / 2, (box.min.z + box.max.z) / 2 );

        glm::vec3 trueMin = glm::min(box.max, box.min);
        glm::vec3 trueMax = glm::max(box.max, box.min);
        glm::vec3 extent = glm::vec3( ( trueMax.x - trueMin.x ) / 2, ( trueMax.y - trueMin.y ) / 2,( trueMax.z - trueMin.z ) / 2  );

        // glm::vec3 min = glm::vec3(std::numeric_limits<float>::max());
        // glm::vec3 max = glm::vec3(-std::numeric_limits<float>::max());
        // for ( const auto& vertexCorner : vertexCorners )
        // {
        //     min.x = std::min( vertexCorner.base.pos.x, min.x );
        //     min.y = std::min( vertexCorner.base.pos.y, min.y );
        //     min.z = std::min( vertexCorner.base.pos.z, min.z );

        //     max.x = std::max( vertexCorner.base.pos.x, max.x );
        //     max.y = std::max( vertexCorner.base.pos.y, max.y );
        //     max.z = std::max( vertexCorner.base.pos.z, max.z );
        // }



        // std::cout << "Min: " << box.min.x << ", " << box.min.y << ", " << box.min.z << ")\n";
        // std::cout << "Max: " << box.max.x << ", " << box.max.y << ", " << box.max.z << ")\n";
        // std::cout << "True Min: " << trueMin.x << ", " << trueMin.y << ", " << trueMin.z << ")\n";
        // std::cout << "True Max: " << trueMax.x << ", " << trueMax.y << ", " << trueMax.z << ")\n";
        // std::cout << "Iterated Min: " << min.x << ", " << min.y << ", " << min.z << ")\n";
        // std::cout << "Iterated Max: " << max.x << ", " << max.y << ", " << max.z << ")\n";
        // std::cout << "Extent: " << extent.x << ", " << extent.y << ", " << extent.z << ")\n";
        // std::cout << "Centre: " << centre.base.pos.x << ", " << centre.base.pos.y << ", " << centre.base.pos.z << ")\n";

        const float r = extent.x * std::abs(frustumPlanes[0].normal.x) +
            extent.y * std::abs(frustumPlanes[0].normal.y) + extent.z * std::abs(frustumPlanes[0].normal.z);

        //std::cout << "extentx * normalx: " << extent.x << " * " << std::abs(frustumPlanes[0].normal.x)<< " = " << ( extent.x * std::abs(frustumPlanes[0].normal.x ) ) << "\n";

        float distance = glm::dot(frustumPlanes[0].normal, centre.base.pos) - frustumPlanes[0].offset;

        // std::cout << "Distance: " << distance << "\n";
        // std::cout << "R: " << r << "\n";
        // std::cout << "Is within: " << (distance >= -r) << "\n\n";


        for ( const auto& vertexCorner : vertexCorners )
        {
            int outsideCorners = 0;
            for ( const auto& plane : frustumPlanes )
            {
                float distance = ( glm::dot( plane.normal, vertexCorner.base.pos ) ) - plane.offset;

                if ( distance > 0 )
                    outsideCorners++;
            }

            if ( outsideCorners >= 6 )
                return true;

        }
        return false;
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

        for ( const auto& vertex : vertices )
        {
            std::cout << "(" << vertex.base.pos.x << ", " << vertex.base.pos.y << ", " << vertex.base.pos.z << ")\n";
        }

        std::cout << "\n";

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

    Frustum& updateCameraFrustum()
    {
        camFrustum.createFrustum();
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

    glm::vec3 position = thisEntitysTransformComponent->GetPosition();
    glm::quat rotation = thisEntitysTransformComponent->GetRotation();

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
}