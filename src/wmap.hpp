#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <vector>
#include <glm/glm.hpp>

namespace eng
{
    class ResourceLoaderInterface;
}

namespace wmap
{
    struct Face
    {
        std::vector<glm::vec3> vertices;
        glm::vec4 uvBasis[2];
        glm::vec3 planeNormal;
        float planeDistance;
        std::string texture;
    };

    struct Shape
    {
        std::vector<Face> faces;
        glm::vec3 center;
        struct
        {
            glm::vec3 min, max;
        } extents;
    };

    struct Entity
    {
        std::map<std::string, std::string> params;
    };

    struct Map
    {
        std::vector<Shape> shapes;
        std::vector<Entity> entities;
    };

    Map load(const std::string& path, const float scale = 64.0f);

    std::vector<std::pair<uint32_t, uint32_t>> createGeometry(const Map& map, eng::ResourceLoaderInterface& resourceLoader, const std::string& texturesDirectoryPath);

    bool testInside(const Map& map, const glm::vec3& position);

    
}
