#include "dungeon.hpp"

#include <numeric>
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

        if (std::max(parent.width, parent.height) <= params.minSplitDimension)
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

    uint32_t playerStartRoom = rooms.size();
    uint32_t playerStartRoomSize = 0;
    for (uint32_t i = 0; i < rooms.size(); ++i)
    {
        if (rooms[i].portalRecordsRange.end - rooms[i].portalRecordsRange.start == 1)
        {
            uint32_t size = rooms[i].width * rooms[i].height;
            if (playerStartRoom == rooms.size() || size < playerStartRoomSize)
            {
                playerStartRoom = i;
                playerStartRoomSize = size;
            }
        }
    }

    // populate
    std::vector<Obstacle> obstacles;
    std::vector<std::pair<uint32_t, uint32_t>> spawnPoints;
    std::vector<int> spaces;
    std::pair<uint32_t, uint32_t> playerSpawn = { 0, 0 };
    for (const auto& room : rooms)
    {
        if (room.width > 2 && room.height > 2)
        {
            uint32_t availableTiles = (room.width - 2) * (room.height - 2);
            spaces.assign(availableTiles, 0);
            uint32_t countObstacles = 1 + availableTiles / 20 + (prng() % (1 + availableTiles / 5));
            for (uint32_t i = 0; i < countObstacles; ++i)
            {
                uint32_t x = prng() % (room.width - 2);
                uint32_t y = prng() % (room.height - 2);
                if (!spaces[y * (room.width - 2) + x])
                {
                    spaces[y * (room.width - 2) + x] = 1;
                    obstacles.push_back(Obstacle { .x = room.x + 1 + x, .y = room.y + 1 + y, .width = 1, .height = 1 });
                }
            }

            if (&room == &rooms[playerStartRoom])
            {
                playerSpawn = { room.x + 1, room.y + 1 };
                for (int i = 0; i < 10000; ++i)
                {
                    uint32_t x = prng() % (room.width - 2);
                    uint32_t y = prng() % (room.height - 2);
                    if (!spaces[y * (room.width - 2) + x])
                    {
                        spaces[y * (room.width - 2) + x] = 1;
                        playerSpawn = { room.x + 1 + x, room.y + 1 + y };
                        break;
                    }
                }
            }
            else
            {
                uint32_t countEnemies = 1 + availableTiles / 10 + (prng() % (1 + availableTiles / 8));
                for (uint32_t i = 0; i < countEnemies; ++i)
                {
                    uint32_t x = prng() % (room.width - 2);
                    uint32_t y = prng() % (room.height - 2);
                    if (!spaces[y * (room.width - 2) + x])
                    {
                        spaces[y * (room.width - 2) + x] = 1;
                        spawnPoints.push_back({ room.x + 1 + x, room.y + 1 + y });
                    }
                }
            }
        }
        else if (&room == &rooms[playerStartRoom])
        {
            playerSpawn = { room.x + room.width / 2, room.y + room.height / 2 };
        }
    }

    return Dungeon {
        .rooms = std::move(rooms),
        .portals = std::move(portals),
        .roomPortalRecords = std::move(roomPortalRecords),
        .obstacles = std::move(obstacles),
        .spawnPoints = std::move(spawnPoints),
        .playerSpawn = std::move(playerSpawn),
    };
}

