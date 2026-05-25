#include "includes.hpp"
#include "components/components.hpp"

class Entity
{
    std::string name;
    bool active = true; // active: as in, if it should have calculations performed, essentially: is it present? if it's not active, we can forego calculating stuff with it as it doesnt exist.
    std::vector<std::unique_ptr<Component>> components;

    public:
    explicit Entity( const std::string& entityName ) : name(entityName) {}

    const std::string& GetName() const { return name; }
    bool IsActive() const { return active; }
    void SetActive(bool isActive) { active = isActive; }

    void Initialize();
    void Update(float deltaTime);
    void Render();

    template<typename T, typename... Args>
    T* AddComponent(Args&&... args) // see NOTES' std.txt -- universal reference to differentiate w/ std::forward passed rvalues and lvalues.
    {
        static_assert(std::is_base_of<Component, T>::value, "T must derive from Component");

        // Create new component
        auto component = std::make_unique<T>(std::forward<Args>(args)...); // Constructs the component of type T using the full args list provided
        T* componentPtr = component.get(); // get the component's std::make_unique's underlying unique_ptr (which is a pointer).
        componentPtr->SetOwner(this); // set the owner of the component we just created to this entity object.
        components.push_back(std::move(component)); // then within entity's components vector, push this component so we can access it through the vector.
        return componentPtr;
    }

    // If the getComponent<ComponentTypenameToGet>() type name is not equal to the component's type, then component.get() returns nullptr because dynamic_cast<T*> cannot be cast'd/converted;
    // it has to be castable, otherwise nullptr, disallowing the condition to true + return as nullptr == false, preventing the if from continuing.
        // Essentially, if the <type> specified in the function call does NOT match the component's type, keep iterating until it finds the right component type.
    template<typename T>
    T* GetComponent() {
        for (auto& component : components) {
            if (T* result = dynamic_cast<T*>(component.get())) {
                return result;
            }
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