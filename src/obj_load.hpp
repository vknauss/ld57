#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace eng
{
    class ResourceLoaderInterface;
};

std::vector<std::pair<uint32_t, uint32_t>> loadGeometryObj(eng::ResourceLoaderInterface& resourceLoader, const std::string& path, const uint32_t defaultMaterial, const std::string& texturesDirectoryPath = ".");