Dungeon::Geometry Dungeon::createGeometry(const float wallHeight, const float doorWidth, const float wallThickness, const float doorHeight, const float obstacleHeight) const
{
    Geometry geometry;

    std::queue<uint32_t> wallNodeIndices;

    for (const auto& room : rooms)
    {
        // Floor
        geometry.floor.indices.push_back(geometry.floor.positions.size());
        geometry.floor.indices.push_back(geometry.floor.positions.size() + 1);
        geometry.floor.indices.push_back(geometry.floor.positions.size() + 2);
        geometry.floor.indices.push_back(geometry.floor.positions.size() + 2);
        geometry.floor.indices.push_back(geometry.floor.positions.size() + 1);
        geometry.floor.indices.push_back(geometry.floor.positions.size() + 3);

        geometry.floor.positions.push_back(glm::vec3(room.x, 0, room.y));
        geometry.floor.texCoords.push_back(glm::vec2(room.x, room.y));
        geometry.floor.normals.push_back(glm::vec3(0, 1, 0));

        geometry.floor.positions.push_back(glm::vec3(room.x, 0, room.y + room.height));
        geometry.floor.texCoords.push_back(glm::vec2(room.x, room.y + room.height));
        geometry.floor.normals.push_back(glm::vec3(0, 1, 0));

        geometry.floor.positions.push_back(glm::vec3(room.x + room.width, 0, room.y));
        geometry.floor.texCoords.push_back(glm::vec2(room.x + room.width, room.y));
        geometry.floor.normals.push_back(glm::vec3(0, 1, 0));

        geometry.floor.positions.push_back(glm::vec3(room.x + room.width, 0, room.y + room.height));
        geometry.floor.texCoords.push_back(glm::vec2(room.x + room.width, room.y + room.height));
        geometry.floor.normals.push_back(glm::vec3(0, 1, 0));

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

            geometry.walls.indices.push_back(geometry.walls.positions.size());
            geometry.walls.indices.push_back(geometry.walls.positions.size() + 1);
            geometry.walls.indices.push_back(geometry.walls.positions.size() + 2);
            geometry.walls.indices.push_back(geometry.walls.positions.size() + 2);
            geometry.walls.indices.push_back(geometry.walls.positions.size() + 1);
            geometry.walls.indices.push_back(geometry.walls.positions.size() + 3);

            geometry.walls.positions.push_back(points[0]);
            geometry.walls.positions.push_back(points[0] + glm::vec3(0, wallHeight, 0));
            geometry.walls.texCoords.push_back(glm::vec2(tc[0], 0));
            geometry.walls.texCoords.push_back(glm::vec2(tc[0], wallHeight));
            geometry.walls.normals.push_back(normal);
            geometry.walls.normals.push_back(normal);

            for (uint32_t j = room.wallStartPortalRecord.i[i]; j < (i < 3 ? room.wallStartPortalRecord.i[i + 1] : room.portalRecordsRange.end); ++j)
            {
                const auto& portal = portals[roomPortalRecords[j].portal];

                glm::vec3 pos(portal.x, 0, portal.y);
                float ltc = glm::dot(dir, pos);

                // finish wall segment
                geometry.walls.positions.push_back(pos + normal * 0.5f * wallThickness - 0.5f * doorWidth * dir);
                geometry.walls.positions.push_back(pos + normal * 0.5f * wallThickness - 0.5f * doorWidth * dir + glm::vec3(0, wallHeight, 0));
                geometry.walls.texCoords.push_back(glm::vec2(ltc - 0.5 * doorWidth, 0));
                geometry.walls.texCoords.push_back(glm::vec2(ltc - 0.5 * doorWidth, wallHeight));
                geometry.walls.normals.push_back(normal);
                geometry.walls.normals.push_back(normal);

                // interior face 1
                geometry.walls.indices.push_back(geometry.walls.positions.size());
                geometry.walls.indices.push_back(geometry.walls.positions.size() + 1);
                geometry.walls.indices.push_back(geometry.walls.positions.size() + 2);
                geometry.walls.indices.push_back(geometry.walls.positions.size() + 2);
                geometry.walls.indices.push_back(geometry.walls.positions.size() + 1);
                geometry.walls.indices.push_back(geometry.walls.positions.size() + 3);

                geometry.walls.positions.push_back(pos + normal * 0.5f * wallThickness - 0.5f * doorWidth * dir);
                geometry.walls.positions.push_back(pos + normal * 0.5f * wallThickness - 0.5f * doorWidth * dir + glm::vec3(0, wallHeight, 0));
                geometry.walls.texCoords.push_back(glm::vec2(ltc - 0.5 * doorWidth, 0));
                geometry.walls.texCoords.push_back(glm::vec2(ltc - 0.5 * doorWidth, wallHeight));
                geometry.walls.normals.push_back(dir);
                geometry.walls.normals.push_back(dir);

                geometry.walls.positions.push_back(pos - 0.5f * doorWidth * dir);
                geometry.walls.positions.push_back(pos - 0.5f * doorWidth * dir + glm::vec3(0, wallHeight, 0));
                geometry.walls.texCoords.push_back(glm::vec2(ltc - 0.5 * doorWidth + 0.5f * wallThickness, 0));
                geometry.walls.texCoords.push_back(glm::vec2(ltc - 0.5 * doorWidth + 0.5f * wallThickness, wallHeight));
                geometry.walls.normals.push_back(dir);
                geometry.walls.normals.push_back(dir);

                // interior face 2
                geometry.walls.indices.push_back(geometry.walls.positions.size());
                geometry.walls.indices.push_back(geometry.walls.positions.size() + 1);
                geometry.walls.indices.push_back(geometry.walls.positions.size() + 2);
                geometry.walls.indices.push_back(geometry.walls.positions.size() + 2);
                geometry.walls.indices.push_back(geometry.walls.positions.size() + 1);
                geometry.walls.indices.push_back(geometry.walls.positions.size() + 3);

                geometry.walls.positions.push_back(pos + 0.5f * doorWidth * dir);
                geometry.walls.positions.push_back(pos + 0.5f * doorWidth * dir + glm::vec3(0, wallHeight, 0));
                geometry.walls.texCoords.push_back(glm::vec2(ltc + 0.5 * doorWidth - 0.5f * wallThickness, 0));
                geometry.walls.texCoords.push_back(glm::vec2(ltc + 0.5 * doorWidth - 0.5f * wallThickness, wallHeight));
                geometry.walls.normals.push_back(-dir);
                geometry.walls.normals.push_back(-dir);

                geometry.walls.positions.push_back(pos + normal * 0.5f * wallThickness + 0.5f * doorWidth * dir);
                geometry.walls.positions.push_back(pos + normal * 0.5f * wallThickness + 0.5f * doorWidth * dir + glm::vec3(0, wallHeight, 0));
                geometry.walls.texCoords.push_back(glm::vec2(ltc + 0.5 * doorWidth, 0));
                geometry.walls.texCoords.push_back(glm::vec2(ltc + 0.5 * doorWidth, wallHeight));
                geometry.walls.normals.push_back(-dir);
                geometry.walls.normals.push_back(-dir);

                // upper portion
                geometry.walls.indices.push_back(geometry.walls.positions.size());
                geometry.walls.indices.push_back(geometry.walls.positions.size() + 1);
                geometry.walls.indices.push_back(geometry.walls.positions.size() + 2);
                geometry.walls.indices.push_back(geometry.walls.positions.size() + 2);
                geometry.walls.indices.push_back(geometry.walls.positions.size() + 1);
                geometry.walls.indices.push_back(geometry.walls.positions.size() + 3);

                geometry.walls.positions.push_back(pos + normal * 0.5f * wallThickness - 0.5f * doorWidth * dir + glm::vec3(0, doorHeight, 0));
                geometry.walls.positions.push_back(pos + normal * 0.5f * wallThickness - 0.5f * doorWidth * dir + glm::vec3(0, wallHeight, 0));
                geometry.walls.texCoords.push_back(glm::vec2(ltc - 0.5 * doorWidth, doorHeight));
                geometry.walls.texCoords.push_back(glm::vec2(ltc - 0.5 * doorWidth, wallHeight));
                geometry.walls.normals.push_back(normal);
                geometry.walls.normals.push_back(normal);

                geometry.walls.positions.push_back(pos + normal * 0.5f * wallThickness + 0.5f * doorWidth * dir + glm::vec3(0, doorHeight, 0));
                geometry.walls.positions.push_back(pos + normal * 0.5f * wallThickness + 0.5f * doorWidth * dir + glm::vec3(0, wallHeight, 0));
                geometry.walls.texCoords.push_back(glm::vec2(ltc + 0.5 * doorWidth, doorHeight));
                geometry.walls.texCoords.push_back(glm::vec2(ltc + 0.5 * doorWidth, wallHeight));
                geometry.walls.normals.push_back(normal);
                geometry.walls.normals.push_back(normal);

                // next wall segment
                geometry.walls.indices.push_back(geometry.walls.positions.size());
                geometry.walls.indices.push_back(geometry.walls.positions.size() + 1);
                geometry.walls.indices.push_back(geometry.walls.positions.size() + 2);
                geometry.walls.indices.push_back(geometry.walls.positions.size() + 2);
                geometry.walls.indices.push_back(geometry.walls.positions.size() + 1);
                geometry.walls.indices.push_back(geometry.walls.positions.size() + 3);

                geometry.walls.positions.push_back(pos + normal * 0.5f * wallThickness + 0.5f * doorWidth * dir);
                geometry.walls.positions.push_back(pos + normal * 0.5f * wallThickness + 0.5f * doorWidth * dir + glm::vec3(0, wallHeight, 0));
                geometry.walls.texCoords.push_back(glm::vec2(ltc + 0.5 * doorWidth, 0));
                geometry.walls.texCoords.push_back(glm::vec2(ltc + 0.5 * doorWidth, wallHeight));
                geometry.walls.normals.push_back(normal);
                geometry.walls.normals.push_back(normal);
            }

            geometry.walls.positions.push_back(points[1]);
            geometry.walls.positions.push_back(points[1] + glm::vec3(0, wallHeight, 0));
            geometry.walls.texCoords.push_back(glm::vec2(tc[1], 0));
            geometry.walls.texCoords.push_back(glm::vec2(tc[1], wallHeight));
            geometry.walls.normals.push_back(normal);
            geometry.walls.normals.push_back(normal);

            // tops of walls
            geometry.walls.indices.push_back(geometry.walls.positions.size());
            geometry.walls.indices.push_back(geometry.walls.positions.size() + 1);
            geometry.walls.indices.push_back(geometry.walls.positions.size() + 2);
            geometry.walls.indices.push_back(geometry.walls.positions.size() + 2);
            geometry.walls.indices.push_back(geometry.walls.positions.size() + 1);
            geometry.walls.indices.push_back(geometry.walls.positions.size() + 3);

            geometry.walls.positions.push_back(points[0] + glm::vec3(0, wallHeight, 0));
            geometry.walls.positions.push_back(points[0] + glm::vec3(0, wallHeight, 0) - 0.5f * wallThickness * normal);
            geometry.walls.texCoords.push_back(glm::vec2(tc[0], wallHeight));
            geometry.walls.texCoords.push_back(glm::vec2(tc[0], wallHeight + 0.5f * wallThickness));
            geometry.walls.normals.push_back(glm::vec3(0, 1, 0));
            geometry.walls.normals.push_back(glm::vec3(0, 1, 0));

            geometry.walls.positions.push_back(points[1] + glm::vec3(0, wallHeight, 0));
            geometry.walls.positions.push_back(points[1] + glm::vec3(0, wallHeight, 0) - 0.5f * wallThickness * normal);
            geometry.walls.texCoords.push_back(glm::vec2(tc[1], wallHeight));
            geometry.walls.texCoords.push_back(glm::vec2(tc[1], wallHeight + 0.5f * wallThickness));
            geometry.walls.normals.push_back(glm::vec3(0, 1, 0));
            geometry.walls.normals.push_back(glm::vec3(0, 1, 0));
        }
    }

    for (const auto& obstacle : obstacles)
    {
        geometry.obstacleSides.indices.push_back(geometry.obstacleSides.positions.size());
        geometry.obstacleSides.indices.push_back(geometry.obstacleSides.positions.size() + 1);
        geometry.obstacleSides.indices.push_back(geometry.obstacleSides.positions.size() + 2);
        geometry.obstacleSides.indices.push_back(geometry.obstacleSides.positions.size() + 2);
        geometry.obstacleSides.indices.push_back(geometry.obstacleSides.positions.size() + 1);
        geometry.obstacleSides.indices.push_back(geometry.obstacleSides.positions.size() + 3);

        geometry.obstacleSides.positions.push_back(glm::vec3(obstacle.x, 0, obstacle.y + obstacle.height));
        geometry.obstacleSides.positions.push_back(glm::vec3(obstacle.x, obstacleHeight, obstacle.y + obstacle.height));
        geometry.obstacleSides.positions.push_back(glm::vec3(obstacle.x, 0, obstacle.y));
        geometry.obstacleSides.positions.push_back(glm::vec3(obstacle.x, obstacleHeight, obstacle.y));
        geometry.obstacleSides.texCoords.push_back(glm::vec2(obstacle.y + obstacle.height, 0));
        geometry.obstacleSides.texCoords.push_back(glm::vec2(obstacle.y + obstacle.height, obstacleHeight));
        geometry.obstacleSides.texCoords.push_back(glm::vec2(obstacle.y, 0));
        geometry.obstacleSides.texCoords.push_back(glm::vec2(obstacle.y, obstacleHeight));
        geometry.obstacleSides.normals.push_back(glm::vec3(-1, 0, 0));
        geometry.obstacleSides.normals.push_back(glm::vec3(-1, 0, 0));
        geometry.obstacleSides.normals.push_back(glm::vec3(-1, 0, 0));
        geometry.obstacleSides.normals.push_back(glm::vec3(-1, 0, 0));

        geometry.obstacleSides.indices.push_back(geometry.obstacleSides.positions.size());
        geometry.obstacleSides.indices.push_back(geometry.obstacleSides.positions.size() + 1);
        geometry.obstacleSides.indices.push_back(geometry.obstacleSides.positions.size() + 2);
        geometry.obstacleSides.indices.push_back(geometry.obstacleSides.positions.size() + 2);
        geometry.obstacleSides.indices.push_back(geometry.obstacleSides.positions.size() + 1);
        geometry.obstacleSides.indices.push_back(geometry.obstacleSides.positions.size() + 3);

        geometry.obstacleSides.positions.push_back(glm::vec3(obstacle.x, 0, obstacle.y));
        geometry.obstacleSides.positions.push_back(glm::vec3(obstacle.x, obstacleHeight, obstacle.y));
        geometry.obstacleSides.positions.push_back(glm::vec3(obstacle.x + obstacle.width, 0, obstacle.y));
        geometry.obstacleSides.positions.push_back(glm::vec3(obstacle.x + obstacle.width, obstacleHeight, obstacle.y));
        geometry.obstacleSides.texCoords.push_back(glm::vec2(obstacle.x, 0));
        geometry.obstacleSides.texCoords.push_back(glm::vec2(obstacle.x, obstacleHeight));
        geometry.obstacleSides.texCoords.push_back(glm::vec2(obstacle.x + obstacle.width, 0));
        geometry.obstacleSides.texCoords.push_back(glm::vec2(obstacle.x + obstacle.width, obstacleHeight));
        geometry.obstacleSides.normals.push_back(glm::vec3(0, 0, -1));
        geometry.obstacleSides.normals.push_back(glm::vec3(0, 0, -1));
        geometry.obstacleSides.normals.push_back(glm::vec3(0, 0, -1));
        geometry.obstacleSides.normals.push_back(glm::vec3(0, 0, -1));

        geometry.obstacleSides.indices.push_back(geometry.obstacleSides.positions.size());
        geometry.obstacleSides.indices.push_back(geometry.obstacleSides.positions.size() + 1);
        geometry.obstacleSides.indices.push_back(geometry.obstacleSides.positions.size() + 2);
        geometry.obstacleSides.indices.push_back(geometry.obstacleSides.positions.size() + 2);
        geometry.obstacleSides.indices.push_back(geometry.obstacleSides.positions.size() + 1);
        geometry.obstacleSides.indices.push_back(geometry.obstacleSides.positions.size() + 3);

        geometry.obstacleSides.positions.push_back(glm::vec3(obstacle.x + obstacle.width, 0, obstacle.y));
        geometry.obstacleSides.positions.push_back(glm::vec3(obstacle.x + obstacle.width, obstacleHeight, obstacle.y));
        geometry.obstacleSides.positions.push_back(glm::vec3(obstacle.x + obstacle.width, 0, obstacle.y + obstacle.height));
        geometry.obstacleSides.positions.push_back(glm::vec3(obstacle.x + obstacle.width, obstacleHeight, obstacle.y + obstacle.height));
        geometry.obstacleSides.texCoords.push_back(glm::vec2(obstacle.y, 0));
        geometry.obstacleSides.texCoords.push_back(glm::vec2(obstacle.y, obstacleHeight));
        geometry.obstacleSides.texCoords.push_back(glm::vec2(obstacle.y + obstacle.height, 0));
        geometry.obstacleSides.texCoords.push_back(glm::vec2(obstacle.y + obstacle.height, obstacleHeight));
        geometry.obstacleSides.normals.push_back(glm::vec3(1, 0, 0));
        geometry.obstacleSides.normals.push_back(glm::vec3(1, 0, 0));
        geometry.obstacleSides.normals.push_back(glm::vec3(1, 0, 0));
        geometry.obstacleSides.normals.push_back(glm::vec3(1, 0, 0));

        geometry.obstacleSides.indices.push_back(geometry.obstacleSides.positions.size());
        geometry.obstacleSides.indices.push_back(geometry.obstacleSides.positions.size() + 1);
        geometry.obstacleSides.indices.push_back(geometry.obstacleSides.positions.size() + 2);
        geometry.obstacleSides.indices.push_back(geometry.obstacleSides.positions.size() + 2);
        geometry.obstacleSides.indices.push_back(geometry.obstacleSides.positions.size() + 1);
        geometry.obstacleSides.indices.push_back(geometry.obstacleSides.positions.size() + 3);

        geometry.obstacleSides.positions.push_back(glm::vec3(obstacle.x + obstacle.width, 0, obstacle.y + obstacle.height));
        geometry.obstacleSides.positions.push_back(glm::vec3(obstacle.x + obstacle.width, obstacleHeight, obstacle.y + obstacle.height));
        geometry.obstacleSides.positions.push_back(glm::vec3(obstacle.x, 0, obstacle.y + obstacle.height));
        geometry.obstacleSides.positions.push_back(glm::vec3(obstacle.x, obstacleHeight, obstacle.y + obstacle.height));
        geometry.obstacleSides.texCoords.push_back(glm::vec2(obstacle.x + obstacle.width, 0));
        geometry.obstacleSides.texCoords.push_back(glm::vec2(obstacle.x + obstacle.width, obstacleHeight));
        geometry.obstacleSides.texCoords.push_back(glm::vec2(obstacle.x, 0));
        geometry.obstacleSides.texCoords.push_back(glm::vec2(obstacle.x, obstacleHeight));
        geometry.obstacleSides.normals.push_back(glm::vec3(0, 0, 1));
        geometry.obstacleSides.normals.push_back(glm::vec3(0, 0, 1));
        geometry.obstacleSides.normals.push_back(glm::vec3(0, 0, 1));
        geometry.obstacleSides.normals.push_back(glm::vec3(0, 0, 1));

        geometry.obstacleTops.indices.push_back(geometry.obstacleTops.positions.size());
        geometry.obstacleTops.indices.push_back(geometry.obstacleTops.positions.size() + 1);
        geometry.obstacleTops.indices.push_back(geometry.obstacleTops.positions.size() + 2);
        geometry.obstacleTops.indices.push_back(geometry.obstacleTops.positions.size() + 2);
        geometry.obstacleTops.indices.push_back(geometry.obstacleTops.positions.size() + 1);
        geometry.obstacleTops.indices.push_back(geometry.obstacleTops.positions.size() + 3);
        geometry.obstacleTops.positions.push_back(glm::vec3(obstacle.x, obstacleHeight, obstacle.y));
        geometry.obstacleTops.positions.push_back(glm::vec3(obstacle.x, obstacleHeight, obstacle.y + obstacle.height));
        geometry.obstacleTops.positions.push_back(glm::vec3(obstacle.x + obstacle.width, obstacleHeight, obstacle.y));
        geometry.obstacleTops.positions.push_back(glm::vec3(obstacle.x + obstacle.width, obstacleHeight, obstacle.y + obstacle.height));
        geometry.obstacleTops.texCoords.push_back(glm::vec2(obstacle.x, obstacle.y));
        geometry.obstacleTops.texCoords.push_back(glm::vec2(obstacle.x, obstacle.y + obstacle.height));
        geometry.obstacleTops.texCoords.push_back(glm::vec2(obstacle.x + obstacle.width, obstacle.y));
        geometry.obstacleTops.texCoords.push_back(glm::vec2(obstacle.x + obstacle.width, obstacle.y + obstacle.height));
        geometry.obstacleTops.normals.push_back(glm::vec3(0, 1, 0));
        geometry.obstacleTops.normals.push_back(glm::vec3(0, 1, 0));
        geometry.obstacleTops.normals.push_back(glm::vec3(0, 1, 0));
        geometry.obstacleTops.normals.push_back(glm::vec3(0, 1, 0));
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

    for (const auto& obstacle : obstacles)
    {
        bodies.push_back(physicsSystem.GetBodyInterface().CreateAndAddBody(
                    JPH::BodyCreationSettings(shapeRefs.emplace_back(new JPH::BoxShape(JPH::Vec3(0.5 * obstacle.width, 0.5 * wallHeight, 0.5 * obstacle.height))),
                        JPH::Vec3(obstacle.x + 0.5 * obstacle.width, 0.5 * wallHeight, obstacle.y + 0.5 * obstacle.height), JPH::Quat::sIdentity(), JPH::EMotionType::Static, 0),
                    JPH::EActivation::DontActivate));
    }
}

