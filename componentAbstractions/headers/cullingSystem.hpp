#include "entity.hpp"


class CullingSystem
{
    CameraComponent* camera;
    std::vector<Entity*> visibleEntities;

    public:
    void setCamera( CameraComponent* cam )
    {
        camera = cam;
    }

    // idea is: first pass ALL the entities, and then check the entities bounding box against the camera's frustum.
    void CullScene( const std::vector<Entity*>& allEntities )
    {
        visibleEntities.clear();
        if ( !camera ) return;

        Frustum& frustum = camera->getCameraFrustum(); // HAS to be updated per frame for the cull.

        for ( auto entity : allEntities )
        {
            // For an entity, we should also check if it's active (if yes, go to the next loop iteration via continue), but whatever, ignore it for now.

            auto modelComponent = entity->GetComponent<ModelComponent>();
            if ( !modelComponent ) throw std::runtime_error("Culling: model component is nullptr."); // Tutorial does a continue instead of throw.

            auto transformComponent = entity->GetComponent<TransformComponent>();
            if ( !transformComponent ) throw std::runtime_error("Culling: transform component is nullptr.");; // Tutorial does a continue instead of throw.

            auto boundingComponent = entity->GetComponent<BoundingComponent>();
            if ( !boundingComponent ) throw std::runtime_error("Culling: bounding component is nullptr.");; // In the tutorial, it isn't a compotent, but I think it's neater like this.

            // THIS IS NOT GOOD ENOUGH.
            AABB_box box = boundingComponent->transform( transformComponent->GetTransformMatrix() );
            frustum.isBoxWithinFrustum( box );
            // if ( frustum.isBoxWithinFrustum( box ) )
            //     std::cout << "RENDERING!\n";
            // else
            //     std::cout << "culled.\n";
        }

    }

};