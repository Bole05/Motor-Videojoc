#include "GameObject.h"
#include <random>

uint32_t GameObject::GenerateUID() {
    static std::mt19937 generator{ std::random_device{}() };//generador del num aleatorio y cada vez de llamarlo que la genera un num random nuevo 
    std::uniform_int_distribution<uint32_t>dist(1u, UINT32_MAX); //num min es 1 en vez de 0, max es el limite de la variable tipo uint
    return dist(generator); //sacar el num
}

GameObject::GameObject(const std::string& name, GameObject* parent) :uid(GenerateUID()), name(name), parent(parent) {
    transform = static_cast<ComponentTransform*>(CreateComponent(ComponentType::TRANSFORM));
    mesh      = static_cast<ComponentMesh*>(CreateComponent(ComponentType::MESH));
    texture   = static_cast<ComponentTexture*>(CreateComponent(ComponentType::TEXTURE));
}

Component* GameObject::CreateComponent(ComponentType type) {
    // Un GameObject sólo tiene un componente de cada tipo
    for (const auto& comp : components) {
        if (comp->GetType() == type) return comp.get();
    }

    std::unique_ptr<Component> comp;
    switch (type) {
    case ComponentType::TRANSFORM: comp = std::make_unique<ComponentTransform>(this); break;
    case ComponentType::MESH:      comp = std::make_unique<ComponentMesh>(this);      break;
    case ComponentType::TEXTURE:   comp = std::make_unique<ComponentTexture>(this);   break;
    default: return nullptr;
    }
    components.push_back(std::move(comp));
    return components.back().get();
}

void GameObject::Update(float deltaTime) {
    for (const auto& comp : components) {
        if (comp->IsActive()) comp->Update(deltaTime);
    }
    for (const auto& child : children) child->Update(deltaTime);
}

std::shared_ptr<GameObject> GameObject::Clone(GameObject* newParent) const {
    auto copy = std::make_shared<GameObject>(name, newParent);
    copy->filePath = filePath;
    copy->visible = visible;
    copy->transform->CopyFrom(*transform);
    copy->mesh->CopyFrom(*mesh);
    copy->texture->CopyFrom(*texture);
    copy->children.reserve(children.size());
    for (const auto& child : children) {
        copy->children.push_back(child->Clone(copy.get()));
    }
    return copy;
}

glm::mat4 GameObject::GetWorldMatrix() const {
    glm::mat4 world(1.0f);
    for (const GameObject* o = this; o; o = o->parent) {
        world = o->transform->GetLocalMatrix() * world;
    }
    return world;
}

bool GameObject::IsSelfOrDescendantOf(const GameObject* ancestor) const {
    for (const GameObject* p = this; p; p = p->parent) {
        if (p == ancestor) return true;
    }
    return false;
}

bool GameObject::IsVisibleInHierarchy() const {
    for (const GameObject* p = this; p; p = p->parent) {
        if (!p->visible) return false;
    }
    return true;
}
