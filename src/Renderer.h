#pragma once

#include <glad/glad.h>
#include <string>
#include <vector>
#include <memory>
#include <glm/glm.hpp>
#include "Window.h"
#include "Texture.h"
#include "Geometria.h"
#include "GameObject.h"
#include "Shader.h"

// Renderer (parte del MOTOR): escena de GameObjects, cámara, selección y render 3D con OpenGL.
// No sabe nada de ImGui: toda la interfaz (paneles, menús, inspector...) vive en la clase Editor,
// que lee y cambia las opciones del Renderer (por eso Editor es friend).
class Renderer {
    friend class Editor;
public:
    Renderer();
    ~Renderer();

    // Inicializa OpenGL, luces, shaders y carga el modelo inicial (BakerHouse)
    bool Init();

    // Mueve la cámara (WASD) y actualiza los GameObjects.
    // keyboardFree = false si el usuario está escribiendo en un campo de texto del editor.
    void Update(float deltaTime, bool keyboardFree);

    // Dibuja la escena 3D (la interfaz del editor se dibuja después, encima)
    void Render(const Window& window);

    // Libera los recursos de GPU
    void CleanUp();

    // --- Gestión de Escena (Modelos FBX / OBJ) ---
    bool AddModel(const std::string& path);
    // Carga una imagen y la asigna como textura al objeto seleccionado
    bool ApplyTextureToSelected(const std::string& path);
    void DuplicateSelected();
    void DeleteSelected();
    void ClearScene();

    // Crea un GameObject vacío (sin malla), como "Create Empty" de Unity.
    // Si parent es nullptr se crea en la raíz de la escena.
    std::shared_ptr<GameObject> CreateEmptyObject(GameObject* parent = nullptr);

    // Mueve un objeto a otro padre (nullptr = raíz) en la posición 'index' entre sus
    // hermanos (-1 = al final). Conserva su posición en el mundo, como en Unity.
    // Devuelve false si el movimiento no es válido (p.ej. meterlo dentro de su propio hijo).
    bool ReparentObject(GameObject* obj, GameObject* newParent, int index = -1);

    // Crea una geometría básica (cubo, esfera...) delante de la cámara
    void AddPrimitive(PrimitiveType type);

    // Límite de FPS elegido en el panel Configuración (0 = sin límite). Lo aplica Engine::Run.
    int GetFpsLimit() const { return fpsLimit; }

    // --- Selección con el ratón en la vista 3D ---
    // (x, y) en píxeles de la ventana. Selecciona la pieza del modelo que hay bajo el
    // ratón (p.ej. la chimenea). Con selectWholeModel = true se selecciona el modelo raíz.
    // Si no hay nada debajo, se deselecciona.
    void PickAt(float x, float y, float windowWidth, float windowHeight, bool selectWholeModel = false);

    // --- Controles de Cámara (los del README) ---
    //  Clic derecho (mantener) + WASD     : moverse tipo FPS          (Update)
    //  Clic derecho (mantener) + ratón    : mirar alrededor            (FreeLook)
    //  Rueda del ratón                    : zoom in / out              (OnMouseWheel)
    //  ALT + clic izquierdo + arrastrar   : orbitar alrededor del objeto seleccionado (Orbit)
    //  Tecla F                            : enfocar el objeto seleccionado (FocusSelected)
    //  SHIFT (mantener)                   : velocidad x2 al moverse
    void BeginFreeLook()  { freeLookActive = true; }
    void EndFreeLook()    { freeLookActive = false; }
    bool IsFreeLookActive() const { return freeLookActive; }
    void FreeLook(float deltaX, float deltaY);

    void BeginOrbit();
    void EndOrbit()       { orbitActive = false; }
    bool IsOrbitActive() const { return orbitActive; }
    void Orbit(float deltaX, float deltaY);

    void BeginPan()       { panActive = true; }   // extra: clic central + arrastrar = desplazar
    void EndPan()         { panActive = false; }
    bool IsPanActive() const { return panActive; }
    void Pan(float deltaX, float deltaY);

    void OnMouseWheel(float wheelY);
    void FocusSelected();
    void ResetCamera();

    // --- Getters / Setters ---
    // El objeto seleccionado puede ser un modelo entero o un nodo hijo (p.ej. la chimenea)
    std::shared_ptr<GameObject> GetSelectedObject() { return selectedObject; }
    void SetSelectedObject(const std::shared_ptr<GameObject>& obj) { selectedObject = obj; }
    size_t GetObjectCount() const { return objects.size(); }


private:
    void RenderGrid();
    void RenderObjects(const glm::mat4& view);

    // --- Modo Debug: cajas envolventes (AABB / OBB) ---
    // Dibuja las cajas de 'obj' y sus hijos. Devuelve en outMin/outMax el AABB mundo
    // que envuelve al nodo y a todos sus descendientes (false si no hay geometría).
    bool RenderDebugNode(const std::shared_ptr<GameObject>& obj, const glm::mat4& parentWorld,
                         bool inheritedSelected, glm::vec3& outMin, glm::vec3& outMax);
    void RenderDebug(const glm::mat4& view);

