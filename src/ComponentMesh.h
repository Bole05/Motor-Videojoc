#pragma once

#include <memory>
#include <vector>
#include <glm/glm.hpp>
#include "Component.h"

class Geometria;

// Componente Mesh: la malla que dibuja el GameObject.
// La Geometria (buffers de GPU del fichero) se comparte entre todos los nodos de un mismo FBX;
// meshIndices indica qué submallas de esa Geometria dibuja ESTE GameObject.
class ComponentMesh : public Component {
public:
    static constexpr ComponentType TYPE = ComponentType::MESH;

    explicit ComponentMesh(GameObject* owner) : Component(owner, TYPE) {}

    // true si tiene geometría cargada y alguna submalla que dibujar
    bool HasGeometry() const;
    unsigned int GetVertexCount() const;     // sólo las submallas de este GameObject
    unsigned int GetTriangleCount() const;

    void CopyFrom(const ComponentMesh& other);

    std::shared_ptr<Geometria> geometry;     // nullptr = GameObject vacío (sin malla)
    std::vector<unsigned int> meshIndices;

    // Visualización
    glm::vec3 color = glm::vec3(0.75f);
    bool wireframe = false;
};
