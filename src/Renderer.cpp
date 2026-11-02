#include "Renderer.h"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <functional>
#include <set>
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <cfloat>
#include "Log.h"

// Segundos desde que arrancó SDL (para animaciones: pulso de la selección, uTime del shader)
static float GetTimeSeconds() { return (float)SDL_GetTicks() / 1000.0f; }

// ─────────────────────────────────────────────
//  Constructor / Destructor
// ─────────────────────────────────────────────

Renderer::Renderer() {}

Renderer::~Renderer() {
    CleanUp();
}

// ─────────────────────────────────────────────
//  Init: OpenGL + Luz + Shaders + Modelo inicial (BakerHouse)
// ─────────────────────────────────────────────

bool Renderer::Init() {
    // 1. Configurar GL_LIGHT0 (luz blanca difusa y especular)
    GLfloat lightAmbient[]  = { 0.35f, 0.35f, 0.35f, 1.0f };
    GLfloat lightDiffuse[]  = { 0.9f,  0.9f,  0.9f,  1.0f };
    GLfloat lightSpecular[] = { 0.6f,  0.6f,  0.6f,  1.0f };
    glLightfv(GL_LIGHT0, GL_AMBIENT,  lightAmbient);
    glLightfv(GL_LIGHT0, GL_DIFFUSE,  lightDiffuse);
    glLightfv(GL_LIGHT0, GL_SPECULAR, lightSpecular);
    glEnable(GL_LIGHT0);
    glLightModeli(GL_LIGHT_MODEL_TWO_SIDE, GL_TRUE);

    LOG("Renderer inicializado correctamente.");

    // 2b. Shaders GLSL
    InitShaders();

    // Posición inicial de la cámara
    ResetCamera();

    // VSync (se puede cambiar en el panel Configuración)
    SDL_GL_SetSwapInterval(vsync ? 1 : 0);

    // Mensajes de Assimp -> Consola, y textura de comprobación
    Geometria::DebugInit();
    CreateCheckerTexture();

    // 3. Cargar la casa (BakerHouse.fbx) automáticamente al arrancar el motor.
    //    Probamos primero la ruta relativa al directorio de trabajo y, si no existe,
    //    la carpeta assets que hay junto al ejecutable.
    //    Antes de cargar se comprueba que el fichero exista, para no llenar la Consola de
    //    errores falsos (Visual Studio ejecuta desde build/, donde no hay carpeta assets).
    std::vector<std::string> houseCandidates = { "assets/BakerHouse.fbx" };
    if (const char* basePath = SDL_GetBasePath()) {
        houseCandidates.push_back(std::string(basePath) + "assets/BakerHouse.fbx");
    }
    bool houseLoaded = false;
    for (const auto& candidate : houseCandidates) {
        std::error_code ec;
        if (std::filesystem::exists(candidate, ec)) {
            houseLoaded = AddModel(candidate);
            break;
        }
    }
    if (!houseLoaded) {
        LOG("Error: no se ha podido cargar assets/BakerHouse.fbx al iniciar (no esta junto al ejecutable).");
    }

    return true;
}

// ─────────────────────────────────────────────
//  Update: animaciones de rotación
// ─────────────────────────────────────────────

void Renderer::Update(float deltaTime, bool keyboardFree) {
    // Clic derecho (mantener) + WASD: moverse tipo FPS. SHIFT duplica la velocidad.
    if (freeLookActive && keyboardFree) {
        const bool* keys = SDL_GetKeyboardState(nullptr);
        glm::vec3 move(0.0f);
        if (keys[SDL_SCANCODE_W]) move += GetCameraForward();
        if (keys[SDL_SCANCODE_S]) move -= GetCameraForward();
        if (keys[SDL_SCANCODE_D]) move += GetCameraRight();
        if (keys[SDL_SCANCODE_A]) move -= GetCameraRight();
        if (glm::length(move) > 0.0f) {
            float speed = cameraMoveSpeed;
            if (keys[SDL_SCANCODE_LSHIFT] || keys[SDL_SCANCODE_RSHIFT]) speed *= 2.0f;
            cameraPosition += glm::normalize(move) * speed * deltaTime;
        }
    }

    // Actualizar los GameObjects (cada uno actualiza sus componentes y sus hijos)
    for (const auto& obj : objects) obj->Update(deltaTime);
}

// ─────────────────────────────────────────────
//  Gestión de Escena
// ─────────────────────────────────────────────

// Crea un GameObject para un nodo del FBX y, recursivamente, para todos sus hijos.
// La matriz local del nodo se pasa a Posición / Rotación / Escala del Transform, para que
// se pueda editar desde el Inspector igual que cualquier objeto.
static std::shared_ptr<GameObject> CreateNodeObject(const ModelNode& node, const std::shared_ptr<Geometria>& geom,
                                                    const std::string& path, GameObject* parent) {
    auto obj = std::make_shared<GameObject>(node.name, parent);
    obj->filePath = path;
    obj->mesh->geometry = geom;
    obj->mesh->meshIndices = node.meshes;
    if (!obj->transform->SetFromMatrix(node.transform)) {
        obj->transform->baseTransform = node.transform; // No se pudo descomponer: se usa la matriz tal cual
    }

    obj->children.reserve(node.children.size());
    for (const auto& childNode : node.children) {
        obj->children.push_back(CreateNodeObject(childNode, geom, path, obj.get()));
    }
    return obj;
}

