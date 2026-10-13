#pragma once

class GameObject;//No usamos include para que llama varia vez el include

enum class ComponentType{TRANSFORM,MESH,MATERIAL,UNKNOWN};

class Component {
public:
	Component(GameObject* Owner, ComponentType type) :owner(owner), type(type),active(false){}
	virtual ~Component() = default;
	virtual void Enable();
	virtual void Update();
	virtual void OnEditor();

	ComponentType GetType() const { return type; }
	bool IsActive() const { return active; }
	void SetActive(bool act) { active = act; }
	GameObject* GetOwner() const { return owner; }
private:
	ComponentType type=ComponentType::UNKNOWN;
	bool active=true;
	GameObject* owner=nullptr;

};