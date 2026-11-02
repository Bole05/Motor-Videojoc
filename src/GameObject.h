#pragma once

#include <string>
#include <vector>
#include <memory>
#include <glm/glm.hpp>
#include "Component.h"
#include "ComponentTransform.h"
#include "ComponentMesh.h"
#include "ComponentTexture.h"

// GameObject: un objeto de la escena formado por componentes.
// Cuando se importa un FBX se crea un GameObject por cada nodo del fichero, formando un árbol:
//   BakerHouse.fbx
//     |- Baker_house
//     |- Chimney
// Todo GameObject se crea con sus 3 componentes básicos: Transform, Mesh y Textura
// (un GameObject vacío tiene el Mesh sin geometría).
class GameObject {
public:
    explicit GameObject(const std::string& name = "GameObject", GameObject* parent = nullptr);
    ~GameObject() = default;

    GameObject(const GameObject&) = delete;
    GameObject& operator=(const GameObject&) = delete;

    // Actualiza sus componentes y, después, a sus hijos
    void Update(float deltaTime);

    // Copia profunda (componentes + hijos). La geometría y las texturas de GPU se comparten.
    std::shared_ptr<GameObject> Clone(GameObject* newParent) const;

    // --- Componentes ---
    // Crea un componente del tipo indicado y lo añade (si ya existe uno, devuelve el existente)
    Component* CreateComponent(ComponentType type);

    // Búsqueda por tipo: gameObject->GetComponent<ComponentMesh>()
    // Se compara el tipo antes del static_cast, así nunca se convierte a un tipo equivocado.
    template <typename T>
    T* GetComponent() const {
        for (const auto& comp : components) {
            if (comp->GetType() == T::TYPE) return static_cast<T*>(comp.get());
        }
        return nullptr;
    }

    const std::vector<std::unique_ptr<Component>>& GetComponents() const { return components; }

    // Matriz mundo = matrices locales desde la raíz hasta este objeto
    glm::mat4 GetWorldMatrix() const;
    // true si este objeto es 'ancestor' o está dentro de él (hijo, nieto...)
    bool IsSelfOrDescendantOf(const GameObject* ancestor) const;
    // true si él y todos sus padres son visibles
    bool IsVisibleInHierarchy() const;

    // --- Datos del GameObject ---
    std::string name;
    std::string filePath;          // Fichero del que se importó (vacío si se creó en el editor)
    bool visible = true;           // Si está oculto, también se ocultan sus hijos

    // Jerarquía
    GameObject* parent = nullptr;
    std::vector<std::shared_ptr<GameObject>> children;

    // Acceso directo a los 3 componentes básicos (los mismos que devuelve GetComponent<T>()).
    // Siempre existen: se crean en el constructor.
    ComponentTransform* transform = nullptr;
    ComponentMesh*      mesh      = nullptr;
    ComponentTexture*   texture   = nullptr;

private:
    std::vector<std::unique_ptr<Component>> components;
};
