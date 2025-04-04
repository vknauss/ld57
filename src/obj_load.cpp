#include "obj_load.hpp"
#include "engine.hpp"

#include <iostream>
#include <stdexcept>
#include <tiny_obj_loader.h>

std::vector<std::pair<uint32_t, uint32_t>> loadGeometryObj(eng::ResourceLoaderInterface& resourceLoader, const std::string& path, const uint32_t defaultMaterial, const std::string& texturesDirectoryPath)
{
    tinyobj::ObjReaderConfig objReaderConfig;
    objReaderConfig.triangulate = true;
    objReaderConfig.vertex_color = false;
    tinyobj::ObjReader objReader;
    if (!objReader.ParseFromFile(path, objReaderConfig))
    {
        throw std::runtime_error("TinyObjLoader failed to load file \"" + path + "\":" + objReader.Error());
    }

    if (!objReader.Warning().empty())
    {
        std::cout << "TinyObjLoader warning loading file \"" + path + "\":" + objReader.Warning() << std::endl;
    }

    std::map<int, uint32_t> materialResourceMap;
    std::map<uint32_t, eng::GeometryDescription> materialGeometryMap;
    const auto& attrib = objReader.GetAttrib();
    for (const auto& shape : objReader.GetShapes())
    {
        for (uint32_t i = 0; i < shape.mesh.num_face_vertices.size(); ++i)
        {
            uint32_t materialResource = defaultMaterial;
            int materialID = shape.mesh.material_ids[i];
            auto materialResourceIter = materialResourceMap.find(materialID);
            if (materialResourceIter == materialResourceMap.end())
            {
                const auto& material = objReader.GetMaterials()[materialID];
                if (!material.diffuse_texname.empty())
                {
                    materialResource = resourceLoader.loadTexture(texturesDirectoryPath + "/" + material.diffuse_texname);
                }

                bool inserted;
                std::tie(materialResourceIter, inserted) = materialResourceMap.insert({ materialID, materialResource });
            }
            else
            {
                materialResource = materialResourceIter->second;
            }

            auto& geometry = materialGeometryMap[materialResource];
            for (uint32_t j = 0; j < 3; ++j)
            {
                const auto& indices = shape.mesh.indices[3 * i + j];
                if (indices.texcoord_index < 0)
                {
                    throw std::runtime_error("vertex missing tex coords");
                }
                
                geometry.indices.push_back(geometry.positions.size());
                geometry.positions.push_back({
                            attrib.vertices[3 * indices.vertex_index],
                            attrib.vertices[3 * indices.vertex_index + 1],
                            attrib.vertices[3 * indices.vertex_index + 2],
                        });
                geometry.texCoords.push_back({
                            attrib.texcoords[2 * indices.texcoord_index],
                            attrib.texcoords[2 * indices.texcoord_index + 1],
                        });
                geometry.normals.push_back({
                            attrib.normals[3 * indices.normal_index],
                            attrib.normals[3 * indices.normal_index + 1],
                            attrib.normals[3 * indices.normal_index + 2],
                        });
            }
        }
    }

    std::vector<std::pair<uint32_t, uint32_t>> resourcePairs;
    resourcePairs.reserve(materialGeometryMap.size());
    for (const auto& [materialResource, geometryDescription] : materialGeometryMap)
    {
        resourcePairs.push_back({ materialResource, resourceLoader.createGeometry(geometryDescription) });
    }

    return resourcePairs;
}
