#pragma once

#include <memory>
#include "Component.h"

class Texture;

// Componente Textura: qué textura usa el GameObject al dibujarse.
// Por defecto usa las texturas de los materiales del FBX (las guarda la Geometria);
// si se arrastra una imagen encima, se guarda aquí como textura manual (overrideTexture).
class ComponentTexture : public Component {
public:
    static constexpr ComponentType TYPE = ComponentType::TEXTURE;

    explicit ComponentTexture(GameObject* owner) : Component(owner, TYPE) {}

    bool HasOverride() const;               // true si tiene una textura manual cargada

    void CopyFrom(const ComponentTexture& other);

    bool showTexture = true;                // Mostrar / ocultar la textura de este objeto
    std::shared_ptr<Texture> overrideTexture;
    bool useCheckerTexture = false;         // Textura de comprobación (cuadros blancos y negros)
};
