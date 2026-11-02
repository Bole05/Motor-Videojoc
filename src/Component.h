#pragma once

class GameObject; // Declaración adelantada: evita incluir GameObject.h (y la inclusión circular)

// Tipos de componente. Cada componente concreto guarda el suyo en una constante TYPE,
// así GameObject::GetComponent<T>() puede comprobar el tipo y usar static_cast con seguridad.
enum class ComponentType { TRANSFORM, MESH, TEXTURE, UNKNOWN };

// Clase base de todos los componentes de un GameObject (patrón Component).
class Component {
public:
    Component(GameObject* owner, ComponentType type) : type(type), owner(owner) {}
    virtual ~Component() = default;

    // Un componente pertenece a un único GameObject: no se copia con el constructor de copia.
    // (Para duplicar un GameObject se usa CopyFrom, que copia los datos pero no el dueño.)
    Component(const Component&) = delete;
    Component& operator=(const Component&) = delete;

    // Se llama una vez por frame desde GameObject::Update
    virtual void Update(float /*deltaTime*/) {}

    ComponentType GetType() const { return type; }
    bool IsActive() const { return active; }
    void SetActive(bool act) { active = act; }
    GameObject* GetOwner() const { return owner; }

private:
    ComponentType type = ComponentType::UNKNOWN;
    bool active = true;
    GameObject* owner = nullptr;
};