bool Renderer::AddModel(const std::string& path) {
    // Extraer el nombre de fichero sin directorios
    std::string fileName = path;
    size_t lastSlash = fileName.find_last_of("/\\");
    if (lastSlash != std::string::npos) {
        fileName = fileName.substr(lastSlash + 1);
    }

    auto geom = std::make_shared<Geometria>();
    if (!geom->Load(path)) {
        LOG("Error al cargar modelo desde: %s", path.c_str());
        return false;
    }

    // Objeto raíz con el nombre del fichero. Su baseTransform centra y escala el modelo
    // entero; debajo cuelgan los nodos del FBX (p.ej. Baker_house y Chimney).
    const ModelNode& rootNode = geom->GetRootNode();
    auto obj = std::make_shared<GameObject>(fileName);
    obj->filePath = path;
    obj->mesh->geometry = geom;
    obj->mesh->meshIndices = rootNode.meshes;
    obj->transform->baseTransform = geom->GetNormalizeMatrix() * rootNode.transform;
    for (const auto& childNode : rootNode.children) {
        obj->children.push_back(CreateNodeObject(childNode, geom, path, obj.get()));
    }

    // Si ya hay otros objetos en la escena, colocarlo ligeramente al lado
    if (!objects.empty()) {
        obj->transform->position.x = (float)objects.size() * 1.5f;
    }

    objects.push_back(obj);
    selectedObject = obj;

    LOG("Objeto '%s' anadido a la escena con %zu nodos hijos. Total objetos: %zu",
        obj->name.c_str(), obj->children.size(), objects.size());
    return true;
}

bool Renderer::ApplyTextureToSelected(const std::string& path) {
    auto sel = GetSelectedObject();
    if (!sel) {
        LOG("Selecciona un objeto antes de arrastrar una textura: %s", path.c_str());
        return false;
    }
    auto tex = std::make_shared<Texture>();
    if (!tex->Load(path)) {
        LOG("No se pudo cargar la textura: %s", path.c_str());
        return false;
    }
    sel->texture->overrideTexture = tex;
    sel->texture->showTexture = true;
    LOG("Textura '%s' asignada a '%s'", path.c_str(), sel->name.c_str());
    return true;
}

void Renderer::DuplicateSelected() {
    if (!selectedObject) return;

    GameObject* parent = selectedObject->parent;
    auto copy = selectedObject->Clone(parent);
    copy->name = selectedObject->name + " (Copia)";

    if (parent) {
        // Nodo hijo: la copia queda como hermano, dentro del mismo padre
        parent->children.push_back(copy);
    } else {
        copy->transform->position.x += 1.2f; // Desplazamiento visible para distinguirlo
        objects.push_back(copy);
    }
    selectedObject = copy;
    LOG("Objeto duplicado como: %s", copy->name.c_str());
}

void Renderer::DeleteSelected() {
    if (!selectedObject) return;

    LOG("Eliminando objeto: %s", selectedObject->name.c_str());
    GameObject* parent = selectedObject->parent;
    auto& list = parent ? parent->children : objects;
    auto it = std::find(list.begin(), list.end(), selectedObject);
    if (it != list.end()) {
        list.erase(it);
    }

    // Tras borrar un hijo seleccionamos su padre; tras borrar un modelo, el último de la escena
    selectedObject.reset();
    if (parent) {
        for (auto& root : objects) {
            std::function<bool(const std::shared_ptr<GameObject>&)> findParent =
                [&](const std::shared_ptr<GameObject>& o) {
                    if (o.get() == parent) { selectedObject = o; return true; }
                    for (auto& ch : o->children) if (findParent(ch)) return true;
                    return false;
                };
            if (findParent(root)) break;
        }
    } else if (!objects.empty()) {
        selectedObject = objects.back();
    }
}

void Renderer::ClearScene() {
    objects.clear();
    selectedObject.reset();
    LOG("Escena vaciada.");
}

// ─────────────────────────────────────────────
//  Jerarquía: GameObjects vacíos y cambio de padre
// ─────────────────────────────────────────────

// Atajos: matriz local / mundo de un nodo (las calcula su componente Transform)
static glm::mat4 LocalMatrix(const GameObject& obj) { return obj.transform->GetLocalMatrix(); }
static glm::mat4 WorldMatrix(const GameObject* obj) { return obj->GetWorldMatrix(); }

// true si 'node' es 'ancestor' o está dentro de él (hijo, nieto...)
static bool IsSelfOrDescendant(const GameObject* node, const GameObject* ancestor) {
    return node && node->IsSelfOrDescendantOf(ancestor);
}

std::shared_ptr<GameObject> Renderer::FindShared(const GameObject* raw) const {
    if (!raw) return nullptr;
    std::function<std::shared_ptr<GameObject>(const std::vector<std::shared_ptr<GameObject>>&)> search =
        [&](const std::vector<std::shared_ptr<GameObject>>& list) -> std::shared_ptr<GameObject> {
            for (const auto& o : list) {
                if (o.get() == raw) return o;
                if (auto found = search(o->children)) return found;
            }
            return nullptr;
        };
    return search(objects);
}

// "GameObject", "GameObject (1)", "GameObject (2)"... sin repetir nombres de la escena
std::string Renderer::MakeUniqueName(const std::string& base) const {
    std::set<std::string> used;
    std::function<void(const std::vector<std::shared_ptr<GameObject>>&)> collect =
        [&](const std::vector<std::shared_ptr<GameObject>>& list) {
            for (const auto& o : list) { used.insert(o->name); collect(o->children); }
        };
    collect(objects);

    if (!used.count(base)) return base;
    for (int i = 1; ; ++i) {
        std::string candidate = base + " (" + std::to_string(i) + ")";
        if (!used.count(candidate)) return candidate;
    }
}

std::shared_ptr<GameObject> Renderer::CreateEmptyObject(GameObject* parent) {
    auto obj = std::make_shared<GameObject>(MakeUniqueName("GameObject"), parent);
    // Sin malla: no se dibuja nada, sólo sirve de contenedor para agrupar otros objetos
    SiblingList(parent).push_back(obj);
    selectedObject = obj;
    LOG("GameObject vacio creado: %s%s%s", obj->name.c_str(),
        parent ? " (hijo de " : "", parent ? (parent->name + ")").c_str() : "");
    return obj;
}

