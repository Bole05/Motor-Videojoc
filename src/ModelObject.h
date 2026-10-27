#pragma once

#include <string>
#include <memory>
#include <vector>
#include <glm/glm.hpp>
#include "Geometria.h"
#include "Texture.h"

// Un objeto de la escena. Cuando se importa un FBX se crea un ModelObject por
// cada nodo del fichero, formando un árbol (padre -> hijos), por ejemplo:
//   BakerHouse.fbx
//     |- Baker_house
//     |- Chimney
struct ModelObject {
    // Dato
    std::string name;
    std::string filePath;
    std::shared_ptr<Geometria> mesh;          // Geometría del FBX (compartida por todos sus nodos)
    std::vector<unsigned int> meshIndices;    // Submallas que dibuja ESTE nodo

    // Jerarquía
    ModelObject* parent = nullptr;
    std::vector<std::shared_ptr<ModelObject>> children;

    // Transform propio del nodo según el FBX (se aplica después del Transform editable)
    glm::mat4 baseTransform = glm::mat4(1.0f);

    // Transform
    glm::vec3 position = glm::vec3(0.0f, 0.0f, 0.0f);
    glm::vec3 rotation = glm::vec3(0.0f, 0.0f, 0.0f); // Ángulos en grados (X, Y, Z)
    glm::vec3 scale    = glm::vec3(1.0f, 1.0f, 1.0f);

    // Visuales y material
    glm::vec3 color    = glm::vec3(0.75f, 0.75f, 0.75f);
    bool visible       = true;
    bool wireframe     = false;

    // Texturas
    bool showTexture   = true;                // Mostrar / ocultar la textura de este objeto
    std::shared_ptr<Texture> overrideTexture; // Textura asignada a mano (arrastrando una imagen)
    bool useCheckerTexture = false;           // Textura de comprobación (cuadros blancos y negros)

    // Animación de rotación opcional
    bool autoRotate    = false;
    float autoRotateSpeed = 30.0f; // Grados por segundo
};
