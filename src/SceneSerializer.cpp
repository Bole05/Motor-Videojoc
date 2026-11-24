#pragma once
#include "SceneSerializer.h"
#include "GameObject.h"
#include "Geometria.h"
#include "Texture.h"
#include "Log.h"
#include <unordered_map>
#include <SDL3/SDL.h>
#include <nlohmann/json.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <filesystem>
#include <fstream>

//Renombrar
//Para renombrar una variable de tipo tipo
using json = nlohmann::json;
//Para renombrar funcion
namespace fs = std::filesystem;
using SceneList = SceneSerializer::SceneList;

namespace {   // funciones internas de este .cpp = esconder que otra class no la puede ver

    //json no se entiende un glm::vec3, solo numero,texto,booleanos, array y objetos, por lo tanto debemos hacer un paso más en inicio para convertir tip vec en forma de que el json que la puede entender 
    json Vec3ToJson(const glm::vec3& v) {
        return json::array({ v.x, v.y, v.z });
    }

    //Leer y contiviertir en Vec3
    glm::vec3 ReadVec3(const json& j, const char* key, const glm::vec3& def) {
        const auto it = j.find(key);
        if (it == j.end() || !it->is_array() || it->size() != 3) return def;
        return glm::vec3((*it)[0].get<float>(), (*it)[1].get<float>(), (*it)[2].get<float>());
    }

    // "C:/.../build/Debug/assets/Street/x.fbx"  ->  "assets/Street/x.fbx"
    std::string MakeRelativePath(const std::string& path) {
        if (path.empty() || path.rfind("(primitiva)", 0) == 0) return path;   // vacío o primitiva: tal cual

        fs::path p = fs::path(path).lexically_normal();
        if (const char* base = SDL_GetBasePath()) {
            // SDL_GetBasePath termina en '\', parent_path() lo quita
            const fs::path baseDir = fs::path(base).lexically_normal().parent_path();
            const fs::path rel = p.lexically_relative(baseDir);
            if (!rel.empty() && *rel.begin() != "..") return rel.generic_string();
        }
        return p.generic_string();   // no está dentro de la carpeta del motor: se deja absoluta

    }

    // "assets/Street/x.fbx" -> busca en la carpeta de trabajo y, si no, junto al .exe
    std::string ResolvePath(const std::string& path) {
        std::error_code ec;
        if (fs::exists(path, ec)) return path;
        if (const char* base = SDL_GetBasePath()) {
            const std::string candidate = std::string(base) + path;
            if (fs::exists(candidate, ec)) return candidate;
        }
        return "";   // no se encuentra
    }

    //convertir una matrix de 4x4 en forma array, para que la libreria se entienda
    json Mat4ToJson(const glm::mat4& m) {
        const float* p = glm::value_ptr(m);
        return json(std::vector<float>(p, p + 16));
    }

    //Convertir la forma que se entienda en Mat4x4
    glm::mat4 ReadMat4(const json& j, const char* key) {
        glm::mat4 m(1.0f);
        const auto it = j.find(key);
        if (it == j.end() || !it->is_array() || it->size() != 16) return m;
        float* p = glm::value_ptr(m);
        for (size_t i = 0; i < 16; ++i) p[i] = (*it)[i].get<float>();
        return m;
    }

