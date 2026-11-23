#pragma once

#include <memory>
#include <string>
#include <vector>

class GameObject;

//No se guarda ningún dato propio
namespace SceneSerializer {
    using SceneList = std::vector<std::shared_ptr<GameObject>>;

    bool SaveToFile(const std::string& path, const SceneList& scene);
}