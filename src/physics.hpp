#pragma once

#include <vector>

namespace JPH
{
    class CharacterVirtual;
    class PhysicsSystem;
    class BodyID;
    class SubShapeIDPair;
    class ContactManifold;
}

namespace std
{
    template<class> class function;
}

namespace fff
{
    struct PhysicsWorldInterface
    {
        virtual ~PhysicsWorldInterface() = default;

        virtual JPH::PhysicsSystem& getPhysicsSystem() = 0;

        virtual void update(const float deltaTime) = 0;

        virtual void updateCharacter(JPH::CharacterVirtual& character, const float deltaTime) = 0;

        virtual void setOnCollisionEnter(const std::function<void(JPH::BodyID, JPH::BodyID)> &fn) = 0;
        virtual void setOnCollisionExit(const std::function<void(JPH::BodyID, JPH::BodyID)> &fn) = 0;

        virtual std::vector<std::pair<JPH::SubShapeIDPair, JPH::ContactManifold>> getContacts(const JPH::BodyID body0, const JPH::BodyID body1) const = 0;
    };

    PhysicsWorldInterface* createPhysicsWorld();
}
