#include "dungeon.hpp"

#include <queue>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include "jph_glm_convert.hpp"

constexpr uint32_t U32_MAX = std::numeric_limits<uint32_t>::max();

Dungeon Dungeon::generate(const Dungeon::GenerationParams& params)
{
    std::mt19937_64 prng;
    prng.seed(params.seed);

    // Partition space iteratively to create base layout of rooms. Continue until reached partitionedRoomCount
    auto compareRoomsForSplit = [](const auto& left, const auto& right) {
        return std::min(left.width, left.height) < std::min(right.width, right.height);
    };
    std::priority_queue<Room, std::vector<Room>, decltype(compareRoomsForSplit)> splitQueue(compareRoomsForSplit);
    splitQueue.push(Room {
            .x = 0,
            .y = 0,
            .width = params.width,
            .height = params.height,
        });
    std::vector<Room> rooms;

    while (!splitQueue.empty() && rooms.size() + splitQueue.size() < params.partitionedRoomCount)
    {
        Room parent = splitQueue.top();
        splitQueue.pop();

        if (std::max(parent.width, parent.height) < params.minSplitDimension)
        {
            rooms.push_back(parent);
            continue;
        }

        Room child0 = parent;
        Room child1 = parent;

        if (parent.width >= parent.height)
        {
            uint32_t split = (prng() % (parent.width - params.minSplitDimension)) + params.minSplitDimension / 2;
            child0.width = split;
            child1.x += split;
            child1.width = parent.width - split;
        }
        else
        {
            uint32_t split = (prng() % (parent.height - params.minSplitDimension)) + params.minSplitDimension / 2;
            child0.height = split;
            child1.y += split;
            child1.height = parent.height - split;
        }

        splitQueue.push(child0);
        splitQueue.push(child1);
    }

    rooms.reserve(rooms.size() + splitQueue.size());
    while (!splitQueue.empty())
    {
        rooms.push_back(splitQueue.top());
        splitQueue.pop();
    }

    // Sort rooms by min/max coordinates along one axis at a time and find overlapping portals
    std::vector<Portal> portals;
    std::vector<RoomPortalRecord> roomPortalRecords;
    std::vector<int> portalOverlap;

    std::vector<uint32_t> minIndices(rooms.size());
    std::iota(minIndices.begin(), minIndices.end(), 0);
    std::vector<uint32_t> maxIndices = minIndices;

    // X direction
    std::sort(minIndices.begin(), minIndices.end(), [&](const auto left, const auto right) {
            return rooms[left].x < rooms[right].x;
        });
    std::sort(maxIndices.begin(), maxIndices.end(), [&](const auto left, const auto right) {
            return rooms[left].x + rooms[left].width < rooms[right].x + rooms[right].width;
        });
    for (uint32_t mini = 0, maxi = 0; mini < minIndices.size() && maxi < maxIndices.size(); ++mini)
    {
        const auto& room0 = rooms[minIndices[mini]];
        for (; maxi < maxIndices.size() && rooms[maxIndices[maxi]].x + rooms[maxIndices[maxi]].width < room0.x; ++maxi);
        for (uint32_t tmaxi = maxi; tmaxi < maxIndices.size() && rooms[maxIndices[tmaxi]].x + rooms[maxIndices[tmaxi]].width == room0.x; ++tmaxi)
        {
            const auto& room1 = rooms[maxIndices[tmaxi]];
            int overlap = static_cast<int>(std::min(room0.y + room0.height, room1.y + room1.height)) - static_cast<int>(std::max(room0.y, room1.y));
            if (overlap >= static_cast<int>(params.minPortalOverlap))
            {
                roomPortalRecords.push_back({ { minIndices[mini], maxIndices[tmaxi] }, static_cast<uint32_t>(portals.size()) });
                roomPortalRecords.push_back({ { maxIndices[tmaxi], minIndices[mini] }, static_cast<uint32_t>(portals.size()) });
                portals.push_back(Portal {
                        .rooms = { minIndices[mini], maxIndices[tmaxi] },
                        .x = room0.x,
                        .y = (std::max(room0.y, room1.y) + std::min(room0.y + room0.height, room1.y + room1.height)) / 2,
                    });
                portalOverlap.push_back(overlap);
            }
        }
    }

    // Y direction
    std::sort(minIndices.begin(), minIndices.end(), [&](const auto left, const auto right) {
            return rooms[left].y < rooms[right].y;
        });
    std::sort(maxIndices.begin(), maxIndices.end(), [&](const auto left, const auto right) {
            return rooms[left].y + rooms[left].height < rooms[right].y + rooms[right].height;
        });
    for (uint32_t mini = 0, maxi = 0; mini < minIndices.size() && maxi < maxIndices.size(); ++mini)
    {
        const auto& room0 = rooms[minIndices[mini]];
        for (; maxi < maxIndices.size() && rooms[maxIndices[maxi]].y + rooms[maxIndices[maxi]].height < room0.y; ++maxi);
        for (uint32_t tmaxi = maxi; tmaxi < maxIndices.size() && rooms[maxIndices[tmaxi]].y + rooms[maxIndices[tmaxi]].height == room0.y; ++tmaxi)
        {
            const auto& room1 = rooms[maxIndices[tmaxi]];
            int overlap = static_cast<int>(std::min(room0.x + room0.width, room1.x + room1.width)) - static_cast<int>(std::max(room0.x, room1.x));
            if (overlap >= static_cast<int>(params.minPortalOverlap))
            {
                roomPortalRecords.push_back({ { minIndices[mini], maxIndices[tmaxi] }, static_cast<uint32_t>(portals.size()) });
                roomPortalRecords.push_back({ { maxIndices[tmaxi], minIndices[mini] }, static_cast<uint32_t>(portals.size()) });
                portals.push_back(Portal {
                        .rooms = { minIndices[mini], maxIndices[tmaxi] },
                        .x = (std::max(room0.x, room1.x) + std::min(room0.x + room0.width, room1.x + room1.width)) / 2,
                        .y = room0.y,
                    });
                portalOverlap.push_back(overlap);
            }
        }
    }

    // Create indices into portalRecords for the start and end of the range of portal records per room
    std::sort(roomPortalRecords.begin(), roomPortalRecords.end(), [](const auto& left, const auto& right) {
            return left.rooms[0] < right.rooms[0];
        });
    for (uint32_t roomi = 0, portali = 0; roomi < rooms.size(); ++roomi)
    {
        for (; portali < roomPortalRecords.size() && roomPortalRecords[portali].rooms[0] < roomi; ++portali);
        rooms[roomi].portalRecordsRange.start = portali;
        for (; portali < roomPortalRecords.size() && roomPortalRecords[portali].rooms[0] == roomi; ++portali);
        rooms[roomi].portalRecordsRange.end = portali;
    }

    // Select rooms to add into the final layout by choosing a room randomly then iteratively selecting neighbors based on the smallest overlap > threshold that connects a room inside to one outside
    const auto comparePortalIndicesForSelection = [&](const auto left, const auto right) {
        return portalOverlap[left] > portalOverlap[right];
    };
    std::priority_queue<uint32_t, std::vector<uint32_t>, decltype(comparePortalIndicesForSelection)> portalIndexSelectionQueue(comparePortalIndicesForSelection);

    std::vector<uint32_t> roomSelectedIndices(rooms.size(), U32_MAX);
    std::vector<Room> selectedRooms;

    const auto selectRoom = [&](const uint32_t index) {
        for (uint32_t i = rooms[index].portalRecordsRange.start; i < rooms[index].portalRecordsRange.end; ++i)
        {
            portalIndexSelectionQueue.push(roomPortalRecords[i].portal);
        }
        roomSelectedIndices[index] = selectedRooms.size();
        selectedRooms.push_back(rooms[index]);
    };

    selectRoom(prng() % rooms.size());

    std::vector<Portal> selectedPortals;
    std::vector<RoomPortalRecord> selectedPortalRecords;

    while (!portalIndexSelectionQueue.empty() && selectedRooms.size() < params.targetRoomCount)
    {
        uint32_t portalIndex = portalIndexSelectionQueue.top();
        portalIndexSelectionQueue.pop();
        const auto& portal = portals[portalIndex];
        if ((roomSelectedIndices[portal.rooms[0]] < selectedRooms.size()) == (roomSelectedIndices[portal.rooms[1]] < selectedRooms.size()))
        {
            continue;
        }

        selectRoom(roomSelectedIndices[portal.rooms[0]] < selectedRooms.size() ? portal.rooms[1] : portal.rooms[0]);

        selectedPortalRecords.push_back({ { roomSelectedIndices[portal.rooms[0]], roomSelectedIndices[portal.rooms[1]] }, static_cast<uint32_t>(selectedPortals.size()) });
        selectedPortalRecords.push_back({ { roomSelectedIndices[portal.rooms[1]], roomSelectedIndices[portal.rooms[0]] }, static_cast<uint32_t>(selectedPortals.size()) });
        selectedPortals.push_back(Portal {
                    .rooms = { roomSelectedIndices[portal.rooms[0]], roomSelectedIndices[portal.rooms[1]] },
                    .x = portal.x,
                    .y = portal.y,
                });
    }

    // Condense to selected rooms and portals and recalculate portal indices based on only selected
    rooms = std::move(selectedRooms);
    portals = std::move(selectedPortals);
    roomPortalRecords = std::move(selectedPortalRecords);

    std::sort(roomPortalRecords.begin(), roomPortalRecords.end(), [](const auto& left, const auto& right) {
            return left.rooms[0] < right.rooms[0];
        });
    for (uint32_t roomi = 0, portali = 0; roomi < rooms.size(); ++roomi)
    {
        for (; portali < roomPortalRecords.size() && roomPortalRecords[portali].rooms[0] < roomi; ++portali);
        rooms[roomi].portalRecordsRange.start = portali;
        for (; portali < roomPortalRecords.size() && roomPortalRecords[portali].rooms[0] == roomi; ++portali);
        rooms[roomi].portalRecordsRange.end = portali;
    }

    const auto wallOfPortal = [](const Room& room, const Portal& portal) {
        if (portal.y == room.y) return 1;
        if (portal.x == room.x + room.width) return 2;
        if (portal.y == room.y + room.height) return 3;
        return 0;
    };

    for (auto& room : rooms)
    {
        std::sort(roomPortalRecords.begin() + room.portalRecordsRange.start, roomPortalRecords.begin() + room.portalRecordsRange.end,
                [&](const auto& left, const auto& right) {
                    const auto& lportal = portals[left.portal], &rportal = portals[right.portal];
                    int lwall = wallOfPortal(room, lportal), rwall = wallOfPortal(room, rportal);
                    if (lwall == rwall)
                    {
                        if (lwall == 0) return lportal.y > rportal.y;
                        if (lwall == 1) return lportal.x < rportal.x;
                        if (lwall == 2) return lportal.y < rportal.y;
                        return lportal.x > rportal.x;
                    }
                    return lwall < rwall;
                });

        for (uint32_t wall = 0, recordIndex = room.portalRecordsRange.start; wall < 4; ++wall)
        {
            room.wallStartPortalRecord.i[wall] = recordIndex;
            for (; recordIndex < room.portalRecordsRange.end; ++recordIndex)
            {
                uint32_t pwall = wallOfPortal(room, portals[roomPortalRecords[recordIndex].portal]);
                if (pwall > wall) break;
            }
        }
    }

    return Dungeon {
        .rooms = std::move(rooms),
        .portals = std::move(portals),
        .roomPortalRecords = std::move(roomPortalRecords),
    };
}

