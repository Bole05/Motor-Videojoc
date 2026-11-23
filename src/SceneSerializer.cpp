#pragma once
#include "SceneSerializer.h"
#include "GameObject.h"
#include "Geometria.h"
#include "Texture.h"
#include "Log.h"
#include <nlohmann/json.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <filesystem>
#include <fstream>

using json = nlohmann::json;
namespace fs = std::filesystem;

namespace {   // funciones internas de este .cpp

    json Vec3ToJson(const glm::vec3& v) {
        return json::array({ v.x, v.y, v.z });
    }

    json Mat4ToJson(const glm::mat4& m) {
        const float* p = glm::value_ptr(m);
        return json(std::vector<float>(p, p + 16));
    }

    json GameObjectToJson(const GameObject& obj) {
        json j;
        j["uid"] = obj.uid;
        j["name"] = obj.name;
        j["visible"] = obj.visible;

        // Transform
        json t;
        t["position"] = Vec3ToJson(obj.transform->position);
        t["rotation"] = Vec3ToJson(obj.transform->rotation);
        t["scale"] = Vec3ToJson(obj.transform->scale);
        t["baseTransform"] = Mat4ToJson(obj.transform->baseTransform);
        j["transform"] = t;

        // Mesh: sólo DE DÓNDE sale la malla, no los vértices
        json m;
        m["source"] = obj.mesh->geometry ? obj.mesh->geometry->GetFilePath() : "";
        m["meshIndices"] = obj.mesh->meshIndices;
        m["color"] = Vec3ToJson(obj.mesh->color);
        m["wireframe"] = obj.mesh->wireframe;
        j["mesh"] = m;

        // Textura
        json tx;
        tx["showTexture"] = obj.texture->showTexture;
        tx["overrideTexture"] = obj.texture->HasOverride() ? obj.texture->overrideTexture->GetPath() : "";
        tx["useCheckerTexture"] = obj.texture->useCheckerTexture;
        j["texture"] = tx;

        // Hijos: la misma función para cada uno
        json children = json::array();
        for (const auto& child : obj.children) {
            children.push_back(GameObjectToJson(*child));
        }
        j["children"] = children;
        return j;
    }

} // 