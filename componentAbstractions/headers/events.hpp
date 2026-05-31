#include "../../headers/includes.hpp"

class Entity;

struct Event
{
    virtual ~Event() = default;
};

class MoveEvent : public Event // Don't really need the inheritance but whatever
{
    Entity* entity;

    public:
    MoveEvent( Entity* e1 ) : entity( e1 ) {}

    Entity* GetEntity() const { return entity; }
};



struct EventListener
{
    virtual void OnEvent( const Event& event ) = 0; // Figure out WHY it's = 0?
};

class EventSystem
{
    std::vector<EventListener*> listeners;

    public:
    void AddListener( EventListener* listener )
    {
        listeners.push_back( listener );
    }


    void DispatchEvent( const Event& event )
    {
        for ( auto listener : listeners )
        {
            listener->OnEvent(event);
        }
    }

    // Every single listener within the listeners is a listener that belongs to a certain component
    // on Event sends a general Event to that listener's original struct/component.
    // onEvent is VIRTUAL so it goes to that listener's component struct to do whatever is written there.
    // we FILTER specific events through this general event because it can be downcasted with dynamic_cast
        // dynamic_cast<const CollisionEvent*>(&event) is the example provided within the tutorial site:
        // If we want to create a collision event to occur whenever it happens, on the dispatchEvent call, within the physicalComponent's override of OnEvent (remember, EventListener's OnEvent is virtual)
        // we can then just downcast it like that and then do WHATEVER.



};