bool Renderer::ReparentObject(GameObject* obj, GameObject* newParent, int index) {
    if (!obj) return false;
    // No se puede meter un objeto dentro de sí mismo ni de uno de sus hijos (crearía un ciclo)
    if (newParent && IsSelfOrDescendant(newParent, obj)) return false;

    std::shared_ptr<GameObject> shared = FindShared(obj);
    if (!shared) return false;

    // Guardar la matriz mundo ANTES de moverlo para que no "salte" de sitio
    const glm::mat4 world = WorldMatrix(obj);

    // Quitarlo de su lista actual
    auto& oldList = SiblingList(obj->parent);
    auto it = std::find(oldList.begin(), oldList.end(), shared);
    if (it == oldList.end()) return false;
    const int oldIndex = (int)std::distance(oldList.begin(), it);
    const bool sameList = (obj->parent == newParent);
    oldList.erase(it);

    // Si se mueve dentro de la misma lista hacia abajo, el índice destino se desplaza uno
    if (sameList && index > oldIndex) --index;

    // Insertarlo en la nueva lista
    auto& newList = SiblingList(newParent);
    if (index < 0 || index > (int)newList.size()) index = (int)newList.size();
    newList.insert(newList.begin() + index, shared);
    obj->parent = newParent;

    // Recalcular Posición / Rotación / Escala locales para conservar la posición en el mundo:
    //   world = parentWorld * TRS * baseTransform  ->  TRS = inv(parentWorld) * world * inv(baseTransform)
    if (!sameList) {
        const glm::mat4 parentWorld = newParent ? WorldMatrix(newParent) : glm::mat4(1.0f);
        const glm::mat4 trs = glm::inverse(parentWorld) * world * glm::inverse(obj->transform->baseTransform);

        obj->transform->SetFromMatrix(trs);
    }

    selectedObject = shared;
    LOG("'%s' movido a %s", obj->name.c_str(), newParent ? newParent->name.c_str() : "la raiz de la escena");
    return true;
}

// ─────────────────────────────────────────────
//  Cámara Orbital
// ─────────────────────────────────────────────

// Dirección en la que mira la cámara. yaw = 0 y pitch = 0 miran hacia -Z;
// pitch positivo = la cámara está "por encima" y mira hacia abajo.
glm::vec3 Renderer::GetCameraForward() const {
    const float y = glm::radians(cameraYaw);
    const float p = glm::radians(cameraPitch);
    return -glm::vec3(std::cos(p) * std::sin(y), std::sin(p), std::cos(p) * std::cos(y));
}

glm::vec3 Renderer::GetCameraRight() const {
    return glm::normalize(glm::cross(GetCameraForward(), glm::vec3(0.0f, 1.0f, 0.0f)));
}

void Renderer::PlaceCameraAround(const glm::vec3& target, float distance) {
    cameraFocusDistance = distance;
    cameraPosition = target - GetCameraForward() * distance;
}

// Clic derecho + ratón: girar la vista sin moverse (free look)
void Renderer::FreeLook(float deltaX, float deltaY) {
    cameraYaw   -= deltaX * cameraLookSensitivity;
    cameraPitch += deltaY * cameraLookSensitivity;
    cameraPitch = glm::clamp(cameraPitch, -89.0f, 89.0f);
}

// ALT + clic izquierdo: el punto alrededor del que se orbita es el objeto seleccionado
// (centro de su caja AABB con sus hijos); si no hay selección, el punto que se está mirando.
void Renderer::BeginOrbit() {
    orbitActive = true;
    glm::vec3 mn, mx;
    if (selectedObject && ComputeWorldAABB(selectedObject.get(), true, mn, mx)) {
        orbitPivot = (mn + mx) * 0.5f;
    } else if (selectedObject) {
        orbitPivot = glm::vec3(WorldMatrix(selectedObject.get())[3]);   // posición del objeto
    } else {
        orbitPivot = cameraPosition + GetCameraForward() * cameraFocusDistance;
    }

    // Orientar la cámara hacia el pivote para que la órbita no dé un salto
    glm::vec3 toCamera = cameraPosition - orbitPivot;
    float dist = glm::length(toCamera);
    if (dist < 0.05f) { dist = cameraFocusDistance; toCamera = -GetCameraForward() * dist; }
    const glm::vec3 d = toCamera / dist;
    cameraPitch = glm::degrees(std::asin(glm::clamp(d.y, -1.0f, 1.0f)));
    cameraYaw   = glm::degrees(std::atan2(d.x, d.z));
    PlaceCameraAround(orbitPivot, dist);
}

void Renderer::Orbit(float deltaX, float deltaY) {
    const float dist = glm::length(cameraPosition - orbitPivot);
    cameraYaw   += deltaX * cameraOrbitSensitivity;
    cameraPitch -= deltaY * cameraOrbitSensitivity;
    cameraPitch = glm::clamp(cameraPitch, -89.0f, 89.0f);
    PlaceCameraAround(orbitPivot, dist);
}

// Extra (no está en el README): clic central + arrastrar desplaza la cámara
void Renderer::Pan(float deltaX, float deltaY) {
    const float scale = 0.0015f * (std::max)(cameraFocusDistance, 0.5f);
    const glm::vec3 up = glm::normalize(glm::cross(GetCameraRight(), GetCameraForward()));
    cameraPosition += (-GetCameraRight() * deltaX + up * deltaY) * scale;
}

