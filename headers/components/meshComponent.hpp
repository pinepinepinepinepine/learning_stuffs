#include "components.hpp"

// Mesh component
// Manages the visual representation of an entity by handling its 3D mesh (collection of vertices) and material (textures)

// TODO: We'll have to make a Mesh struct to handle vertices (like our Model struct), and a Material struct to handle textures (like our Texture struct).

class MeshComponent : public Component
{
    Mesh* mesh = nullptr;
    Material* material = nullptr;

    public:
    MeshComponent( Mesh* m, Material* mat ) : mesh (m ), material( mat ) {}

    void SetMesh(Mesh* m) { mesh = m; }
    void SetMaterial(Material* mat) { material = mat; }
    Mesh* GetMesh() const { return mesh; }
    Material* GetMaterial() const { return material; }

    void Render() override;
};