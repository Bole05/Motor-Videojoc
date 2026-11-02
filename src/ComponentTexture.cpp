#include "ComponentTexture.h"
#include "Texture.h"

bool ComponentTexture::HasOverride() const {
    return overrideTexture && overrideTexture->IsLoaded();
}

void ComponentTexture::CopyFrom(const ComponentTexture& other) {
    SetActive(other.IsActive());
    showTexture = other.showTexture;
    overrideTexture = other.overrideTexture;   // la textura de GPU se comparte
    useCheckerTexture = other.useCheckerTexture;
}