// Rueda: zoom acercándose / alejándose en la dirección en la que se mira
void Renderer::OnMouseWheel(float wheelY) {
    const float step = (std::max)(cameraFocusDistance * 0.12f, 0.05f) * wheelY;
    const float newDist = cameraFocusDistance - step;
    if (newDist < 0.1f) {
        // Muy cerca del punto enfocado: se sigue avanzando sin cambiar la distancia de enfoque
        cameraPosition += GetCameraForward() * 0.05f * wheelY;
        return;
    }
    cameraPosition += GetCameraForward() * step;
    cameraFocusDistance = (std::min)(newDist, 1000.0f);
}

// Tecla F: centrar la cámara en el objeto seleccionado, a una distancia que lo deje entero en pantalla
void Renderer::FocusSelected() {
    if (!selectedObject) {
        LOG("F: no hay ningun objeto seleccionado para enfocar.");
        return;
    }
    glm::vec3 center, mn, mx;
    float radius = 0.5f;
    if (ComputeWorldAABB(selectedObject.get(), true, mn, mx)) {
        center = (mn + mx) * 0.5f;
        radius = (std::max)(glm::length(mx - mn) * 0.5f, 0.05f);
    } else {
        center = glm::vec3(WorldMatrix(selectedObject.get())[3]);   // GameObject vacío sin hijos
    }
    // Con un FOV de 45º, a radius / sin(22.5º) la esfera envolvente cabe en pantalla
    const float dist = radius / std::sin(glm::radians(22.5f)) * 1.1f;
    PlaceCameraAround(center, dist);
    LOG("Camara enfocada en: %s", selectedObject->name.c_str());
}

void Renderer::ResetCamera() {
    cameraYaw = 0.0f;
    cameraPitch = 20.0f;
    PlaceCameraAround(glm::vec3(0.0f), 3.5f);
}

// ─────────────────────────────────────────────
//  Render: escena 3D con OpenGL (la interfaz la dibuja el Editor encima)
// ─────────────────────────────────────────────