eng::GeometryDescription Dungeon::createGeometry(const float wallHeight, const float doorWidth, const float wallThickness, const float doorHeight) const
{
    eng::GeometryDescription geometry;

    std::queue<uint32_t> wallNodeIndices;

    for (const auto& room : rooms)
    {
        // Floor
        geometry.indices.push_back(geometry.positions.size());
        geometry.indices.push_back(geometry.positions.size() + 1);
        geometry.indices.push_back(geometry.positions.size() + 2);
        geometry.indices.push_back(geometry.positions.size() + 2);
        geometry.indices.push_back(geometry.positions.size() + 1);
        geometry.indices.push_back(geometry.positions.size() + 3);

        geometry.positions.push_back(glm::vec3(room.x, 0, room.y));
        geometry.texCoords.push_back(glm::vec2(room.x, room.y));
        geometry.normals.push_back(glm::vec3(0, 1, 0));

        geometry.positions.push_back(glm::vec3(room.x, 0, room.y + room.height));
        geometry.texCoords.push_back(glm::vec2(room.x, room.y + room.height));
        geometry.normals.push_back(glm::vec3(0, 1, 0));

        geometry.positions.push_back(glm::vec3(room.x + room.width, 0, room.y));
        geometry.texCoords.push_back(glm::vec2(room.x + room.width, room.y));
        geometry.normals.push_back(glm::vec3(0, 1, 0));

        geometry.positions.push_back(glm::vec3(room.x + room.width, 0, room.y + room.height));
        geometry.texCoords.push_back(glm::vec2(room.x + room.width, room.y + room.height));
        geometry.normals.push_back(glm::vec3(0, 1, 0));

        // Walls
        for (uint32_t i = 0; i < 4; ++i)
        {
            glm::vec3 points[2];
            glm::vec3 dir, normal;

            if (i == 0)
            {
                points[0] = glm::vec3(room.x + 0.5f * wallThickness, 0, room.y + room.height - 0.5f * wallThickness);
                points[1] = glm::vec3(room.x + 0.5f * wallThickness, 0, room.y + 0.5f * wallThickness);
                dir = glm::vec3(0, 0, -1);
                normal = glm::vec3(1, 0, 0);
            }
            else if (i == 1)
            {
                points[0] = glm::vec3(room.x + 0.5f * wallThickness, 0, room.y + 0.5f * wallThickness);
                points[1] = glm::vec3(room.x + room.width - 0.5f * wallThickness, 0, room.y + 0.5f * wallThickness);
                dir = glm::vec3(1, 0, 0);
                normal = glm::vec3(0, 0, 1);
            }
            else if (i == 2)
            {
                points[0] = glm::vec3(room.x + room.width - 0.5f * wallThickness, 0, room.y + 0.5f * wallThickness);
                points[1] = glm::vec3(room.x + room.width - 0.5f * wallThickness, 0, room.y + room.height - 0.5f * wallThickness);
                dir = glm::vec3(0, 0, 1);
                normal = glm::vec3(-1, 0, 0);
            }
            else
            {
                points[0] = glm::vec3(room.x + room.width - 0.5f * wallThickness, 0, room.y + room.height - 0.5f * wallThickness);
                points[1] = glm::vec3(room.x + 0.5f * wallThickness, 0, room.y + room.height - 0.5f * wallThickness);
                dir = glm::vec3(-1, 0, 0);
                normal = glm::vec3(0, 0, -1);
            }

            float tc[] = { glm::dot(dir, points[0]), glm::dot(dir, points[1]) };

            geometry.indices.push_back(geometry.positions.size());
            geometry.indices.push_back(geometry.positions.size() + 1);
            geometry.indices.push_back(geometry.positions.size() + 2);
            geometry.indices.push_back(geometry.positions.size() + 2);
            geometry.indices.push_back(geometry.positions.size() + 1);
            geometry.indices.push_back(geometry.positions.size() + 3);

            geometry.positions.push_back(points[0]);
            geometry.positions.push_back(points[0] + glm::vec3(0, wallHeight, 0));
            geometry.texCoords.push_back(glm::vec2(tc[0], 0));
            geometry.texCoords.push_back(glm::vec2(tc[0], wallHeight));
            geometry.normals.push_back(normal);
            geometry.normals.push_back(normal);

            for (uint32_t j = room.wallStartPortalRecord.i[i]; j < (i < 3 ? room.wallStartPortalRecord.i[i + 1] : room.portalRecordsRange.end); ++j)
            {
                const auto& portal = portals[roomPortalRecords[j].portal];

                glm::vec3 pos(portal.x, 0, portal.y);
                float ltc = glm::dot(dir, pos);

                // finish wall segment
                //
                geometry.positions.push_back(pos + normal * 0.5f * wallThickness - 0.5f * doorWidth * dir);
                geometry.positions.push_back(pos + normal * 0.5f * wallThickness - 0.5f * doorWidth * dir + glm::vec3(0, wallHeight, 0));
                geometry.texCoords.push_back(glm::vec2(ltc - 0.5 * doorWidth, 0));
                geometry.texCoords.push_back(glm::vec2(ltc - 0.5 * doorWidth, wallHeight));
                geometry.normals.push_back(normal);
                geometry.normals.push_back(normal);

                // interior face 1

                geometry.indices.push_back(geometry.positions.size());
                geometry.indices.push_back(geometry.positions.size() + 1);
                geometry.indices.push_back(geometry.positions.size() + 2);
                geometry.indices.push_back(geometry.positions.size() + 2);
                geometry.indices.push_back(geometry.positions.size() + 1);
                geometry.indices.push_back(geometry.positions.size() + 3);

                geometry.positions.push_back(pos + normal * 0.5f * wallThickness - 0.5f * doorWidth * dir);
                geometry.positions.push_back(pos + normal * 0.5f * wallThickness - 0.5f * doorWidth * dir + glm::vec3(0, wallHeight, 0));
                geometry.texCoords.push_back(glm::vec2(ltc - 0.5 * doorWidth, 0));
                geometry.texCoords.push_back(glm::vec2(ltc - 0.5 * doorWidth, wallHeight));
                geometry.normals.push_back(dir);
                geometry.normals.push_back(dir);

                geometry.positions.push_back(pos - 0.5f * doorWidth * dir);
                geometry.positions.push_back(pos - 0.5f * doorWidth * dir + glm::vec3(0, wallHeight, 0));
                geometry.texCoords.push_back(glm::vec2(ltc - 0.5 * doorWidth + 0.5f * wallThickness, 0));
                geometry.texCoords.push_back(glm::vec2(ltc - 0.5 * doorWidth + 0.5f * wallThickness, wallHeight));
                geometry.normals.push_back(dir);
                geometry.normals.push_back(dir);

                // interior face 2

                geometry.indices.push_back(geometry.positions.size());
                geometry.indices.push_back(geometry.positions.size() + 1);
                geometry.indices.push_back(geometry.positions.size() + 2);
                geometry.indices.push_back(geometry.positions.size() + 2);
                geometry.indices.push_back(geometry.positions.size() + 1);
                geometry.indices.push_back(geometry.positions.size() + 3);

                geometry.positions.push_back(pos + 0.5f * doorWidth * dir);
                geometry.positions.push_back(pos + 0.5f * doorWidth * dir + glm::vec3(0, wallHeight, 0));
                geometry.texCoords.push_back(glm::vec2(ltc + 0.5 * doorWidth - 0.5f * wallThickness, 0));
                geometry.texCoords.push_back(glm::vec2(ltc + 0.5 * doorWidth - 0.5f * wallThickness, wallHeight));
                geometry.normals.push_back(-dir);
                geometry.normals.push_back(-dir);

                geometry.positions.push_back(pos + normal * 0.5f * wallThickness + 0.5f * doorWidth * dir);
                geometry.positions.push_back(pos + normal * 0.5f * wallThickness + 0.5f * doorWidth * dir + glm::vec3(0, wallHeight, 0));
                geometry.texCoords.push_back(glm::vec2(ltc + 0.5 * doorWidth, 0));
                geometry.texCoords.push_back(glm::vec2(ltc + 0.5 * doorWidth, wallHeight));
                geometry.normals.push_back(-dir);
                geometry.normals.push_back(-dir);

                /// upper portion

                geometry.indices.push_back(geometry.positions.size());
                geometry.indices.push_back(geometry.positions.size() + 1);
                geometry.indices.push_back(geometry.positions.size() + 2);
                geometry.indices.push_back(geometry.positions.size() + 2);
                geometry.indices.push_back(geometry.positions.size() + 1);
                geometry.indices.push_back(geometry.positions.size() + 3);

                geometry.positions.push_back(pos + normal * 0.5f * wallThickness - 0.5f * doorWidth * dir + glm::vec3(0, doorHeight, 0));
                geometry.positions.push_back(pos + normal * 0.5f * wallThickness - 0.5f * doorWidth * dir + glm::vec3(0, wallHeight, 0));
                geometry.texCoords.push_back(glm::vec2(ltc - 0.5 * doorWidth, doorHeight));
                geometry.texCoords.push_back(glm::vec2(ltc - 0.5 * doorWidth, wallHeight));
                geometry.normals.push_back(normal);
                geometry.normals.push_back(normal);

                geometry.positions.push_back(pos + normal * 0.5f * wallThickness + 0.5f * doorWidth * dir + glm::vec3(0, doorHeight, 0));
                geometry.positions.push_back(pos + normal * 0.5f * wallThickness + 0.5f * doorWidth * dir + glm::vec3(0, wallHeight, 0));
                geometry.texCoords.push_back(glm::vec2(ltc + 0.5 * doorWidth, doorHeight));
                geometry.texCoords.push_back(glm::vec2(ltc + 0.5 * doorWidth, wallHeight));
                geometry.normals.push_back(normal);
                geometry.normals.push_back(normal);

                // next wall segment

                geometry.indices.push_back(geometry.positions.size());
                geometry.indices.push_back(geometry.positions.size() + 1);
                geometry.indices.push_back(geometry.positions.size() + 2);
                geometry.indices.push_back(geometry.positions.size() + 2);
                geometry.indices.push_back(geometry.positions.size() + 1);
                geometry.indices.push_back(geometry.positions.size() + 3);

                geometry.positions.push_back(pos + normal * 0.5f * wallThickness + 0.5f * doorWidth * dir);
                geometry.positions.push_back(pos + normal * 0.5f * wallThickness + 0.5f * doorWidth * dir + glm::vec3(0, wallHeight, 0));
                geometry.texCoords.push_back(glm::vec2(ltc + 0.5 * doorWidth, 0));
                geometry.texCoords.push_back(glm::vec2(ltc + 0.5 * doorWidth, wallHeight));
                geometry.normals.push_back(normal);
                geometry.normals.push_back(normal);
            }

            geometry.positions.push_back(points[1]);
            geometry.positions.push_back(points[1] + glm::vec3(0, wallHeight, 0));
            geometry.texCoords.push_back(glm::vec2(tc[1], 0));
            geometry.texCoords.push_back(glm::vec2(tc[1], wallHeight));
            geometry.normals.push_back(normal);
            geometry.normals.push_back(normal);


            // tops of walls
            geometry.indices.push_back(geometry.positions.size());
            geometry.indices.push_back(geometry.positions.size() + 1);
            geometry.indices.push_back(geometry.positions.size() + 2);
            geometry.indices.push_back(geometry.positions.size() + 2);
            geometry.indices.push_back(geometry.positions.size() + 1);
            geometry.indices.push_back(geometry.positions.size() + 3);

            geometry.positions.push_back(points[0] + glm::vec3(0, wallHeight, 0));
            geometry.positions.push_back(points[0] + glm::vec3(0, wallHeight, 0) - 0.5f * wallThickness * normal);
            geometry.texCoords.push_back(glm::vec2(tc[0], wallHeight));
            geometry.texCoords.push_back(glm::vec2(tc[0], wallHeight + 0.5f * wallThickness));
            geometry.normals.push_back(glm::vec3(0, 1, 0));
            geometry.normals.push_back(glm::vec3(0, 1, 0));

            geometry.positions.push_back(points[1] + glm::vec3(0, wallHeight, 0));
            geometry.positions.push_back(points[1] + glm::vec3(0, wallHeight, 0) - 0.5f * wallThickness * normal);
            geometry.texCoords.push_back(glm::vec2(tc[1], wallHeight));
            geometry.texCoords.push_back(glm::vec2(tc[1], wallHeight + 0.5f * wallThickness));
            geometry.normals.push_back(glm::vec3(0, 1, 0));
            geometry.normals.push_back(glm::vec3(0, 1, 0));

        }
    }

    return geometry;
}

