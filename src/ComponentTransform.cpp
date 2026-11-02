#include "ComponentTransform.h"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/matrix_decompose.hpp>
#include <glm/gtx/euler_angles.hpp>

void ComponentTransform::Update(float deltaTime) {
    if (!autoRotate) return;
    rotation.y += autoRotateSpeed * deltaTime;
    if (rotation.y >= 360.0f) rotation.y -= 360.0f;
    if (rotation.y < 0.0f)    rotation.y += 360.0f;
}

glm::mat4 ComponentTransform::GetLocalMatrix() const {
    glm::mat4 local = glm::translate(glm::mat4(1.0f), position);
    local = glm::rotate(local, glm::radians(rotation.y), glm::vec3(0, 1, 0));
    local = glm::rotate(local, glm::radians(rotation.x), glm::vec3(1, 0, 0));
    local = glm::rotate(local, glm::radians(rotation.z), glm::vec3(0, 0, 1));
    local = glm::scale(local, scale);
    return local * baseTransform;
}

bool ComponentTransform::SetFromMatrix(const glm::mat4& trs) {
    glm::vec3 newScale, translation, skew;
    glm::vec4 perspective;
    glm::quat orientation;
    if (!glm::decompose(trs, newScale, orientation, translation, skew, perspective)) return false;

    float yaw = 0.0f, pitch = 0.0f, roll = 0.0f;
    glm::extractEulerAngleYXZ(glm::mat4_cast(orientation), yaw, pitch, roll);
    position = translation;
    rotation = glm::degrees(glm::vec3(pitch, yaw, roll));   // (X, Y, Z)
    scale    = newScale;
    return true;
}

void ComponentTransform::Reset() {
    position = glm::vec3(0.0f);
    rotation = glm::vec3(0.0f);
    scale    = glm::vec3(1.0f);
}

void ComponentTransform::CopyFrom(const ComponentTransform& other) {
    SetActive(other.IsActive());
    position = other.position;
    rotation = other.rotation;
    scale = other.scale;
    baseTransform = other.baseTransform;
    autoRotate = other.autoRotate;
    autoRotateSpeed = other.autoRotateSpeed;
}