void Renderer::Render(const Window& window) {
    // Estado de OpenGL elegido en el panel Configuración
    if (depthTest) glEnable(GL_DEPTH_TEST); else glDisable(GL_DEPTH_TEST);
    if (cullFace) { glEnable(GL_CULL_FACE); glCullFace(GL_BACK); } else glDisable(GL_CULL_FACE);

    // Limpiar pantalla
    glClearColor(clearColor.r, clearColor.g, clearColor.b, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // Matriz de Proyección Perspectiva
    float aspect = (float)window.GetWidth() / (float)window.GetHeight();
    glm::mat4 projection = glm::perspective(glm::radians(45.0f), aspect, 0.1f, 5000.0f);
    glMatrixMode(GL_PROJECTION);
    glLoadMatrixf(glm::value_ptr(projection));

    // Matriz de Vista (Cámara Orbital)
    const glm::vec3 cameraPos = cameraPosition;
    glm::mat4 view = glm::lookAt(cameraPos, cameraPos + GetCameraForward(), glm::vec3(0.0f, 1.0f, 0.0f));
    lastView = view;
    lastProjection = projection;
    lastCameraPos = cameraPos;

    glMatrixMode(GL_MODELVIEW);
    glLoadMatrixf(glm::value_ptr(view));

    // Posición de luz fija en el mundo
    GLfloat lightPos[] = { lightDirection.x, lightDirection.y, lightDirection.z, 0.0f };
    glLightfv(GL_LIGHT0, GL_POSITION, lightPos);

    // Dibujar Rejilla 3D de suelo
    if (showScene && showGrid) {
        RenderGrid();
    }

    // Dibujar Objetos FBX / OBJ de la escena
    if (showScene && !objects.empty()) {
        RenderObjects(view);
    }

    // Resaltar la pieza seleccionada
    if (showScene && showSelectionHighlight && selectedObject) {
        RenderSelectionHighlight(view);
    }

    // Modo Debug: cajas envolventes (AABB / OBB) encima de la escena
    if (showScene && debugMode && !objects.empty()) {
        RenderDebug(view);
    }
}

// ─────────────────────────────────────────────
//  RenderGrid: rejilla de orientación 3D
// ─────────────────────────────────────────────

void Renderer::RenderGrid() {
    glDisable(GL_LIGHTING);
    glLineWidth(1.0f);
    glBegin(GL_LINES);

    const int gridSize = 10;
    const float step = 1.0f;

    for (int i = -gridSize; i <= gridSize; ++i) {
        if (i == 0) {
            // Eje Z central (azul)
            glColor3f(0.2f, 0.4f, 0.9f);
        } else {
            glColor3f(0.25f, 0.25f, 0.3f);
        }
        glVertex3f((float)i * step, 0.0f, -(float)gridSize * step);
        glVertex3f((float)i * step, 0.0f,  (float)gridSize * step);

        if (i == 0) {
            // Eje X central (rojo)
            glColor3f(0.9f, 0.25f, 0.25f);
        } else {
            glColor3f(0.25f, 0.25f, 0.3f);
        }
        glVertex3f(-(float)gridSize * step, 0.0f, (float)i * step);
        glVertex3f( (float)gridSize * step, 0.0f, (float)i * step);
    }

    glEnd();
}

// ─────────────────────────────────────────────
//  RenderObjects: dibuja los modelos de la escena
// ─────────────────────────────────────────────

void Renderer::RenderObjects(const glm::mat4& view) {
    const bool shaderActive = useShaders && shader.IsValid();
    if (shaderActive) {
        // Valores iguales para todo el frame
        shader.Use();
        shader.SetMat4("uView", view);
        shader.SetMat4("uProjection", lastProjection);
        shader.SetVec3("uCameraPos", lastCameraPos);
        shader.SetVec3("uLightDir", glm::normalize(lightDirection));
        shader.SetVec3("uLightColor", lightColor);
        shader.SetFloat("uAmbient", shaderAmbient);
        shader.SetFloat("uSpecular", shaderSpecular);
        shader.SetFloat("uShininess", shaderShininess);
        shader.SetInt("uMode", shaderMode);
        shader.SetFloat("uTime", GetTimeSeconds());
        shader.SetInt("uTexture", 0);   // la textura va en la unidad 0
    } else {
        if (fixedLighting) glEnable(GL_LIGHTING); else glDisable(GL_LIGHTING);
        glEnable(GL_NORMALIZE);
    }

    for (const auto& obj : objects) {
        RenderNode(obj, glm::mat4(1.0f), view, nullptr, true, false, false);
    }

    if (shaderActive) {
        Shader::Unuse();   // lo que viene después (selección, debug, ImGui) usa su propio modo
    }

    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    glDisable(GL_NORMALIZE);
    glDisable(GL_LIGHTING);
}

void Renderer::RenderNode(const std::shared_ptr<GameObject>& obj, const glm::mat4& parentWorld, const glm::mat4& view,
                          const Texture* inheritedTexture, bool inheritedShowTexture, bool inheritedWireframe,
                          bool inheritedSelected) {
    // Si un nodo está oculto, se ocultan también todos sus hijos
    if (!obj || !obj->visible) return;

    // Matriz local (Posición -> Rotación -> Escala -> transform del FBX), del componente Transform
    const glm::mat4 local = obj->transform->GetLocalMatrix();

    // Matriz mundo = mundo del padre * local (así los hijos siguen al padre)
    const glm::mat4 world = parentWorld * local;

    // Propiedades que se heredan del padre
    const bool isSelected = inheritedSelected || (obj == selectedObject);
    const Texture* nodeTexture = (obj->texture->overrideTexture && obj->texture->overrideTexture->IsLoaded())
                                 ? obj->texture->overrideTexture.get() : inheritedTexture;
    if (obj->texture->useCheckerTexture && checkerTexture && checkerTexture->IsLoaded()) {
        nodeTexture = checkerTexture.get();   // la textura de comprobación tiene prioridad
    }
    const bool showTex   = (inheritedShowTexture && obj->texture->showTexture) || obj->texture->useCheckerTexture;
    const bool wireframe = inheritedWireframe || obj->mesh->wireframe;

    if (obj->mesh->geometry && obj->mesh->geometry->HasMesh() && !obj->mesh->meshIndices.empty()) {
        // Modo Wireframe por objeto o global (vale para los dos caminos)
        glPolygonMode(GL_FRONT_AND_BACK, (wireframe || globalWireframe) ? GL_LINE : GL_FILL);

        if (useShaders && shader.IsValid()) {
            // Camino con SHADER: se pasan la matriz del objeto y su color como uniforms
            glm::vec3 col = obj->mesh->color;
            if (isSelected) col = glm::clamp(col * 1.15f, 0.0f, 1.0f);
            shader.SetMat4("uModel", world);
            shader.SetMat3("uNormalMatrix", glm::transpose(glm::inverse(glm::mat3(world))));
            shader.SetVec3("uColor", col);
            obj->mesh->geometry->DrawShader(obj->mesh->meshIndices, globalShowTextures && showTex, nodeTexture,
                                  shader.GetUniformLocation("uHasTexture"));
        } else {
            // Camino antiguo: pipeline fijo de OpenGL
            glPushMatrix();
            glLoadMatrixf(glm::value_ptr(view * world));

            // Configurar material
            const bool useTex = globalShowTextures && showTex;
            const bool texturedNow = useTex && obj->mesh->geometry->HasUVs() &&
                (obj->mesh->geometry->HasAnyTexture() || nodeTexture != nullptr);
            // Con textura usamos blanco para no oscurecerla (GL_MODULATE multiplica textura * color)
            glm::vec3 objCol = texturedNow ? glm::vec3(1.0f) : obj->mesh->color;
            if (isSelected) {
                // Ligeramente resaltado si está seleccionado
                objCol = glm::clamp(objCol * 1.15f, 0.0f, 1.0f);
            }

            GLfloat matDiffuse[]  = { objCol.r, objCol.g, objCol.b, 1.0f };
            GLfloat matAmbient[]  = { objCol.r * 0.35f, objCol.g * 0.35f, objCol.b * 0.35f, 1.0f };
            GLfloat matSpecular[] = { 0.4f, 0.4f, 0.4f, 1.0f };
            glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE,   matDiffuse);
            glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT,   matAmbient);
            glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR,  matSpecular);
            glMaterialf (GL_FRONT_AND_BACK, GL_SHININESS, 32.0f);

            // Modo Wireframe por objeto o global
            if (wireframe || globalWireframe) {
                glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
            } else {
                glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
            }

            obj->mesh->geometry->Draw(obj->mesh->meshIndices, useTex, nodeTexture);

            glPopMatrix();
        }
    }

    for (const auto& child : obj->children) {
        RenderNode(child, world, view, nodeTexture, showTex, wireframe, isSelected);
    }
}

// ─────────────────────────────────────────────
//  Modo Debug: AABB / OBB
// ─────────────────────────────────────────────

// Las 8 esquinas de una caja (min, max) transformadas por 'm'.
// Índice de esquina = bits (x, y, z): 0 = min, 1 = max
static void BoxCorners(const glm::vec3& mn, const glm::vec3& mx, const glm::mat4& m, glm::vec3 out[8]) {
    for (int i = 0; i < 8; ++i) {
        const glm::vec3 p((i & 1) ? mx.x : mn.x, (i & 2) ? mx.y : mn.y, (i & 4) ? mx.z : mn.z);
        out[i] = glm::vec3(m * glm::vec4(p, 1.0f));
    }
}