void Dungeon::createPhysicsBodies(const float wallHeight, const float doorWidth, const float wallThickness,
        std::vector<JPH::BodyID>& bodies, std::vector<JPH::Ref<JPH::Shape>>& shapeRefs, JPH::PhysicsSystem& physicsSystem) const
{
    std::queue<uint32_t> wallNodeIndices;

    for (const auto& room : rooms)
    {
        bodies.push_back(physicsSystem.GetBodyInterface().CreateAndAddBody(
                    JPH::BodyCreationSettings(shapeRefs.emplace_back(new JPH::BoxShape(JPH::Vec3(room.width * 0.5, 0.5, room.height * 0.5))),
                        JPH::Vec3(room.x + 0.5 * room.width, - 0.5, room.y + 0.5 * room.height), JPH::Quat::sIdentity(), JPH::EMotionType::Static, 0),
                    JPH::EActivation::DontActivate));

        for (uint32_t i = 0; i < 4; ++i)
        {
            glm::vec3 points[2];
            glm::vec3 dir, normal;

            if (i == 0)
            {
                points[0] = glm::vec3(room.x, 0, room.y + room.height);
                points[1] = glm::vec3(room.x, 0, room.y);
                dir = glm::vec3(0, 0, -1);
                normal = glm::vec3(1, 0, 0);
            }
            else if (i == 1)
            {
                points[0] = glm::vec3(room.x, 0, room.y);
                points[1] = glm::vec3(room.x + room.width, 0, room.y);
                dir = glm::vec3(1, 0, 0);
                normal = glm::vec3(0, 0, 1);
            }
            else if (i == 2)
            {
                points[0] = glm::vec3(room.x + room.width, 0, room.y);
                points[1] = glm::vec3(room.x + room.width, 0, room.y + room.height);
                dir = glm::vec3(0, 0, 1);
                normal = glm::vec3(-1, 0, 0);
            }
            else
            {
                points[0] = glm::vec3(room.x + room.width, 0, room.y + room.height);
                points[1] = glm::vec3(room.x, 0, room.y + room.height);
                dir = glm::vec3(-1, 0, 0);
                normal = glm::vec3(0, 0, -1);
            }

            glm::vec3 lastPoint = points[0];

            for (uint32_t j = room.wallStartPortalRecord.i[i]; j < (i < 3 ? room.wallStartPortalRecord.i[i + 1] : room.portalRecordsRange.end); ++j)
            {
                const auto& portal = portals[roomPortalRecords[j].portal];

                glm::vec3 pos(portal.x, 0, portal.y);
                pos -= 0.5f * doorWidth * dir;

                bodies.push_back(physicsSystem.GetBodyInterface().CreateAndAddBody(
                            JPH::BodyCreationSettings(shapeRefs.emplace_back(new JPH::BoxShape(glm_to_jph(glm::abs(0.5f * (pos - lastPoint + 0.5f * wallThickness * normal + glm::vec3(0, wallHeight, 0)))))),
                                glm_to_jph(0.5f * (pos + lastPoint + 0.5f * wallThickness * normal + glm::vec3(0, wallHeight, 0))), JPH::Quat::sIdentity(), JPH::EMotionType::Static, 0),
                            JPH::EActivation::DontActivate));

                lastPoint = pos + doorWidth * dir;
            }

            bodies.push_back(physicsSystem.GetBodyInterface().CreateAndAddBody(
                        JPH::BodyCreationSettings(shapeRefs.emplace_back(new JPH::BoxShape(glm_to_jph(glm::abs(0.5f * (points[1] - lastPoint + 0.5f * wallThickness * normal + glm::vec3(0, wallHeight, 0)))))),
                            glm_to_jph(0.5f * (points[1] + lastPoint + 0.5f * wallThickness * normal + glm::vec3(0, wallHeight, 0))), JPH::Quat::sIdentity(), JPH::EMotionType::Static, 0),
                        JPH::EActivation::DontActivate));
        }
    }
}

