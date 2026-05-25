#include "../includes.hpp"

class Entity;

class Component
{
    protected:
    Entity* ownerOfComponent = nullptr; // which ENTITY owns this component.

    public:
    void SetOwner(Entity* entity) { ownerOfComponent = entity; }
    Entity* GetOwner() const { return ownerOfComponent; }

    // Virtual Functions: the derived component classes (specific components) will be executed, NOT the base class's virtual functions.
        // this implies that each derived component class HAS a destructor, initialize(), update() and render() function.
        // HOWEVER: if we DO NOT write an overriding function (one of these functions) within the derived class, it will execute these functions from the base class as a fallback.
    virtual ~Component() = default;         // Destructor (~)
    virtual void Initialize() {}
    virtual void Update(float deltaTime) {}
    virtual void Render() {}
};