// Dibuja las 12 aristas de una caja a partir de sus 8 esquinas (en coordenadas de mundo)
static void DrawBoxEdges(const glm::vec3 c[8], const glm::vec3& color, float width) {
    static const int edges[12][2] = {
        {0, 1}, {2, 3}, {4, 5}, {6, 7},   // aristas en X
        {0, 2}, {1, 3}, {4, 6}, {5, 7},   // aristas en Y
        {0, 4}, {1, 5}, {2, 6}, {3, 7}    // aristas en Z
    };
    glLineWidth(width);
    glColor3f(color.r, color.g, color.b);
    glBegin(GL_LINES);
    for (const auto& e : edges) {
        glVertex3f(c[e[0]].x, c[e[0]].y, c[e[0]].z);
        glVertex3f(c[e[1]].x, c[e[1]].y, c[e[1]].z);
    }
    glEnd();
}

// Dibuja una caja alineada con los ejes del mundo
static void DrawAABB(const glm::vec3& mn, const glm::vec3& mx, const glm::vec3& color, float width) {
    glm::vec3 c[8];
    BoxCorners(mn, mx, glm::mat4(1.0f), c);
    DrawBoxEdges(c, color, width);
}

// Suma al AABB (mn, mx) el AABB mundo de la malla de 'o' y, si 'recursive', el de sus hijos
static void AccumulateWorldAABB(const GameObject* o, const glm::mat4& world, bool recursive,
                                glm::vec3& mn, glm::vec3& mx, bool& any) {
    glm::vec3 lmin, lmax;
    if (o->mesh->geometry && o->mesh->geometry->GetLocalAABB(o->mesh->meshIndices, lmin, lmax)) {
        glm::vec3 c[8];
        BoxCorners(lmin, lmax, world, c);
        for (const auto& p : c) { mn = glm::min(mn, p); mx = glm::max(mx, p); }
        any = true;
    }
    if (recursive) {
        for (const auto& child : o->children) {
            AccumulateWorldAABB(child.get(), world * LocalMatrix(*child), true, mn, mx, any);
        }
    }
}

bool Renderer::ComputeWorldAABB(const GameObject* obj, bool includeChildren, glm::vec3& outMin, glm::vec3& outMax) const {
    outMin = glm::vec3(FLT_MAX);
    outMax = glm::vec3(-FLT_MAX);
    if (!obj) return false;
    bool any = false;
    AccumulateWorldAABB(obj, WorldMatrix(obj), includeChildren, outMin, outMax, any);
    return any;
}

// ─────────────────────────────────────────────
//  Resaltado de la selección
// ─────────────────────────────────────────────

void Renderer::RenderSelectionHighlight(const glm::mat4& view) {
    const GameObject* sel = selectedObject.get();
    if (!sel) return;
    // Si el objeto o alguno de sus padres está oculto, no hay nada que resaltar
    for (const GameObject* p = sel; p; p = p->parent) if (!p->visible) return;

    const glm::mat4 world = WorldMatrix(sel);

    glPushAttrib(GL_ENABLE_BIT | GL_LINE_BIT | GL_POLYGON_BIT | GL_CURRENT_BIT |
                 GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);
    glDisable(GL_LIGHTING);
    glDisable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);   // el resaltado no tapa a nada

    // Pasada 0: partes tapadas por otros objetos, en alambre muy tenue (rayos X)
    glDisable(GL_DEPTH_TEST);
    RenderHighlightNode(sel, world, view, 0);

    // Pasada 1: relleno semitransparente encima de la pieza
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glEnable(GL_POLYGON_OFFSET_FILL);
    glPolygonOffset(-1.0f, -1.0f);
    RenderHighlightNode(sel, world, view, 1);
    glDisable(GL_POLYGON_OFFSET_FILL);

    // Pasada 2: aristas de la malla en alambre
    glEnable(GL_POLYGON_OFFSET_LINE);
    glPolygonOffset(-1.0f, -2.0f);
    RenderHighlightNode(sel, world, view, 2);

    glPopAttrib();
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
}

void Renderer::RenderHighlightNode(const GameObject* obj, const glm::mat4& world, const glm::mat4& view, int pass) {
    if (!obj || !obj->visible) return;

    if (obj->mesh->geometry && obj->mesh->geometry->HasMesh() && !obj->mesh->meshIndices.empty()) {
        glMatrixMode(GL_MODELVIEW);
        glPushMatrix();
        glLoadMatrixf(glm::value_ptr(view * world));

        const glm::vec3& c = selectionColor;
        if (pass == 0) {
            glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
            glLineWidth(1.0f);
            glColor4f(c.r, c.g, c.b, 0.15f);
        } else if (pass == 1) {
            // Opacidad del relleno: late entre 0.15 y 0.40 si el pulso está activado
            float alpha = 0.30f;
            if (selectionPulse) {
                const float t = GetTimeSeconds();
                alpha = 0.15f + 0.25f * (0.5f + 0.5f * std::sin(t * 4.0f));
            }
            glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
            glColor4f(c.r, c.g, c.b, alpha);
        } else {
            glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
            glLineWidth(1.5f);
            glColor4f(c.r, c.g, c.b, 0.9f);
        }

        obj->mesh->geometry->Draw(obj->mesh->meshIndices, false, nullptr);   // sin texturas: sólo el color
        glPopMatrix();
    }

    // Los hijos de la pieza seleccionada también se resaltan
    for (const auto& child : obj->children) {
        RenderHighlightNode(child.get(), world * LocalMatrix(*child), view, pass);
    }
}

// ─────────────────────────────────────────────
//  Selección con el ratón (raycast)
// ─────────────────────────────────────────────

