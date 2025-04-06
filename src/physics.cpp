#include "physics.hpp"
#include "util.hpp"

#include <Jolt/Jolt.h>
#include <Jolt/Core/Factory.h>
#include <Jolt/Core/JobSystemSingleThreaded.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Core/UnorderedMap.h>
#include <Jolt/Physics/Body/BodyActivationListener.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Character/CharacterVirtual.h>
#include <Jolt/Physics/Collision/BroadPhase/BroadPhaseLayerInterfaceTable.h>
#include <Jolt/Physics/Collision/BroadPhase/ObjectVsBroadPhaseLayerFilterTable.h>
#include <Jolt/Physics/Collision/ObjectLayerPairFilterTable.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/HeightFieldShape.h>
#include <Jolt/Physics/Collision/Shape/MeshShape.h>
#include <Jolt/Physics/Collision/Shape/RotatedTranslatedShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/PhysicsSettings.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/RegisterTypes.h>

// from: Matthias Mueller: https://matthias-research.github.io/pages/publications/realtimeCoursenotes.pdf
static uint32_t spatialHash(int x, int y, int z)
{
    return (uint32_t)((x * 92837111ul) ^ (y * 689287499ul) ^ (z * 283923481ul));
}

struct JoltInitialization
{
    JoltInitialization()
    {
        JPH::RegisterDefaultAllocator();
        JPH::Factory::sInstance = new JPH::Factory;
        JPH::RegisterTypes();
    }
};

struct BroadphaseLayer
{
    JPH::BroadPhaseLayerInterfaceTable table;

    BroadphaseLayer() :
        table(2, 2)
    {
        table.MapObjectToBroadPhaseLayer(0, JPH::BroadPhaseLayer(0));
        table.MapObjectToBroadPhaseLayer(1, JPH::BroadPhaseLayer(1));
    }
};

struct ObjectLayerFilter
{
    JPH::ObjectLayerPairFilterTable filter;

    ObjectLayerFilter() :
        filter(2)
    {
        filter.EnableCollision(0, 1);
        filter.EnableCollision(1, 1);
    }
};

struct CollisionPairRecord
{
    JPH::BodyID bodies[2];
    uint32_t hash;
    uint32_t hashTableIndex;
    std::vector<std::pair<JPH::SubShapeIDPair, JPH::ContactManifold>> contacts;
};

struct ContactListener final: public JPH::ContactListener
{
    JPH::BodyInterface& bodyInterface;
    std::mutex mutex;
    std::vector<uint32_t> collisionPairsMap;
    std::vector<CollisionPairRecord> collisionPairs;
    std::vector<std::pair<JPH::BodyID, JPH::BodyID>> addedPairs;
    std::vector<std::pair<JPH::BodyID, JPH::BodyID>> removedPairs;
    JPH::UnorderedMap<JPH::SubShapeIDPair, uint32_t> subShapePairsMap;

    explicit ContactListener(JPH::BodyInterface& bodyInterface)
        : bodyInterface(bodyInterface), collisionPairsMap(1024, UINT32_MAX)
    {
    }

    ~ContactListener()
    {
    }

    void ExpandHashTable()
    {
        collisionPairsMap.assign(collisionPairsMap.size() * 2, UINT32_MAX);

        for (uint32_t i = 0; i < collisionPairs.size(); ++i)
        {
            auto& pair = collisionPairs[i];

            for (uint32_t j = 0; j < collisionPairsMap.size(); ++j)
            {
                uint32_t index = (pair.hash + j) % collisionPairsMap.size();
                if (collisionPairsMap[index] == UINT32_MAX)
                {
                    collisionPairsMap[index] = i;
                    pair.hashTableIndex = index;
                    break;
                }
            }
        }
    }

