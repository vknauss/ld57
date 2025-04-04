#include "wmap.hpp"
#include "engine.hpp"

#include <algorithm>
#include <array>
#include <fstream>
#include <map>
#include <sstream>
#include <string_view>
#include <vector>
#include <iostream>

#include <glm/glm.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/string_cast.hpp>

// inspired by qodot:
// https://github.com/QodotPlugin/Qodot/blob/3c611e7d00cf45ef30faf69741aa87b9fd98bac9/addons/qodot/src/core/qodot_geo_generator.gd#L147

using namespace wmap;

namespace
{
    constexpr float MERGE_TOLERANCE = 0.01;
}

static std::string nextToken(std::ifstream& ifs)
{
    std::string tok;
    while (ifs >> tok)
    {
        if (tok.starts_with("//"))
        {
            std::getline(ifs, tok);
            continue;
        }
        if (!tok.empty())
        {
            break;
        }
    }
    return tok;
}

static std::string parseQuoted(std::ifstream& ifs, const std::string& firstTok)
{
    std::string s;
    if (firstTok.starts_with('\"'))
    {
        if (firstTok.size() > 1 && firstTok.ends_with('\"'))
        {
            s = firstTok.substr(1, firstTok.size() - 2);
        }
        else
        {
            std::stringstream sstr;
            sstr << firstTok;
            std::string tok = nextToken(ifs);
            for (; !tok.empty(); tok = nextToken(ifs))
            {
                sstr << " ";
                sstr << tok;
                if (tok.ends_with('\"'))
                {
                    break;
                }
            }
            s = sstr.str();

            if (s.size() > 1)
            {
                s = s.substr(1, s.size() - 2);
            }
            else
            {
                s.clear();
            }
        }
    }
    return s;
}

namespace qmap
{

    struct BrushFace
    {
        glm::vec3 points[3];
        std::string texture;
        glm::vec4 uvBasis[2];
        glm::vec2 uvScale;
        glm::vec3 planeNormal;
        float planeDistance;
    };

    struct Brush
    {
        std::vector<BrushFace> faces;
    };

    struct Entity
    {
        std::map<std::string, std::string> properties;
        std::vector<Brush> brushes;
    };

    struct MapFile
    {
        std::vector<Entity> entities;
    };


    static Brush readMapBrush(std::ifstream& ifs)
    {
        Brush brush;

        auto tok = nextToken(ifs);
        for (; tok.starts_with('('); tok = nextToken(ifs))
        {
            BrushFace face;
            int index = 0;
            for (index = 0; tok.starts_with('('); ++index, tok = nextToken(ifs))
            {
                if (index > 2)
                {
                    throw std::runtime_error("too many vertices in brush face");
                }

                ifs >> face.points[index].x >> face.points[index].y >> face.points[index].z;
                tok = nextToken(ifs);
                if (!tok.starts_with(')'))
                {
                    throw std::runtime_error("unexpected token parsing face vertex: " + tok);
                }
            }
            if (index < 3)
            {
                throw std::runtime_error("too few vertices in brush face");
            }

            face.texture = tok;

            tok = nextToken(ifs);
            for (index = 0; tok.starts_with('['); ++index, tok = nextToken(ifs))
            {
                if (index > 1)
                {
                    throw std::runtime_error("too many uv offsets in brush face");
                }

                ifs >> face.uvBasis[index].x >> face.uvBasis[index].y >> face.uvBasis[index].z >> face.uvBasis[index].w;
                tok = nextToken(ifs);
                if (!tok.starts_with(']'))
                {
                    throw std::runtime_error("unexpected token parsing uv offset: " + tok);
                }
            }
            if (index < 2)
            {
                throw std::runtime_error("too few uv offsets in brush face");
            }

            std::stof(tok); // rotation, don't care
            ifs >> face.uvScale.x >> face.uvScale.y;

            face.planeNormal = glm::normalize(glm::cross(face.points[2] - face.points[1], face.points[1] - face.points[0]));
            face.planeDistance = glm::dot(face.planeNormal, face.points[0]);

            brush.faces.push_back(face);
        }

        if (!tok.starts_with('}'))
        {
            throw std::runtime_error("unexpected token parsing brush: " + tok);
        }

        return brush;
    }