    //Lo que hace este bloque es coger un GameObject y convertirlo en objecto Json
    json GameObjectToJson(const GameObject& obj) {//ller el Gameobject
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
        m["source"] = obj.mesh->geometry ? MakeRelativePath(obj.mesh->geometry->GetFilePath()) : "";
        m["meshIndices"] = obj.mesh->meshIndices;
        m["color"] = Vec3ToJson(obj.mesh->color);
        m["wireframe"] = obj.mesh->wireframe;
        j["mesh"] = m;

        // Textura
        json tx;
        tx["showTexture"] = obj.texture->showTexture;
        tx["overrideTexture"] = obj.texture->HasOverride() ? MakeRelativePath(obj.texture->overrideTexture->GetPath()) : "";
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

    json SceneToJson(const SceneList& scene) {
        json root;
        root["version"] = 1;
        json objects = json::array();
        for (const auto& obj : scene) objects.push_back(GameObjectToJson(*obj));
        root["gameObjects"] = objects;
        return root;
    }

    bool SceneFromJson(const json& root, SceneList& outScene, SceneResourceCache& cache) {
        if (!root.is_object() || !root.contains("gameObjects")) return false;
        try {
            SceneList loaded;
            for (const auto& j : root["gameObjects"]) {
                loaded.push_back(GameObjectFromJson(j, nullptr, cache));
            }
            outScene = std::move(loaded);
            return true;
        }
        catch (const json::exception& e) {
            LOG("Escena: error al leer los datos (%s)", e.what());
            return false;
        }
    }

    struct LoadCache {
        std::unordered_map<std::string, std::shared_ptr<Geometria>> geometries;
        std::unordered_map<std::string, std::shared_ptr<Texture>> textures;
    };

    std::shared_ptr<Geometria> GetGeometry(const std::string& source, LoadCache& cache) {
        // ¿Ya la hemos cargado? Se reutiliza
        if (auto it = cache.geometries.find(source); it != cache.geometries.end()) return it->second;

        auto geom = std::make_shared<Geometria>();
        bool ok = false;

        const std::string prefix = "(primitiva) ";
        if (source.rfind(prefix, 0) == 0) {
            // Primitiva: buscar qué tipo es por su nombre ("Cubo", "Esfera"...)
            const std::string name = source.substr(prefix.size());
            const PrimitiveType types[] = { PrimitiveType::Cube, PrimitiveType::Sphere, PrimitiveType::Cylinder,
                                            PrimitiveType::Cone, PrimitiveType::Plane, PrimitiveType::Pyramid };
            for (PrimitiveType t : types) {
                if (name == Geometria::PrimitiveName(t)) { ok = geom->CreatePrimitive(t); break; }
            }
        }
        else {
            const std::string disk = ResolvePath(source);
            ok = !disk.empty() && geom->Load(disk);
        }

        if (!ok) {
            LOG("Escena: no se ha podido cargar la malla '%s'", source.c_str());
            return nullptr;
        }
        cache.geometries[source] = geom;
        return geom;
    }

    std::shared_ptr<Texture> GetTexture(const std::string& path, LoadCache& cache) {
        if (auto it = cache.textures.find(path); it != cache.textures.end()) return it->second;

        auto tex = std::make_shared<Texture>();
        const std::string disk = ResolvePath(path);
        if (disk.empty() || !tex->Load(disk)) {
            LOG("Escena: no se ha podido cargar la textura '%s'", path.c_str());
            return nullptr;
        }
        cache.textures[path] = tex;
        return tex;
    }

    std::shared_ptr<GameObject> GameObjectFromJson(const json& j, GameObject* parent, LoadCache& cache) {
        auto obj = std::make_shared<GameObject>(j.value("name", std::string("GameObject")), parent);
        obj->uid = j.value("uid", obj->uid);
        obj->visible = j.value("visible", true);

        // Transform
        if (j.contains("transform")) {
            const json& t = j["transform"];
            obj->transform->position = ReadVec3(t, "position", glm::vec3(0.0f));
            obj->transform->rotation = ReadVec3(t, "rotation", glm::vec3(0.0f));
            obj->transform->scale = ReadVec3(t, "scale", glm::vec3(1.0f));
            obj->transform->baseTransform = ReadMat4(t, "baseTransform");
        }

        // Mesh
        if (j.contains("mesh")) {
            const json& m = j["mesh"];
            obj->mesh->color = ReadVec3(m, "color", glm::vec3(0.75f));
            obj->mesh->wireframe = m.value("wireframe", false);

            const std::string source = m.value("source", std::string());
            if (!source.empty()) {
                obj->mesh->geometry = GetGeometry(source, cache);
                if (obj->mesh->geometry && m.contains("meshIndices")) {
                    for (const auto& idx : m["meshIndices"]) {
                        const unsigned int i = idx.get<unsigned int>();
                        // Sólo submallas que existan de verdad (por si el FBX ha cambiado)
                        if (i < obj->mesh->geometry->GetMeshCount()) obj->mesh->meshIndices.push_back(i);
                    }
                }
            }
        }

        // Textura
        if (j.contains("texture")) {
            const json& tx = j["texture"];
            obj->texture->showTexture = tx.value("showTexture", true);
            obj->texture->useCheckerTexture = tx.value("useCheckerTexture", false);
            const std::string texPath = tx.value("overrideTexture", std::string());
            if (!texPath.empty()) obj->texture->overrideTexture = GetTexture(texPath, cache);
        }

        // Hijos: la misma función, con ESTE objeto como padre
        if (j.contains("children")) {
            for (const auto& child : j["children"]) {
                obj->children.push_back(GameObjectFromJson(child, obj.get(), cache));
            }
        }
        return obj;
    }
} 
// Guardar Modificación
bool SceneSerializer::SaveToFile(const std::string& path, const SceneList& scene) {

    //Crear una carpeta especifica para uso de guardar las diferentes verciones 
    std::error_code ec;
    fs::create_directories(fs::path(path).parent_path(), ec);

    //Abrir el fichero
    std::ofstream file(path);
    if (!file) {
        LOG("No se puede escribir la escena en %s", path.c_str());
        return false;
    }

    //Guardamos una nueva verción
    file << SceneToJson(scene).dump(2, ' ', false, json::error_handler_t::replace);
    LOG("Escena guardada en %s", path.c_str());
    return true;
}

bool SceneSerializer::LoadFromFile(const std::string& path, SceneList& outScene, SceneResourceCache& cache) {
    std::ifstream file(path);
    if (!file) {
        LOG("Escena: no se encuentra el fichero %s", path.c_str());
        return false;
    }

    // false = no lanzar excepción si el JSON está mal; devuelve un json "descartado"
    const json root = json::parse(file, nullptr, false);
    if (root.is_discarded() || !root.contains("gameObjects")) {
        LOG("Escena: %s no es un fichero de escena valido", path.c_str());
        return false;
    }
    LOG("Escena cargada desde %s", path.c_str());
    return true;
}

std::shared_ptr<GameObject> SceneSerializer::FindByUID(const SceneList& scene, uint32_t uid) {
    if (uid == 0) return nullptr;                       // 0 = "ningún objeto"
    for (const auto& obj : scene) {
        if (obj->uid == uid) return obj;                // es este
        if (auto found = FindByUID(obj->children, uid)) // ¿está en sus hijos?
            return found;
    }
    return nullptr;                                     // no está en la escena
}