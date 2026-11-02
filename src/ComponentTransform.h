#pragma once

#include <glm/glm.hpp>
#include "Component.h"

// Componente Transform: posición, rotación y escala del GameObject respecto a su padre.
class ComponentTransform : public Component {
public:
    static constexpr ComponentType TYPE = ComponentType::TRANSFORM;

    explicit ComponentTransform(GameObject* owner) : Component(owner, TYPE) {}

    void Update(float deltaTime) override;   // giro automático (opcional)

    // Matriz local: Posición -> Rotación (YXZ) -> Escala -> transform propio del FBX
    glm::mat4 GetLocalMatrix() const;
    // Pone posición / rotación / escala a partir de una matriz TRS (false si no se puede descomponer)
    bool SetFromMatrix(const glm::mat4& trs);
    void Reset();

    // Copia los datos de otro Transform (para duplicar GameObjects)
    void CopyFrom(const ComponentTransform& other);

    glm::vec3 position = glm::vec3(0.0f);
    glm::vec3 rotation = glm::vec3(0.0f);   // Ángulos en grados (X, Y, Z)
    glm::vec3 scale    = glm::vec3(1.0f);

    // Transform propio del nodo según el FBX (se aplica después del Transform editable)
    glm::mat4 baseTransform = glm::mat4(1.0f);

    // Animación de giro continuo alrededor de Y (extra)
    bool autoRotate = false;
    float autoRotateSpeed = 30.0f;          // Grados por segundo
};