    // Resaltado de la selección: vuelve a dibujar la pieza seleccionada (y sus hijos)
    // con un relleno de color semitransparente y su malla en alambre por encima
    void RenderSelectionHighlight(const glm::mat4& view);
    void RenderHighlightNode(const GameObject* obj, const glm::mat4& world, const glm::mat4& view, int pass);
    // AABB en el mundo de un nodo (sólo su malla, o incluyendo hijos)
    bool ComputeWorldAABB(const GameObject* obj, bool includeChildren, glm::vec3& outMin, glm::vec3& outMax) const;
    // Dibuja un nodo y, recursivamente, todos sus hijos
    void RenderNode(const std::shared_ptr<GameObject>& obj, const glm::mat4& parentWorld, const glm::mat4& view,
                    const Texture* inheritedTexture, bool inheritedShowTexture, bool inheritedWireframe,
                    bool inheritedSelected);
    void CreateCheckerTexture();

    // --- Shaders (GLSL) ---
    // Carga assets/shaders/default.vert/.frag (o el código por defecto si no se encuentran)
    void InitShaders();
    // Utilidades de la jerarquía
    std::shared_ptr<GameObject> FindShared(const GameObject* raw) const;
    std::vector<std::shared_ptr<GameObject>>& SiblingList(GameObject* parent) { return parent ? parent->children : objects; }
    std::string MakeUniqueName(const std::string& base) const;

    // Escena: lista de objetos raíz (cada uno puede tener hijos) y selección
    std::vector<std::shared_ptr<GameObject>> objects;
    std::shared_ptr<GameObject> selectedObject;

    // Matrices del último frame (para convertir el clic del ratón en un rayo 3D)
    glm::mat4 lastView = glm::mat4(1.0f);
    glm::mat4 lastProjection = glm::mat4(1.0f);

    // Cámara libre: posición + orientación (yaw / pitch en grados).
    // La cámara mira en la dirección GetCameraForward(). cameraFocusDistance es la distancia
    // al punto que se está mirando (se usa para orbitar cuando no hay nada seleccionado).
    glm::vec3 cameraPosition = glm::vec3(0.0f);
    float cameraYaw = 0.0f;
    float cameraPitch = 20.0f;
    float cameraFocusDistance = 3.5f;
    float cameraMoveSpeed = 2.5f;     // unidades por segundo (x2 con SHIFT)
    float cameraLookSensitivity = 0.25f;
    float cameraOrbitSensitivity = 0.35f;
    bool freeLookActive = false;
    bool orbitActive = false;
    bool panActive = false;
    glm::vec3 orbitPivot = glm::vec3(0.0f);
    glm::vec3 GetCameraForward() const;
    glm::vec3 GetCameraRight() const;
    // Coloca la cámara a 'distance' de 'target' según el yaw / pitch actuales
    void PlaceCameraAround(const glm::vec3& target, float distance);

    // Modo Debug (F1 o menú "Debug")
    bool debugMode = false;
    bool debugShowAABB = true;          // Caja alineada con los ejes del mundo (verde)
    bool debugShowOBB = false;          // Caja orientada: sigue la rotación del objeto (amarillo)
    bool debugShowGroupAABB = true;     // Caja que envuelve un padre con todos sus hijos (azul)
    bool debugOnlySelected = false;     // Sólo el objeto seleccionado (y sus hijos)
    bool debugThroughObjects = true;    // Ver las cajas a través de las mallas (sin depth test)
    float debugLineWidth = 1.5f;
    bool debugShowVertexNormals = false; // Normales de vértice importadas (cian)
    bool debugShowFaceNormals = false;   // Normales de cara, calculadas por triángulo (magenta)
    float debugNormalLength = 0.05f;     // Longitud de las líneas de normales (unidades de mundo)

    // Resaltado de la selección (menú Vista)
    bool showSelectionHighlight = true;
    bool selectionPulse = true;                                // el relleno "late" suavemente
    glm::vec3 selectionColor = glm::vec3(1.0f, 0.55f, 0.1f);  // naranja, como en Blender/Unity

    // Shaders: con useShaders = false se usa el pipeline fijo de OpenGL de antes (para comparar)
    Shader shader;
    bool useShaders = true;
    int shaderMode = 0;              // 0 Blinn-Phong, 1 Normales, 2 Toon, 3 Sin luz, 4 UVs
    float shaderAmbient = 0.30f;
    float shaderSpecular = 0.35f;
    float shaderShininess = 32.0f;
    glm::vec3 lightDirection = glm::vec3(5.0f, 10.0f, 8.0f);   // hacia la luz (luz direccional)
    glm::vec3 lightColor = glm::vec3(1.0f);
    glm::vec3 lastCameraPos = glm::vec3(0.0f);
    std::string vertexShaderPath, fragmentShaderPath;   // ficheros cargados (vacío = código por defecto)

    // Escena 3D visible (menú View > Escena)
    bool showScene = true;

    // Opciones del render (panel Configuración)
    int fpsLimit = 0;                 // 0 = sin límite
    bool vsync = true;
    bool depthTest = true;
    bool cullFace = false;
    bool fixedLighting = true;        // iluminación del pipeline fijo (cuando no se usan shaders)

    // Textura de comprobación (checker) compartida por todos los objetos que la usan
    std::shared_ptr<Texture> checkerTexture;

    // Opciones visuales
    bool showGrid = true;
    bool globalWireframe = false;
    bool globalShowTextures = true;   // Interruptor global de texturas
    glm::vec3 clearColor = glm::vec3(0.12f, 0.12f, 0.16f);
};
