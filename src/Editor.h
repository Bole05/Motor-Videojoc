#pragma once

#include <SDL3/SDL.h>
#include <glm/glm.hpp>
#include <memory>
#include <string>
#include <vector>
#include "Window.h"

class Renderer;
class GameObject;
class Geometria;

// Editor (parte del EDITOR): toda la interfaz hecha con Dear ImGui.
//  - Menú principal (File, GameObject, Geometrías Básicas, View, Debug, Help)
//  - Paneles: Jerarquía, Inspector, Consola, Configuración, About y Editor de Shaders
//  - Drag & drop de ficheros sobre la ventana (decide si es un modelo o una textura)
// El motor (Renderer) no conoce al Editor: el Editor le pide las acciones (cargar, duplicar,
// enfocar...) y muestra / cambia sus opciones.
class Editor {
public:
    explicit Editor(Renderer& renderer) : renderer(renderer) {}
    ~Editor();

    Editor(const Editor&) = delete;
    Editor& operator=(const Editor&) = delete;

    // Crea el contexto de ImGui y lo conecta con SDL3 + OpenGL. Llamar después de Renderer::Init.
    bool Init(Window& window);
    void CleanUp();

    // Pasa un evento de SDL a ImGui
    void ProcessEvent(const SDL_Event& event);
    // true si ImGui está usando el ratón / el teclado (el motor no debe reaccionar)
    bool WantCaptureMouse() const;
    bool WantTextInput() const;

    // Guarda el tiempo del frame para las gráficas de FPS / ms del panel Configuración
    void RecordFrameTime(float deltaTime);

    // Un frame de interfaz: BeginFrame -> Draw (paneles) -> [render 3D del motor] -> EndFrame
    void BeginFrame();
    void Draw(Window& window);
    void EndFrame();   // dibuja ImGui encima de la escena 3D

    // --- Drag & drop de ficheros sobre la ventana ---
    // Modelo (fbx/obj...) -> se carga en la escena. Imagen (png/dds/jpg...) -> textura:
    // si se suelta encima de la miniatura de un material en el Inspector se sustituye esa
    // textura; si no, se aplica al GameObject seleccionado.
    void HandleDroppedFile(const std::string& path, float x = -1.0f, float y = -1.0f);
    void OnFileDragBegin() { isDraggingFile = true; }
    void OnFileDragMove(float x, float y) { isDraggingFile = true; dragPos = glm::vec2(x, y); }
    void OnFileDragEnd() { isDraggingFile = false; }

private:
    void DrawUI(Window& window);
    void DrawMainMenuBar(Window& window);
    void DrawHierarchyPanel();
    void DrawHierarchyNode(const std::shared_ptr<GameObject>& obj);
    void DrawInspectorPanel(Window& window);
    void DrawConsolePanel();
    void DrawConfigPanel(Window& window);
    void DrawAboutWindow();
    void DrawShaderEditor();
    void CompileShaderFromEditor();
    void CopyShaderSourcesToEditor();

    Renderer& renderer;
    bool initialized = false;

    // Ventanas del editor (se muestran / ocultan desde el menú View)
    bool showHierarchy = true;
    bool showInspector = true;
    bool showConsole = true;
    bool showConfig = false;
    bool showAbout = false;
    bool showShaderEditor = false;

    // Acciones pedidas desde el menú contextual de la jerarquía.
    // Se ejecutan al terminar de dibujar el árbol para no modificarlo mientras se recorre.
    enum class PendingAction { NONE, DUPLICATE, DELETE_OBJ, CREATE_EMPTY, CREATE_EMPTY_CHILD };
    PendingAction pendingAction = PendingAction::NONE;

    // Arrastrar y soltar dentro de la jerarquía: se aplica al terminar de dibujar el árbol
    struct PendingReparent {
        GameObject* obj = nullptr;
        GameObject* newParent = nullptr;
        int index = -1;
        bool active = false;
    };
    PendingReparent pendingReparent;

    // Si la selección cambia desde fuera de la jerarquía (clic en la vista 3D, duplicar...),
    // la jerarquía abre los padres y se desplaza hasta el objeto seleccionado
    const GameObject* lastSelected = nullptr;
    bool revealSelectionInHierarchy = false;

    // Editor de shaders
    std::vector<char> vertexEditBuf, fragmentEditBuf;   // texto del editor
    std::string shaderStatus;
    bool shaderStatusOk = true;

    // Consola
    bool consoleShowInfo = true, consoleShowWarnings = true, consoleShowErrors = true;
    bool consoleAutoScroll = true;
    char consoleFilter[128] = "";
    size_t consoleLastCount = 0;

    // Configuración: historial de FPS
    static const int FPS_HISTORY = 100;
    float fpsHistory[FPS_HISTORY] = {};
    float msHistory[FPS_HISTORY] = {};
    int fpsHistoryIndex = 0;
    char windowTitle[128] = "Motor 3D - Model Viewer & Editor";

    // Zonas del Inspector donde se puede soltar una imagen para sustituir la textura
    // de un material (se rellenan cada frame al dibujar el Inspector)
    struct TextureDropZone {
        glm::vec2 min, max;
        std::shared_ptr<Geometria> geom;
        unsigned int material = 0;
    };
    std::vector<TextureDropZone> textureDropZones;
    const TextureDropZone* FindDropZone(const glm::vec2& p) const;
    bool isDraggingFile = false;
    glm::vec2 dragPos = glm::vec2(-1.0f);

    // Diálogo de carga manual
    char loadPathBuffer[512] = "";
    bool showLoadModal = false;
};