    CollisionPairRecord* SearchPair(uint32_t pairHash, const JPH::BodyID body0, const JPH::BodyID body1, uint32_t* hashTableIndex = nullptr)
    {
        for (uint32_t i = 0; i < collisionPairsMap.size(); ++i)
        {
            uint32_t hashIndex = (pairHash + i) % collisionPairsMap.size();
            uint32_t pairIndex = collisionPairsMap[hashIndex];
            if (pairIndex == UINT32_MAX)
            {
                if (hashTableIndex) *hashTableIndex = hashIndex;
                return nullptr;
            }

            auto& pair = collisionPairs[pairIndex];
            if (pair.hash == pairHash && pair.bodies[0] == body0 && pair.bodies[1] == body1)
            {
                if (hashTableIndex) *hashTableIndex = hashIndex;
                return &pair;
            }
        }

        if (hashTableIndex) *hashTableIndex = UINT32_MAX;

        return nullptr;
    }

    const CollisionPairRecord* FindPair(const JPH::BodyID body0, const JPH::BodyID body1) const
    {
        if (body1 < body0)
        {
            return FindPair(body1, body0);
        }

        uint32_t pairHash = spatialHash((int)body0.GetIndex(), (int)body1.GetIndex(), 0);
        for (uint32_t i = 0; i < collisionPairsMap.size(); ++i)
        {
            uint32_t hashIndex = (pairHash + i) % collisionPairsMap.size();
            uint32_t pairIndex = collisionPairsMap[hashIndex];
            if (pairIndex == UINT32_MAX)
            {
                return nullptr;
            }

            auto& pair = collisionPairs[pairIndex];
            if (pair.hash == pairHash && pair.bodies[0] == body0 && pair.bodies[1] == body1)
            {
                return &pair;
            }
        }

        return nullptr;
    }

    CollisionPairRecord& GetPair(const JPH::BodyID body0, const JPH::BodyID body1, uint32_t* outHashTableIndex = nullptr)
    {
        if (body1 < body0)
        {
            return GetPair(body1, body0, outHashTableIndex);
        }

        uint32_t pairHash = spatialHash((int)body0.GetIndex(), (int)body1.GetIndex(), 0);
        uint32_t hashTableIndex;
        CollisionPairRecord* record = SearchPair(pairHash, body0, body1, &hashTableIndex);
        if (outHashTableIndex) *outHashTableIndex = hashTableIndex;
        if (record)
        {
            return *record;
        }

        if (hashTableIndex == UINT32_MAX)
        {
            ExpandHashTable();
            SearchPair(pairHash, body0, body1, &hashTableIndex);
            if (outHashTableIndex) *outHashTableIndex = hashTableIndex;
        }

        if (outHashTableIndex) *outHashTableIndex = hashTableIndex;
        collisionPairsMap[hashTableIndex] = collisionPairs.size();

        collisionPairs.push_back({{ body0, body1 }, pairHash, hashTableIndex, {}});
        addedPairs.emplace_back(body0, body1);

        return collisionPairs.back();
    }

    void OnContactAdded(const JPH::Body& body0, const JPH::Body& body1, const JPH::ContactManifold& manifold, JPH::ContactSettings&) override
    {
        std::lock_guard lock(mutex);
        uint32_t hashTableIndex;
        auto& pair = GetPair(body0.GetID(), body1.GetID(), &hashTableIndex);
        JPH::SubShapeIDPair idPair(body0.GetID(), manifold.mSubShapeID1, body1.GetID(), manifold.mSubShapeID2);
        subShapePairsMap[idPair] = hashTableIndex;

        for (uint32_t i = 0; i < pair.contacts.size(); ++i)
        {
            if (pair.contacts[i].first == idPair)
            {
                pair.contacts[i].second = manifold;
                return;
            }
        }
        pair.contacts.push_back({ idPair, manifold });
    }

    void OnContactPersisted(const JPH::Body& body0, const JPH::Body& body1, const JPH::ContactManifold& manifold, JPH::ContactSettings& settings) override
    {
        OnContactAdded(body0, body1, manifold, settings);
    }