    static Entity readMapEntity(std::ifstream& ifs)
    {
        Entity entity;

        auto tok = nextToken(ifs);
        for (; tok.starts_with('\"'); tok = nextToken(ifs))
        {
            auto pname = parseQuoted(ifs, tok);
            entity.properties[pname] = parseQuoted(ifs, nextToken(ifs));
        }

        for (; tok.starts_with('{'); tok = nextToken(ifs))
        {
            entity.brushes.push_back(readMapBrush(ifs));
        }

        if (!tok.starts_with('}'))
        {
            throw std::runtime_error("unexpected token parsing entity: " + tok);
        }

        return entity;
    }

    static MapFile readMap(std::ifstream& ifs)
    {
        MapFile map;

        auto tok = nextToken(ifs);
        for (; tok.starts_with('{'); tok = nextToken(ifs))
        {
            map.entities.push_back(readMapEntity(ifs));
        }

        if (!tok.empty())
        {
            throw std::runtime_error("unexpected token parsing map: " + tok);
        }

        return map;
    }

    static bool intersectFaces(const BrushFace& f0, const BrushFace& f1, const BrushFace& f2, glm::vec3& vertex)
    {
        float denom = glm::dot(glm::cross(f0.planeNormal, f1.planeNormal), f2.planeNormal);
        if (denom <= glm::epsilon<float>())
        {
            return false;
        }

        vertex = f0.planeDistance * glm::cross(f1.planeNormal, f2.planeNormal) +
            f1.planeDistance * glm::cross(f2.planeNormal, f0.planeNormal) +
            f2.planeDistance * glm::cross(f0.planeNormal, f1.planeNormal);
        vertex /= denom;

        return true;
    }

    static bool vertexInHull(const Brush& brush, const glm::vec3& vertex)
    {
        for (const auto& face : brush.faces)
        {
            float proj = glm::dot(vertex, face.planeNormal);
            if (proj - face.planeDistance > MERGE_TOLERANCE)
            {
                return false;
            }
        }
        return true;
    }

    static void generateFaceVertices(const Brush& brush, const BrushFace& face, std::vector<glm::vec3>& vertices)
    {
        vertices.clear();
        for (auto i = 0u; i < brush.faces.size(); ++i)
        {
            for (auto j = 0u; j < brush.faces.size(); ++j)
            {
                glm::vec3 vertex;
                if (!intersectFaces(face, brush.faces[i], brush.faces[j], vertex))
                {
                    continue;
                }
                if (!vertexInHull(brush, vertex))
                {
                    continue;
                }

                vertices.push_back(vertex);
            }
        }
    }
}

