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

    // Inicializa OpenGL, luces, ImGui y carga el modelo inicial (BakerHouse)
    bool Init(Window& window);

    // Actualiza animaciones (giro continuo de modelos, etc.)
    void Update(float deltaTime = 0.016f);

    // Dibuja la escena completa (OpenGL + paneles ImGui) e intercambia buffers
    void Render(Window& window);

    // Libera recursos de GPU e ImGui
    void CleanUp();

    // --- Gestión de Escena (Modelos FBX / OBJ) ---
    bool AddModel(const std::string& path);
    // Decide qué hacer con un fichero arrastrado: modelo (fbx/obj...) o imagen (png/dds/jpg...)
    void HandleDroppedFile(const std::string& path);
    // Carga una imagen y la asigna como textura al objeto seleccionado
    bool ApplyTextureToSelected(const std::string& path);
    void DuplicateSelected();
    void DeleteSelected();
    void ClearScene();

    // --- Controles de Cámara Orbital ---
    void OnMouseMoved(float deltaX, float deltaY);
    void OnMouseWheel(float wheelY);
    void ResetCamera();

    // --- Getters / Setters ---
    // El objeto seleccionado puede ser un modelo entero o un nodo hijo (p.ej. la chimenea)
    std::shared_ptr<ModelObject> GetSelectedObject() { return selectedObject; }
    void SetSelectedObject(const std::shared_ptr<ModelObject>& obj) { selectedObject = obj; }
    size_t GetObjectCount() const { return objects.size(); }

    bool IsMouseDragging() const { return isMouseDragging; }
    void SetMouseDragging(bool dragging) { isMouseDragging = dragging; }

private:
    void RenderGrid();
    void RenderObjects(const glm::mat4& view);
    // Dibuja un nodo y, recursivamente, todos sus hijos
    void RenderNode(const std::shared_ptr<ModelObject>& obj, const glm::mat4& parentWorld, const glm::mat4& view,
                    const Texture* inheritedTexture, bool inheritedShowTexture, bool inheritedWireframe,
                    bool inheritedSelected);
    // Pinta un nodo en el panel de jerarquía (TreeNode de ImGui) y sus hijos
    void DrawHierarchyNode(const std::shared_ptr<ModelObject>& obj);
    void DrawUI(Window& window);

    // Acciones pedidas desde el menú contextual de la jerarquía.
    // Se ejecutan al terminar de dibujar el árbol para no modificarlo mientras se recorre.
    enum class PendingAction { NONE, DUPLICATE, DELETE_OBJ };
    PendingAction pendingAction = PendingAction::NONE;

    // Escena: lista de objetos raíz (cada uno puede tener hijos) y selección
    std::vector<std::shared_ptr<ModelObject>> objects;
    std::shared_ptr<ModelObject> selectedObject;

    // Cámara orbital estilo aplicación de modelado 3D
    float cameraDistance = 3.5f;
    float cameraYaw = 0.0f;
    float cameraPitch = 20.0f;
    glm::vec3 cameraTarget = glm::vec3(0.0f, 0.0f, 0.0f);
    bool isMouseDragging = false;

    // Opciones visuales
    bool showGrid = true;
    bool globalWireframe = false;
    bool globalShowTextures = true;   // Interruptor global de texturas
    glm::vec3 clearColor = glm::vec3(0.12f, 0.12f, 0.16f);

    // Buffer para diálogo de carga manual
    char loadPathBuffer[512] = "";
    bool showLoadModal = false;
};
