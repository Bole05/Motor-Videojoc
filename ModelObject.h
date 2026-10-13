#pragma once

#include <string>
#include <memory>
#include <glm/glm.hpp>
#include "Geometria.h"

struct ModelObject {
    // Dato
    std::string name;
    std::string filePath;
    std::shared_ptr<Geometria> mesh;

    // Transform
    glm::vec3 position = glm::vec3(0.0f, 0.0f, 0.0f);
    glm::vec3 rotation = glm::vec3(0.0f, 0.0f, 0.0f); // Ángulos en grados (X, Y, Z)
    glm::vec3 scale    = glm::vec3(1.0f, 1.0f, 1.0f);

    // Visuales y material
    glm::vec3 color    = glm::vec3(0.75f, 0.75f, 0.75f);
    bool visible       = true;
    bool wireframe     = false;

    // Animación de rotación opcional
    bool autoRotate    = false;
    float autoRotateSpeed = 30.0f; // Grados por segundo
};
