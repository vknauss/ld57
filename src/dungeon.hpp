#pragma once

#include <cstdint>
#include <vector>
#include <Jolt/Jolt.h>
#include <Jolt/Physics/Body/Body.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include "engine.hpp"

struct Dungeon
{
    struct Room
    {
        uint32_t x, y;
        uint32_t width, height;
        struct { uint32_t start; uint32_t end; } portalRecordsRange;
        union {
            struct { uint32_t left, top, right, bottom; };
            uint32_t i[4];
        } wallStartPortalRecord;
    };

    struct Portal
    {
        uint32_t rooms[2];
        uint32_t x, y;
    };

    struct RoomPortalRecord
    {
        uint32_t rooms[2];
        uint32_t portal;
    };

    std::vector<Room> rooms;
    std::vector<Portal> portals;
    std::vector<RoomPortalRecord> roomPortalRecords;

    struct GenerationParams
    {
        uint64_t seed;
        uint32_t width, height;
        uint32_t partitionedRoomCount;
        uint32_t targetRoomCount;
        uint32_t minSplitDimension;
        uint32_t minPortalOverlap;
    };

    static Dungeon generate(const GenerationParams& params);

    eng::GeometryDescription createGeometry(const float wallHeight, const float doorWidth, const float wallThickness, const float doorHeight) const;

    void createPhysicsBodies(const float wallHeight, const float doorWidth, const float wallThickness,
            std::vector<JPH::BodyID>& bodies, std::vector<JPH::Ref<JPH::Shape>>& shapeRefs, JPH::PhysicsSystem& physicsSystem) const;
};