// Rayo contra caja (método de los "slabs"). Devuelve la t de entrada (o 0 si el origen está dentro).
static bool RayAABB(const glm::vec3& o, const glm::vec3& d, const glm::vec3& mn, const glm::vec3& mx, float& tEnter) {
    float t0 = 0.0f, t1 = FLT_MAX;
    for (int i = 0; i < 3; ++i) {
        if (std::fabs(d[i]) < 1e-12f) {
            if (o[i] < mn[i] || o[i] > mx[i]) return false;   // paralelo y fuera
            continue;
        }
        float a = (mn[i] - o[i]) / d[i];
        float b = (mx[i] - o[i]) / d[i];
        if (a > b) std::swap(a, b);
        t0 = (std::max)(t0, a);
        t1 = (std::min)(t1, b);
        if (t0 > t1) return false;
    }
    tEnter = t0;
    return true;
}

void Renderer::PickAt(float x, float y, float windowWidth, float windowHeight, bool selectWholeModel) {
    if (windowWidth <= 0.0f || windowHeight <= 0.0f) return;

    // 1) Píxel -> coordenadas normalizadas (-1..1) -> rayo en el mundo
    const float ndcX = 2.0f * x / windowWidth - 1.0f;
    const float ndcY = 1.0f - 2.0f * y / windowHeight;
    const glm::mat4 invVP = glm::inverse(lastProjection * lastView);
    glm::vec4 nearP = invVP * glm::vec4(ndcX, ndcY, -1.0f, 1.0f);
    glm::vec4 farP  = invVP * glm::vec4(ndcX, ndcY,  1.0f, 1.0f);
    nearP /= nearP.w;
    farP  /= farP.w;
    const glm::vec3 rayOrigin(nearP);
    const glm::vec3 rayDir = glm::normalize(glm::vec3(farP) - glm::vec3(nearP));

    // 2) Recorrer la escena buscando el impacto más cercano.
    //    El rayo se pasa al espacio local de cada nodo SIN normalizar la dirección,
    //    así la t del impacto es la misma que en el mundo y se pueden comparar.
    std::shared_ptr<GameObject> best;
    float bestT = FLT_MAX;
    std::function<void(const std::shared_ptr<GameObject>&, const glm::mat4&)> visit =
        [&](const std::shared_ptr<GameObject>& obj, const glm::mat4& parentWorld) {
            if (!obj || !obj->visible) return;   // lo oculto no se puede seleccionar
            const glm::mat4 world = parentWorld * LocalMatrix(*obj);

            glm::vec3 lmin, lmax;
            if (obj->mesh->geometry && obj->mesh->geometry->GetLocalAABB(obj->mesh->meshIndices, lmin, lmax)) {
                const glm::mat4 invWorld = glm::inverse(world);
                const glm::vec3 lo = glm::vec3(invWorld * glm::vec4(rayOrigin, 1.0f));
                const glm::vec3 ld = glm::vec3(invWorld * glm::vec4(rayDir, 0.0f));

                // Primero la caja (rápido); sólo si la toca se comprueban los triángulos
                float tBox = 0.0f;
                if (RayAABB(lo, ld, lmin, lmax, tBox) && tBox < bestT) {
                    float tTri = 0.0f;
                    if (obj->mesh->geometry->RaycastLocal(obj->mesh->meshIndices, lo, ld, tTri) && tTri < bestT) {
                        bestT = tTri;
                        best = obj;
                    }
                }
            }
            for (const auto& child : obj->children) visit(child, world);
        };
    for (const auto& obj : objects) visit(obj, glm::mat4(1.0f));

    // 3) Seleccionar
    if (!best) {
        selectedObject.reset();
        return;
    }
    if (selectWholeModel) {
        // Subir hasta el objeto raíz (el FBX completo o el grupo de más arriba)
        while (best->parent) {
            auto parent = FindShared(best->parent);
            if (!parent) break;
            best = parent;
        }
    }
    selectedObject = best;
    LOG("Seleccionado con el raton: %s", best->name.c_str());
}

void Renderer::RenderDebug(const glm::mat4& view) {
    // Guardar el estado de OpenGL que vamos a tocar y restaurarlo al final
    glPushAttrib(GL_ENABLE_BIT | GL_LINE_BIT | GL_POLYGON_BIT | GL_CURRENT_BIT);
    glDisable(GL_LIGHTING);
    glDisable(GL_TEXTURE_2D);
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    if (debugThroughObjects) glDisable(GL_DEPTH_TEST);

    // Las esquinas se calculan ya en coordenadas de mundo: sólo hace falta la vista
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadMatrixf(glm::value_ptr(view));

    for (const auto& obj : objects) {
        glm::vec3 mn, mx;
        RenderDebugNode(obj, glm::mat4(1.0f), false, mn, mx);
    }

    glPopMatrix();
    glPopAttrib();
}

