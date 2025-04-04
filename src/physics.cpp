#include "physics.hpp"

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

    PhysicsWorld() :
        tempAllocator(),
        jobSystem(JPH::cMaxPhysicsJobs),
        objectVsBroadPhaseLayerFilter(broadPhaseLayer.table, 2, objectLayerFilter.filter, 2),
        physicsSystem()
    {
        physicsSystem.Init(65536, 0, 65536, 10240, broadPhaseLayer.table,
                objectVsBroadPhaseLayerFilter, objectLayerFilter.filter);
    }

    JPH::PhysicsSystem& getPhysicsSystem() override
    {
        return physicsSystem;
    }

    void update(const float deltaTime) override
    {
        physicsSystem.Update(deltaTime, 1, &tempAllocator, &jobSystem);
    }

    void updateCharacter(JPH::CharacterVirtual& character, const float deltaTime) override
    {
        character.ExtendedUpdate(deltaTime, physicsSystem.GetGravity(),
                JPH::CharacterVirtual::ExtendedUpdateSettings(),
                JPH::DefaultBroadPhaseLayerFilter(objectVsBroadPhaseLayerFilter, 1),
                JPH::DefaultObjectLayerFilter(objectLayerFilter.filter, 1),
                {}, {}, tempAllocator);
    }
};

fff::PhysicsWorldInterface* fff::createPhysicsWorld()
{
    return new PhysicsWorld;
}


