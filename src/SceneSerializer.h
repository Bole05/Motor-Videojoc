#pragma once

#include <memory>
#include <string>
#include <vector>
#include <cstdint>
#include <unordered_map>

class GameObject;
class Texture;

//No se guarda ningún dato propio
namespace SceneSerializer {
    //alias,es útil para escribir un parámetro largo en una forma más facil de llamarlo
    //Cada vez que llamamos SceneList, es igual si poner su forma de escriptura original
    using SceneList = std::vector<std::shared_ptr<GameObject>>;

    //path->la ruta del fichero de la escena y la escena que la va a guardar.
    bool SaveToFile(const std::string& path, const SceneList& scene);

    //Leer una verción que havia guardado anteriormente (Clave para implementar la funcion CTRL+Z)
    bool LoadFromFile(const std::string& path, SceneList& outScene);
    std::shared_ptr<GameObject> FindByUID(const SceneList& scene, uint32_t uid);
}

// Mallas y texturas ya cargadas, por ruta. Se reutilizan al cargar o al deshacer.
struct SceneResourceCache {
    std::unordered_map<std::string, std::shared_ptr<Geometria>> geometries;
    std::unordered_map<std::string, std::shared_ptr<Texture>> textures;
};