#pragma once
#include "../../headers/includes.hpp"

// Serves as a central way where we store ANY kind of resources: whether it be models, textures, etc.
// The idea is that we want to reuse assets as many times as we can.
// Create the asset in memory, then our resourceManager can be used a kind of middleman.

// https://stackoverflow.com/questions/8317508/hash-function-for-a-string
inline uint32_t hashString( const char* str )
{
    uint32_t hash = 1315423911;
    for ( const char* c = str; *c != '\0'; c++ ) {
        hash ^= ((hash << 5) + *c + (hash >> 2));
    }
    return hash;
}

template<typename T>
class ResourceHandle;

template <typename T>
class ResourceManager
{
    std::unordered_map<uint32_t, std::unique_ptr<T>> resourceMap;

    public:
    ~ResourceManager() = default; // We don't have to delete our resource pointer's underlying memory because it's a unique_ptr and will call delete by itself when resourceMap is destroyed.

    ResourceHandle<T> addResource( uint32_t key, std::unique_ptr<T> resource )
    {
        if ( resourceMap.find(key) == resourceMap.end() )
        {
            std::cout << "Added Resource: " << key << std::endl;
            resourceMap.emplace( key, std::move( resource ) );
        }
        else
            std::cout << "Key found: we are not re-adding the same hash at key " << key << std::endl;
        return ResourceHandle<T>(key, this);
    }

    void removeResource( uint32_t key )
    {
        resourceMap.erase(key); // We don't have to call delete on the T pointer because it's unique_ptr and will delete underlying memory itself.
    }

    T* getResource( uint32_t key )
    {
        auto element = resourceMap.find(key);

        if ( element != resourceMap.end() )
            return element->second.get(); // .get() returns the pointer address. We can't convert unique_ptr<T*> to T*, so we need the address itself. We don't wanna pass a unique_ptr as it's sole ownership.
        return nullptr;
    }
};

template<typename T>
class ResourceHandle
{
    uint32_t resourceHashID;
    ResourceManager<T>* resourceManager = nullptr;

    public:
    ResourceHandle() : resourceManager(nullptr) {}

    ResourceHandle( uint32_t filepathHash, ResourceManager<T>* manager)
    : resourceHashID( filepathHash ), resourceManager(manager)
    {}

    T* get() const
    { return resourceManager->getResource( resourceHashID ); }

    uint32_t getResourceHash()
    { return resourceHashID; }
};