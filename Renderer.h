#pragma once

#include <glad/glad.h>
#include <string>
#include <vector>
#include <memory>
#include <glm/glm.hpp>
#include "Window.h"
#include "Texture.h"
#include "Geometria.h"
#include "ModelObject.h"

// Renderer: gestiona el renderizado 3D con OpenGL y la interfaz de usuario con ImGui.
class Renderer {
public:
    Renderer();
    ~Renderer();

    // Inicializa OpenGL, luces, textura demo e ImGui
    bool Init(Window& window);

    // Actualiza animaciones (giro continuo de modelos, etc.)
    void Update(float deltaTime = 0.016f);

    // Dibuja la escena completa (OpenGL + paneles ImGui) e intercambia buffers
    void Render(Window& window);

    // Libera recursos de GPU e ImGui
    void CleanUp();

    // --- Gestión de Escena (Modelos FBX / OBJ) ---
    bool AddModel(const std::string& path);
    void DuplicateSelected();
    void DeleteSelected();
    void ClearScene();

    // --- Controles de Cámara Orbital ---
    void OnMouseMoved(float deltaX, float deltaY);
    void OnMouseWheel(float wheelY);
    void ResetCamera();

    // --- Getters / Setters ---
    int GetSelectedIndex() const { return selectedObjectIndex; }
    void SetSelectedIndex(int index) { selectedObjectIndex = index; }
    std::shared_ptr<ModelObject> GetSelectedObject();
    size_t GetObjectCount() const { return objects.size(); }

    bool IsMouseDragging() const { return isMouseDragging; }
    void SetMouseDragging(bool dragging) { isMouseDragging = dragging; }

private:
    void InitCube();
    void RenderCube();
    void RenderGrid();
    void RenderObjects(const glm::mat4& view);
    void DrawUI(Window& window);

    // Recursos del cubo demo
    Texture texture;
    GLuint buffs[3] = {0, 0, 0};
    float angle = 0.0f;

    // Escena: lista de objetos y selección
    std::vector<std::shared_ptr<ModelObject>> objects;
    int selectedObjectIndex = -1;

    // Cámara orbital estilo aplicación de modelado 3D
    float cameraDistance = 3.5f;
    float cameraYaw = 0.0f;
    float cameraPitch = 20.0f;
    glm::vec3 cameraTarget = glm::vec3(0.0f, 0.0f, 0.0f);
    bool isMouseDragging = false;

    // Opciones visuales
    bool showGrid = true;
    bool showDemoCube = false;
    bool globalWireframe = false;
    glm::vec3 clearColor = glm::vec3(0.12f, 0.12f, 0.16f);

    // Buffer para diálogo de carga manual
    char loadPathBuffer[512] = "";
    bool showLoadModal = false;
};
