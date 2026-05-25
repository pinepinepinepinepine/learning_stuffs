#include "component.hpp"
#include "../../headers/includes.hpp"

// An entity is essentially a bundle of components:
// An entity (can) have a model, and whatever other components we give it
// hopefully, we can render entities directly by accessing their underlying components types.
// So, if the entity has a model, we render that in 3D, and if it has a texture, we apply it to that model.

struct Entity
{
    std::string nameOfEntity;
    bool isEntityActive = false;
    std::vector<std::unique_ptr<Component>> components;

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