wmap::Map wmap::load(const std::string& path, const float scale)
{
    Map map;
    std::vector<glm::vec3> faceVertices;
    // const glm::mat3 inverseTransform = glm::inverse(transform);
    constexpr glm::mat3 coordMapping({ 0, 0, -1 }, { -1, 0, 0 }, { 0, 1, 0 });
    const glm::mat3 transform(coordMapping / scale); 

    std::ifstream ifs(path);
    qmap::MapFile mapfile = qmap::readMap(ifs);
    for (const auto& entity : mapfile.entities)
    {
        for (const auto& brush : entity.brushes)
        {
            Shape shape {
                .center = glm::vec3(0),
                .extents = {
                    .min = glm::vec3(std::numeric_limits<float>::max()),
                    .max = -glm::vec3(std::numeric_limits<float>::max()),
                },
            };

            shape.faces.reserve(brush.faces.size());
            uint32_t shapeVertexCount = 0;

            for (const auto& brushface : brush.faces)
            {
                qmap::generateFaceVertices(brush, brushface, faceVertices);
                if (faceVertices.size() < 3)
                {
                    continue;
                }

                glm::vec3 basis0 = glm::normalize(faceVertices[1] - faceVertices[0]);
                glm::vec3 basis1 = glm::normalize(glm::cross(basis0, brushface.planeNormal));
                glm::vec3 center(0);

                for (const auto& vertex : faceVertices)
                {
                    center += vertex;
                }
                center /= faceVertices.size();

                std::sort(faceVertices.begin(), faceVertices.end(), [&](const auto& v0, const auto& v1) {
                        glm::vec3 d0 = v0 - center;
                        glm::vec3 d1 = v1 - center;
                        return glm::atan(glm::dot(d0, basis1), glm::dot(d0, basis0)) <
                            glm::atan(glm::dot(d1, basis1), glm::dot(d1, basis0));
                    });

                Face face {
                    .uvBasis = {
                        glm::vec4(coordMapping * glm::vec3(brushface.uvBasis[0]) * scale / brushface.uvScale.x, brushface.uvBasis[0].w),
                        glm::vec4(coordMapping * glm::vec3(brushface.uvBasis[1]) * scale / brushface.uvScale.y, brushface.uvBasis[1].w),
                    },
                    .planeNormal = glm::normalize(transform * brushface.planeNormal),
                    .texture = brushface.texture,
                };

                face.vertices.reserve(faceVertices.size());
                for (const auto& vertex : faceVertices)
                {
                    const auto v = transform * vertex;
                    shape.center += v;
                    shape.extents.min = glm::min(shape.extents.min, v);
                    shape.extents.max = glm::max(shape.extents.max, v);
                    face.vertices.push_back(v);
                    ++shapeVertexCount;
                }

                face.planeDistance = glm::dot(face.planeNormal, face.vertices[0]);

                shape.faces.push_back(std::move(face));
                faceVertices.clear();
            }

            shape.center /= shapeVertexCount;
            map.shapes.push_back(std::move(shape));
        }

        if (auto it = entity.properties.find("classname"); it != entity.properties.end() && it->second != "worldspawn")
        {
            map.entities.push_back(Entity { entity.properties });
        }
    }

    return map;
}

static std::pair<uint32_t, eng::TextureInfo> loadTexture(eng::ResourceLoaderInterface& resourceLoader,
        const std::string& textureSearchPath,
        const std::string& textureName)
{
    // default trenchbroom config extensions
    constexpr std::array textureExtensions {
        std::string_view{".png"},
        std::string_view{".jpg"},
        std::string_view{".jpeg"},
        std::string_view{".tga"},
    };

    for (const auto& extension : textureExtensions)
    {
        try
        {
            std::stringstream path;
            path << textureSearchPath << "/" << textureName << extension;
            eng::TextureInfo textureInfo;
            auto textureResource = resourceLoader.loadTexture(path.str(), &textureInfo);
            return std::make_pair(std::move(textureResource), std::move(textureInfo));
        }
        catch (std::exception&)
        {
        }
    }

    throw std::runtime_error("no matching texture found: " + textureName);
}