    void OnContactRemoved(const JPH::SubShapeIDPair& subShapeIDPair) override
    {
        std::lock_guard lock(mutex);
        auto iter = subShapePairsMap.find(subShapeIDPair);
        if (iter == subShapePairsMap.end()) return;
        auto hashTableIndex = iter->second;
        subShapePairsMap.erase(iter);

        uint32_t pairIndex = collisionPairsMap[hashTableIndex];
        auto& pair = collisionPairs[pairIndex];
        auto contactIter = std::find_if(pair.contacts.begin(), pair.contacts.end(), [&](const auto& c) {
                return c.first == subShapeIDPair;
            });
        if (contactIter == pair.contacts.end()) return;
        pair.contacts.erase(contactIter);

        if (pair.contacts.empty())
        {
            removedPairs.emplace_back(pair.bodies[0], pair.bodies[1]);
            collisionPairsMap[hashTableIndex] = UINT32_MAX;
            if (pairIndex < collisionPairs.size() - 1)
            {
                pair = std::move(collisionPairs.back());
                collisionPairs.pop_back();
                collisionPairsMap[pair.hashTableIndex] = pairIndex;
            }
        }
    }
};

struct PhysicsWorld final :
    fff::PhysicsWorldInterface,
    JoltInitialization
{
    JPH::TempAllocatorMalloc tempAllocator;
    JPH::JobSystemSingleThreaded jobSystem;
    BroadphaseLayer broadPhaseLayer;
    ObjectLayerFilter objectLayerFilter;
    JPH::ObjectVsBroadPhaseLayerFilterTable objectVsBroadPhaseLayerFilter;
    JPH::PhysicsSystem physicsSystem;
    InitShim<&JPH::PhysicsSystem::Init> initPhysicsSystem;
    ContactListener contactListener;
    std::function<void(const JPH::BodyID, const JPH::BodyID)> onCollisionEnter = nullptr;
    std::function<void(const JPH::BodyID, const JPH::BodyID)> onCollisionExit = nullptr;

    PhysicsWorld() :
        tempAllocator(),
        jobSystem(JPH::cMaxPhysicsJobs),
        objectVsBroadPhaseLayerFilter(broadPhaseLayer.table, 2, objectLayerFilter.filter, 2),
        physicsSystem(),
        initPhysicsSystem(physicsSystem, 65536, 0, 65536, 10240, broadPhaseLayer.table, objectVsBroadPhaseLayerFilter, objectLayerFilter.filter),
        contactListener(physicsSystem.GetBodyInterface())
    {
        physicsSystem.SetContactListener(&contactListener);
    }

    JPH::PhysicsSystem& getPhysicsSystem() override
    {
        return physicsSystem;
    }

    void update(const float deltaTime) override
    {
        physicsSystem.Update(deltaTime, 1, &tempAllocator, &jobSystem);

        if (onCollisionEnter) for (auto&& [body0, body1] : contactListener.addedPairs)
        {
            onCollisionEnter(body0, body1);
            onCollisionEnter(body1, body0);
        }
        if (onCollisionExit) for (auto&& [body0, body1] : contactListener.removedPairs)
        {
            onCollisionExit(body0, body1);
            onCollisionExit(body1, body0);
        }
        contactListener.addedPairs.clear();
        contactListener.removedPairs.clear();
    }

    void updateCharacter(JPH::CharacterVirtual& character, const float deltaTime) override
    {
        character.ExtendedUpdate(deltaTime, physicsSystem.GetGravity(),
                JPH::CharacterVirtual::ExtendedUpdateSettings(),
                JPH::DefaultBroadPhaseLayerFilter(objectVsBroadPhaseLayerFilter, 1),
                JPH::DefaultObjectLayerFilter(objectLayerFilter.filter, 1),
                {}, {}, tempAllocator);
    }

    void setOnCollisionEnter(const std::function<void(JPH::BodyID, JPH::BodyID)>& fn) override
    {
        onCollisionEnter = fn;
    }

    void setOnCollisionExit(const std::function<void(JPH::BodyID, JPH::BodyID)>& fn) override
    {
        onCollisionExit = fn;
    }
};

fff::PhysicsWorldInterface* fff::createPhysicsWorld()
{
    return new PhysicsWorld;
}