bool Renderer::RenderDebugNode(const std::shared_ptr<GameObject>& obj, const glm::mat4& parentWorld,
                               bool inheritedSelected, glm::vec3& outMin, glm::vec3& outMax) {
    outMin = glm::vec3(FLT_MAX);
    outMax = glm::vec3(-FLT_MAX);
    if (!obj || !obj->visible) return false;

    const glm::mat4 world = parentWorld * LocalMatrix(*obj);
    const bool isSelected = (obj == selectedObject);
    const bool inSelection = inheritedSelected || isSelected;
    const bool draw = !debugOnlySelected || inSelection;
    const float width = isSelected ? debugLineWidth * 2.0f : debugLineWidth;

    // 1) Caja de la malla propia del nodo
    bool any = false;
    glm::vec3 lmin, lmax;
    if (obj->mesh->geometry && obj->mesh->geometry->GetLocalAABB(obj->mesh->meshIndices, lmin, lmax)) {
        glm::vec3 corners[8];
        BoxCorners(lmin, lmax, world, corners);
        for (const auto& p : corners) { outMin = glm::min(outMin, p); outMax = glm::max(outMax, p); }
        any = true;

        if (draw && debugShowOBB) {
            // OBB: la caja local girada/escalada con el objeto
            DrawBoxEdges(corners, isSelected ? glm::vec3(1.0f, 0.95f, 0.4f) : glm::vec3(0.9f, 0.75f, 0.1f), width);
        }
        if (draw && debugShowAABB) {
            // AABB: la caja más pequeña alineada con los ejes del mundo que contiene la OBB
            DrawAABB(outMin, outMax, isSelected ? glm::vec3(0.5f, 1.0f, 0.5f) : glm::vec3(0.1f, 0.8f, 0.25f), width);
        }
    }

    // 1b) Normales de la malla (vértice y/o cara)
    if (draw && obj->mesh->geometry && (debugShowVertexNormals || debugShowFaceNormals)) {
        std::vector<glm::vec3> vLines, fLines;
        obj->mesh->geometry->BuildNormalLines(obj->mesh->meshIndices, world, debugNormalLength,
                                    debugShowVertexNormals ? &vLines : nullptr,
                                    debugShowFaceNormals ? &fLines : nullptr);
        auto drawLines = [](const std::vector<glm::vec3>& lines, const glm::vec3& col) {
            if (lines.empty()) return;
            glLineWidth(1.0f);
            glColor3f(col.r, col.g, col.b);
            glBegin(GL_LINES);
            for (const auto& p : lines) glVertex3f(p.x, p.y, p.z);
            glEnd();
        };
        drawLines(vLines, glm::vec3(0.2f, 0.9f, 0.95f));   // cian: normales de vértice
        drawLines(fLines, glm::vec3(1.0f, 0.3f, 0.8f));    // magenta: normales de cara
    }

    // 2) Hijos (y su caja conjunta)
    bool childAny = false;
    for (const auto& child : obj->children) {
        glm::vec3 cmin, cmax;
        if (RenderDebugNode(child, world, inSelection, cmin, cmax)) {
            outMin = glm::min(outMin, cmin);
            outMax = glm::max(outMax, cmax);
            childAny = true;
        }
    }
    any = any || childAny;

    // 3) Caja de grupo: envuelve al padre con todos sus hijos (útil para GameObjects vacíos)
    if (draw && debugShowGroupAABB && childAny) {
        // Un poco más grande para que no se solape con la caja del hijo cuando coinciden
        const glm::vec3 pad = (outMax - outMin) * 0.01f + glm::vec3(0.002f);
        DrawAABB(outMin - pad, outMax + pad,
                 isSelected ? glm::vec3(0.5f, 0.85f, 1.0f) : glm::vec3(0.2f, 0.55f, 0.95f), width);
    }
    return any;
}

// ─────────────────────────────────────────────
//  DrawHierarchyNode: un nodo del árbol de la jerarquía
// ─────────────────────────────────────────────

void Renderer::InitShaders() {
    // Buscar los ficheros junto al directorio de trabajo o junto al ejecutable
    std::vector<std::string> bases = { "" };
    if (const char* basePath = SDL_GetBasePath()) bases.push_back(basePath);

    bool loaded = false;
    for (const auto& base : bases) {
        const std::string vs = base + "assets/shaders/default.vert";
        const std::string fs = base + "assets/shaders/default.frag";
        std::string tmp;
        if (!Shader::ReadTextFile(vs, tmp)) continue;
        if (shader.LoadFromFiles(vs, fs)) {
            vertexShaderPath = vs;
            fragmentShaderPath = fs;
            loaded = true;
        }
        break;   // el fichero existe: si tiene errores se verán en el editor
    }

    if (!loaded && !shader.IsValid()) {
        LOG("Usando el shader por defecto incluido en el motor.");
        vertexShaderPath.clear();
        fragmentShaderPath.clear();
        shader.Compile(Shader::DefaultVertexSource(), Shader::DefaultFragmentSource());
    }

    if (!shader.IsValid()) {
        LOG("No se pudo crear ningun shader: se usara el pipeline fijo de OpenGL.");
        useShaders = false;
    }
}

void Renderer::AddPrimitive(PrimitiveType type) {
    auto geom = std::make_shared<Geometria>();
    if (!geom->CreatePrimitive(type)) {
        LOG("Error al crear la primitiva %s", Geometria::PrimitiveName(type));
        return;
    }
    auto obj = std::make_shared<GameObject>(MakeUniqueName(Geometria::PrimitiveName(type)));
    obj->filePath = geom->GetFilePath();
    obj->mesh->geometry = geom;
    obj->mesh->meshIndices = { 0 };
    // Como en Unity: aparece en el punto que está mirando la cámara
    obj->transform->position = cameraPosition + GetCameraForward() * cameraFocusDistance;

    objects.push_back(obj);
    selectedObject = obj;
}

void Renderer::CreateCheckerTexture() {
    const int size = 64, cell = 8;   // 64x64 píxeles, cuadros de 8x8
    std::vector<unsigned char> pixels(size * size * 4);
    for (int y = 0; y < size; ++y) {
        for (int x = 0; x < size; ++x) {
            const unsigned char c = (((x / cell) + (y / cell)) % 2 == 0) ? 255 : 0;
            unsigned char* p = &pixels[(y * size + x) * 4];
            p[0] = p[1] = p[2] = c;
            p[3] = 255;
        }
    }
    checkerTexture = std::make_shared<Texture>();
    if (!checkerTexture->LoadFromBGRA(pixels.data(), size, size, "Checker (cuadros blancos y negros)")) {
        LOG("Error al crear la textura de comprobacion.");
        checkerTexture.reset();
    }
}

// ─────────────────────────────────────────────
//  CleanUp: liberar recursos de GPU
// ─────────────────────────────────────────────

void Renderer::CleanUp() {
    shader.CleanUp();
    checkerTexture.reset();
    Geometria::DebugShutdown();

    objects.clear();
}