std::vector<std::pair<uint32_t, uint32_t>> wmap::createGeometry(const wmap::Map& map,
        eng::ResourceLoaderInterface& resourceLoader,
        const std::string& texturesDirectoryPath)
{
    std::map<std::string, std::pair<uint32_t, eng::TextureInfo>> textureMap;
    std::map<uint32_t, eng::GeometryDescription> geometryByTextureResource;

    for (const auto& shape : map.shapes)
    {
        for (const auto& face : shape.faces)
        {
            auto textureMapIterator = textureMap.find(face.texture);
            if (textureMapIterator == textureMap.end())
            {
                bool inserted;
                std::tie(textureMapIterator, inserted) = textureMap.insert({
                            face.texture,
                            loadTexture(resourceLoader, texturesDirectoryPath, face.texture)
                        });
            }
            const auto& textureInfo = textureMapIterator->second.second;
            auto& geometry = geometryByTextureResource[textureMapIterator->second.first];

            for (uint32_t i = 2; i < face.vertices.size(); ++i)
            {
                geometry.indices.push_back(geometry.positions.size());
                geometry.indices.push_back(geometry.positions.size() + i - 1);
                geometry.indices.push_back(geometry.positions.size() + i);
            }

            for (const auto& vertex : face.vertices)
            {
                geometry.positions.push_back(vertex);
                glm::vec2 texCoord(face.uvBasis[0].w + glm::dot(glm::vec3(face.uvBasis[0]), vertex),
                        face.uvBasis[1].w + glm::dot(glm::vec3(face.uvBasis[1]), vertex));
                texCoord = glm::vec2(texCoord.x / textureInfo.width, 1.0f - texCoord.y / textureInfo.height);
                geometry.texCoords.push_back(texCoord);
                geometry.normals.push_back(face.planeNormal);
            }
        }
    }

    std::vector<std::pair<uint32_t, uint32_t>> resourceHandlePairs;
    resourceHandlePairs.reserve(geometryByTextureResource.size());
    for (const auto& [ texture, geometry ] : geometryByTextureResource)
    {
        resourceHandlePairs.push_back(std::make_pair(texture, resourceLoader.createGeometry(geometry)));
    }

    return resourceHandlePairs;
}

static bool shapeTestInside(const wmap::Shape& shape, const glm::vec3& position, const float radius = 0.2)
{
    if (glm::any(glm::greaterThan(position - radius, shape.extents.max)) ||
            glm::any(glm::lessThan(position + radius, shape.extents.min)))
    {
        return false;
    }
    // std::cout << glm::to_string(position) << " " << glm::to_string(shape.extents.min) << " " << glm::to_string(shape.extents.max) << std::endl;

    const glm::vec3 fromCenter = position - shape.center;
    for (const auto& face : shape.faces)
    {
        if (glm::dot(face.planeNormal, position) - face.planeDistance > radius + MERGE_TOLERANCE)
        {
            std::cout << "position: " << glm::to_string(position) << std::endl;
            std::cout << "center: " << glm::to_string(shape.center) << std::endl;
            std::cout << "planeNormal: " << glm::to_string(face.planeNormal) << std::endl;
            std::cout << "planeDistance: " << face.planeDistance << std::endl;
            return false;
        }

        for (uint32_t i = 0; i < face.vertices.size(); ++i)
        {
            const glm::vec3 edge = face.vertices[(i + 1) % face.vertices.size()] - face.vertices[i];
            glm::vec3 axis = face.vertices[i] - shape.center;
            axis -= glm::dot(axis, edge) / glm::dot(edge, edge);
            axis = glm::normalize(axis);

            if (glm::dot(axis, fromCenter) - glm::dot(axis, face.vertices[i] - shape.center) > radius + MERGE_TOLERANCE)
            {
                std::cout << "position: " << glm::to_string(position) << std::endl;
                std::cout << "axis : " << glm::to_string(axis) << std::endl;
                std::cout << "center: " << glm::to_string(shape.center) << std::endl;
                std::cout << "vertices: [ " << glm::to_string(face.vertices[(i + 1) % face.vertices.size()]) << ", "
                    << glm::to_string(face.vertices[i]) << " ]" << std::endl;
                return false;
            }
        }
    }

    return true;
}

bool wmap::testInside(const Map& map, const glm::vec3& position)
{
    for (const auto& shape : map.shapes)
    {
        if (shapeTestInside(shape, position))
        {
            for (const auto& face : shape.faces)
            {
                for (const auto& v : face.vertices)
                {
                    std::cout << "  " << glm::to_string(v) << std::endl;
                }
            }
            return true;
        }
    }
    
    return false;
}
