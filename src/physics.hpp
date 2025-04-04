#pragma once

namespace JPH
{
    class CharacterVirtual;
    class PhysicsSystem;
}

namespace fff
{
    struct PhysicsWorldInterface
    {
        virtual ~PhysicsWorldInterface() = default;

        virtual JPH::PhysicsSystem& getPhysicsSystem() = 0;

        virtual void update(const float deltaTime) = 0;

        virtual void updateCharacter(JPH::CharacterVirtual& character, const float deltaTime) = 0;
    };

    PhysicsWorldInterface* createPhysicsWorld();
}
