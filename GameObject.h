#pragma once

#include <string>
#include <vector>
#include <memory>
#include "Component.h"

class GameObject {
public:
    GameObject(const std::string& name = "GameObject", GameObject* parent = nullptr);
    ~GameObject();

    void Update();

    // Gestión de Componentes
    Component* CreateComponent(ComponentType type);

    // Búsqueda de componente por tipo: gameObject->GetComponent<ComponentTransform>()
    template <typename T>
    T* GetComponent() {
        for (Component* comp : components) {
            T* target = dynamic_cast<T*>(comp);
            if (target != nullptr) {
                return target;
            }
        }
        return nullptr;
    }

    // Getters y Setters para el editor y la jerarquía
    const std::string& GetName() const { return name; }
    void SetName(const std::string& newName) { name = newName; }

    bool IsActive() const { return active; }
    void SetActive(bool act) { active = act; }

    GameObject* GetParent() const { return parent; }
    void SetParent(GameObject* newParent);

    const std::vector<GameObject*>& GetChildren() const { return children; }
    const std::vector<Component*>& GetComponents() const { return components; }

    void AddChild(GameObject* child);
    void RemoveChild(GameObject* child);

private:
    std::string name;
    bool active = true;
    GameObject* parent = nullptr;

    std::vector<GameObject*> children;
    std::vector<Component*> components;
};