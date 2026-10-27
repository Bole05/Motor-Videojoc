#include "Renderer.h"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/matrix_decompose.hpp>
#include <glm/gtx/euler_angles.hpp>
#include <functional>
#include <set>
#include <fmt/core.h>
#include <algorithm>
#include <cmath>
#include <cctype>
#include <cstring>
#include <fstream>
#include <cfloat>
#include "Log.h"

// ImGui :UI
#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_opengl3.h>

// ─────────────────────────────────────────────
//  Constructor / Destructor
// ─────────────────────────────────────────────

Renderer::Renderer() {}

Renderer::~Renderer() {
    CleanUp();
}

// ─────────────────────────────────────────────
//  Init: OpenGL + ImGui + Luz + Modelo inicial (BakerHouse)
// ─────────────────────────────────────────────

bool Renderer::Init(Window& window) {
    // 1. Configurar GL_LIGHT0 (luz blanca difusa y especular)
    GLfloat lightAmbient[]  = { 0.35f, 0.35f, 0.35f, 1.0f };
    GLfloat lightDiffuse[]  = { 0.9f,  0.9f,  0.9f,  1.0f };
    GLfloat lightSpecular[] = { 0.6f,  0.6f,  0.6f,  1.0f };
    glLightfv(GL_LIGHT0, GL_AMBIENT,  lightAmbient);
    glLightfv(GL_LIGHT0, GL_DIFFUSE,  lightDiffuse);
    glLightfv(GL_LIGHT0, GL_SPECULAR, lightSpecular);
    glEnable(GL_LIGHT0);
    glLightModeli(GL_LIGHT_MODEL_TWO_SIDE, GL_TRUE);

    // 2. Inicializar Dear ImGui
    //Compruba la verción de ImGui si se encaja 
    IMGUI_CHECKVERSION();
    //Crear contexto de la sistema ImGui, asi que empieza guardar o podemos decir entender las acción que hace en panel de UI
    ImGui::CreateContext();
    //Conecta la sistema imgui con la sistema operativa para que pueda empezar entender, y aqui es donde guarda posicion de raton, tamaño de la ventana Ui etc
    ImGuiIO& io = ImGui::GetIO();
    //Habilita la navegación por la teclado, asi que nos permite usar los comandos del teclado
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    // Estilo visual moderno oscuro
    ImGui::StyleColorsDark();//Fondo gris
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 6.0f;
    style.FrameRounding = 4.0f;
    style.GrabRounding = 4.0f;
    style.PopupRounding = 4.0f;
    style.ScrollbarRounding = 4.0f;

    if (!ImGui_ImplSDL3_InitForOpenGL(window.GetWindow(), window.GetContext())) {//conecta libreria imGui con la libreria SDL3
        LOG("Error al inicializar ImGui_ImplSDL3");
        return false;
    }
    if (!ImGui_ImplOpenGL3_Init("#version 130")) {//Inicializa el renderizador del imGui en OpenGl
        LOG("Error al inicializar ImGui_ImplOpenGL3");
        return false;
    }

    LOG("Renderer e ImGui inicializados correctamente.");

    // 2b. Shaders GLSL
    InitShaders();

    // 3. Cargar la casa (BakerHouse.fbx) automáticamente al arrancar el motor.
    //    Probamos primero la ruta relativa al directorio de trabajo y, si no existe,
    //    la carpeta assets que hay junto al ejecutable.
    bool houseLoaded = AddModel("assets/BakerHouse.fbx");
    if (!houseLoaded) {
        if (const char* basePath = SDL_GetBasePath()) {
            houseLoaded = AddModel(std::string(basePath) + "assets/BakerHouse.fbx");
        }
    }
    if (!houseLoaded) {
        LOG("No se ha podido cargar assets/BakerHouse.fbx al iniciar.");
    }

    return true;
}

// ─────────────────────────────────────────────
//  Update: animaciones de rotación
// ─────────────────────────────────────────────

void Renderer::Update(float deltaTime) {
    // Actualizar rotación continua de objetos configurados (también los hijos)
    std::function<void(const std::shared_ptr<ModelObject>&)> updateNode =
        [&](const std::shared_ptr<ModelObject>& obj) {
            if (!obj) return;
            if (obj->autoRotate) {
                obj->rotation.y += obj->autoRotateSpeed * deltaTime;
                if (obj->rotation.y >= 360.0f) obj->rotation.y -= 360.0f;
                if (obj->rotation.y < 0.0f)    obj->rotation.y += 360.0f;
            }
            for (auto& child : obj->children) updateNode(child);
        };
    for (auto& obj : objects) updateNode(obj);
}

// ─────────────────────────────────────────────
//  Gestión de Escena
// ─────────────────────────────────────────────

// Pasa la matriz local de un nodo del FBX a Posición / Rotación (grados, orden YXZ) / Escala,
// para que se pueda editar desde el inspector igual que cualquier objeto.
static void DecomposeToTransform(const glm::mat4& m, ModelObject& obj) {
    glm::vec3 scale, translation, skew;
    glm::vec4 perspective;
    glm::quat orientation;
    if (!glm::decompose(m, scale, orientation, translation, skew, perspective)) {
        obj.baseTransform = m; // No se pudo descomponer: se usa la matriz tal cual
        return;
    }
    float yaw = 0.0f, pitch = 0.0f, roll = 0.0f;
    glm::extractEulerAngleYXZ(glm::mat4_cast(orientation), yaw, pitch, roll);
    obj.position = translation;
    obj.rotation = glm::degrees(glm::vec3(pitch, yaw, roll)); // (X, Y, Z)
    obj.scale    = scale;
}

// Crea un ModelObject para un nodo del FBX y, recursivamente, para todos sus hijos
static std::shared_ptr<ModelObject> CreateNodeObject(const ModelNode& node, const std::shared_ptr<Geometria>& geom,
                                                     const std::string& path, ModelObject* parent) {
    auto obj = std::make_shared<ModelObject>();
    obj->name = node.name;
    obj->filePath = path;
    obj->mesh = geom;
    obj->meshIndices = node.meshes;
    obj->parent = parent;
    DecomposeToTransform(node.transform, *obj);

    for (const auto& childNode : node.children) {
        obj->children.push_back(CreateNodeObject(childNode, geom, path, obj.get()));
    }
    return obj;
}

// Copia profunda de un objeto con todos sus hijos (para Duplicar)
static std::shared_ptr<ModelObject> CloneObject(const ModelObject& src, ModelObject* parent) {
    auto copy = std::make_shared<ModelObject>(src);
    copy->parent = parent;
    copy->children.clear();
    for (const auto& child : src.children) {
        copy->children.push_back(CloneObject(*child, copy.get()));
    }
    return copy;
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
    auto obj = std::make_shared<ModelObject>();
    obj->name = fileName;
    obj->filePath = path;
    obj->mesh = geom;
    obj->meshIndices = rootNode.meshes;
    obj->baseTransform = geom->GetNormalizeMatrix() * rootNode.transform;
    for (const auto& childNode : rootNode.children) {
        obj->children.push_back(CreateNodeObject(childNode, geom, path, obj.get()));
    }

    // Si ya hay otros objetos en la escena, colocarlo ligeramente al lado
    if (!objects.empty()) {
        obj->position.x = (float)objects.size() * 1.5f;
    }

    objects.push_back(obj);
    selectedObject = obj;

    LOG("Objeto '%s' anadido a la escena con %zu nodos hijos. Total objetos: %zu",
        obj->name.c_str(), obj->children.size(), objects.size());
    return true;
}

static std::string ToLowerExt(const std::string& path) {
    size_t dot = path.find_last_of('.');
    if (dot == std::string::npos) return "";
    std::string ext = path.substr(dot + 1);
    for (auto& ch : ext) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    return ext;
}

const Renderer::TextureDropZone* Renderer::FindDropZone(const glm::vec2& p) const {
    for (const auto& zone : textureDropZones) {
        if (p.x >= zone.min.x && p.x <= zone.max.x && p.y >= zone.min.y && p.y <= zone.max.y) {
            return &zone;
        }
    }
    return nullptr;
}

void Renderer::HandleDroppedFile(const std::string& path, float x, float y) {
    const std::string ext = ToLowerExt(path);
    const bool isImage = (ext == "png" || ext == "jpg" || ext == "jpeg" || ext == "dds" ||
                          ext == "tga" || ext == "bmp" || ext == "tif" || ext == "tiff");
    if (!isImage) {
        isDraggingFile = false;
        AddModel(path);
        return;
    }

    // ¿Se ha soltado encima de la miniatura de una textura del Inspector?
    const TextureDropZone* zone = FindDropZone(glm::vec2(x, y));
    if (!zone && isDraggingFile) zone = FindDropZone(dragPos); // última posición conocida del arrastre
    isDraggingFile = false;

    if (zone && zone->geom) {
        auto tex = std::make_shared<Texture>();
        if (!tex->Load(path)) {
            LOG("No se pudo cargar la textura: %s", path.c_str());
            return;
        }
        zone->geom->ReplaceMaterialTexture(zone->material, tex);
        return;
    }

    // Si no, se asigna como textura manual al objeto seleccionado (comportamiento de antes)
    ApplyTextureToSelected(path);
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
    sel->overrideTexture = tex;
    sel->showTexture = true;
    LOG("Textura '%s' asignada a '%s'", path.c_str(), sel->name.c_str());
    return true;
}

void Renderer::DuplicateSelected() {
    if (!selectedObject) return;

    ModelObject* parent = selectedObject->parent;
    auto copy = CloneObject(*selectedObject, parent);
    copy->name = selectedObject->name + " (Copia)";

    if (parent) {
        // Nodo hijo: la copia queda como hermano, dentro del mismo padre
        parent->children.push_back(copy);
    } else {
        copy->position.x += 1.2f; // Desplazamiento visible para distinguirlo
        objects.push_back(copy);
    }
    selectedObject = copy;
    LOG("Objeto duplicado como: %s", copy->name.c_str());
}

void Renderer::DeleteSelected() {
    if (!selectedObject) return;

    LOG("Eliminando objeto: %s", selectedObject->name.c_str());
    ModelObject* parent = selectedObject->parent;
    auto& list = parent ? parent->children : objects;
    auto it = std::find(list.begin(), list.end(), selectedObject);
    if (it != list.end()) {
        list.erase(it);
    }

    // Tras borrar un hijo seleccionamos su padre; tras borrar un modelo, el último de la escena
    selectedObject.reset();
    if (parent) {
        for (auto& root : objects) {
            std::function<bool(const std::shared_ptr<ModelObject>&)> findParent =
                [&](const std::shared_ptr<ModelObject>& o) {
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

// Matriz local de un nodo: Posición -> Rotación (YXZ) -> Escala -> transform propio del FBX
// (la misma que usa RenderNode)
static glm::mat4 LocalMatrix(const ModelObject& obj) {
    glm::mat4 local = glm::mat4(1.0f);
    local = glm::translate(local, obj.position);
    local = glm::rotate(local, glm::radians(obj.rotation.y), glm::vec3(0, 1, 0));
    local = glm::rotate(local, glm::radians(obj.rotation.x), glm::vec3(1, 0, 0));
    local = glm::rotate(local, glm::radians(obj.rotation.z), glm::vec3(0, 0, 1));
    local = glm::scale(local, obj.scale);
    return local * obj.baseTransform;
}

// Matriz mundo de un nodo (producto de las matrices locales desde la raíz)
static glm::mat4 WorldMatrix(const ModelObject* obj) {
    glm::mat4 world = glm::mat4(1.0f);
    for (const ModelObject* o = obj; o; o = o->parent) {
        world = LocalMatrix(*o) * world;
    }
    return world;
}

// true si 'node' es 'ancestor' o está dentro de él (hijo, nieto...)
static bool IsSelfOrDescendant(const ModelObject* node, const ModelObject* ancestor) {
    for (const ModelObject* p = node; p; p = p->parent) {
        if (p == ancestor) return true;
    }
    return false;
}

std::shared_ptr<ModelObject> Renderer::FindShared(const ModelObject* raw) const {
    if (!raw) return nullptr;
    std::function<std::shared_ptr<ModelObject>(const std::vector<std::shared_ptr<ModelObject>>&)> search =
        [&](const std::vector<std::shared_ptr<ModelObject>>& list) -> std::shared_ptr<ModelObject> {
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
    std::function<void(const std::vector<std::shared_ptr<ModelObject>>&)> collect =
        [&](const std::vector<std::shared_ptr<ModelObject>>& list) {
            for (const auto& o : list) { used.insert(o->name); collect(o->children); }
        };
    collect(objects);

    if (!used.count(base)) return base;
    for (int i = 1; ; ++i) {
        std::string candidate = base + " (" + std::to_string(i) + ")";
        if (!used.count(candidate)) return candidate;
    }
}

std::shared_ptr<ModelObject> Renderer::CreateEmptyObject(ModelObject* parent) {
    auto obj = std::make_shared<ModelObject>();
    obj->name = MakeUniqueName("GameObject");
    obj->parent = parent;
    // Sin malla: no se dibuja nada, sólo sirve de contenedor para agrupar otros objetos
    SiblingList(parent).push_back(obj);
    selectedObject = obj;
    LOG("GameObject vacio creado: %s%s%s", obj->name.c_str(),
        parent ? " (hijo de " : "", parent ? (parent->name + ")").c_str() : "");
    return obj;
}

bool Renderer::ReparentObject(ModelObject* obj, ModelObject* newParent, int index) {
    if (!obj) return false;
    // No se puede meter un objeto dentro de sí mismo ni de uno de sus hijos (crearía un ciclo)
    if (newParent && IsSelfOrDescendant(newParent, obj)) return false;

    std::shared_ptr<ModelObject> shared = FindShared(obj);
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
        const glm::mat4 trs = glm::inverse(parentWorld) * world * glm::inverse(obj->baseTransform);

        glm::vec3 scale, translation, skew;
        glm::vec4 perspective;
        glm::quat orientation;
        if (glm::decompose(trs, scale, orientation, translation, skew, perspective)) {
            float yaw = 0.0f, pitch = 0.0f, roll = 0.0f;
            glm::extractEulerAngleYXZ(glm::mat4_cast(orientation), yaw, pitch, roll);
            obj->position = translation;
            obj->rotation = glm::degrees(glm::vec3(pitch, yaw, roll));
            obj->scale    = scale;
        }
    }

    selectedObject = shared;
    LOG("'%s' movido a %s", obj->name.c_str(), newParent ? newParent->name.c_str() : "la raiz de la escena");
    return true;
}

// ─────────────────────────────────────────────
//  Cámara Orbital
// ─────────────────────────────────────────────

void Renderer::OnMouseMoved(float deltaX, float deltaY) {
    cameraYaw += deltaX * 0.35f;
    cameraPitch -= deltaY * 0.35f;

    // Limitar pitch para no invertir la cámara
    if (cameraPitch > 89.0f)  cameraPitch = 89.0f;
    if (cameraPitch < -89.0f) cameraPitch = -89.0f;
}

void Renderer::OnMouseWheel(float wheelY) {
    cameraDistance -= wheelY * 0.4f;
    if (cameraDistance < 0.5f)  cameraDistance = 0.5f;
    if (cameraDistance > 100.0f) cameraDistance = 100.0f;
}

void Renderer::ResetCamera() {
    cameraDistance = 3.5f;
    cameraYaw = 0.0f;
    cameraPitch = 20.0f;
    cameraTarget = glm::vec3(0.0f, 0.0f, 0.0f);
}

// ─────────────────────────────────────────────
//  Render: OpenGL Scene + ImGui
// ─────────────────────────────────────────────

void Renderer::Render(Window& window) {
    // Iniciar nuevo frame de ImGui
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();

    // Dibujar todos los paneles de la UI
    DrawUI(window);

    // Limpiar pantalla
    glClearColor(clearColor.r, clearColor.g, clearColor.b, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // Matriz de Proyección Perspectiva
    float aspect = (float)window.GetWidth() / (float)window.GetHeight();
    glm::mat4 projection = glm::perspective(glm::radians(45.0f), aspect, 0.1f, 5000.0f);
    glMatrixMode(GL_PROJECTION);
    glLoadMatrixf(glm::value_ptr(projection));

    // Matriz de Vista (Cámara Orbital)
    float radYaw = glm::radians(cameraYaw);
    float radPitch = glm::radians(cameraPitch);
    glm::vec3 cameraOffset;
    cameraOffset.x = cameraDistance * std::cos(radPitch) * std::sin(radYaw);
    cameraOffset.y = cameraDistance * std::sin(radPitch);
    cameraOffset.z = cameraDistance * std::cos(radPitch) * std::cos(radYaw);
    glm::vec3 cameraPos = cameraTarget + cameraOffset;
    glm::mat4 view = glm::lookAt(cameraPos, cameraTarget, glm::vec3(0.0f, 1.0f, 0.0f));
    lastView = view;
    lastProjection = projection;
    lastCameraPos = cameraPos;

    glMatrixMode(GL_MODELVIEW);
    glLoadMatrixf(glm::value_ptr(view));

    // Posición de luz fija en el mundo
    GLfloat lightPos[] = { lightDirection.x, lightDirection.y, lightDirection.z, 0.0f };
    glLightfv(GL_LIGHT0, GL_POSITION, lightPos);

    // Dibujar Rejilla 3D de suelo
    if (showGrid) {
        RenderGrid();
    }

    // Dibujar Objetos FBX / OBJ de la escena
    if (!objects.empty()) {
        RenderObjects(view);
    }

    // Resaltar la pieza seleccionada
    if (showSelectionHighlight && selectedObject) {
        RenderSelectionHighlight(view);
    }

    // Modo Debug: cajas envolventes (AABB / OBB) encima de la escena
    if (debugMode && !objects.empty()) {
        RenderDebug(view);
    }

    // Renderizar Dear ImGui encima de OpenGL
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    // Intercambiar buffers
    window.SwapBuffers();
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
        shader.SetFloat("uTime", (float)ImGui::GetTime());
        shader.SetInt("uTexture", 0);   // la textura va en la unidad 0
    } else {
        glEnable(GL_LIGHTING);
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

void Renderer::RenderNode(const std::shared_ptr<ModelObject>& obj, const glm::mat4& parentWorld, const glm::mat4& view,
                          const Texture* inheritedTexture, bool inheritedShowTexture, bool inheritedWireframe,
                          bool inheritedSelected) {
    // Si un nodo está oculto, se ocultan también todos sus hijos
    if (!obj || !obj->visible) return;

    // Matriz local: Posición -> Rotación (YXZ) -> Escala -> transform propio del FBX
    glm::mat4 local = glm::mat4(1.0f);
    local = glm::translate(local, obj->position);
    local = glm::rotate(local, glm::radians(obj->rotation.y), glm::vec3(0, 1, 0));
    local = glm::rotate(local, glm::radians(obj->rotation.x), glm::vec3(1, 0, 0));
    local = glm::rotate(local, glm::radians(obj->rotation.z), glm::vec3(0, 0, 1));
    local = glm::scale(local, obj->scale);
    local = local * obj->baseTransform;

    // Matriz mundo = mundo del padre * local (así los hijos siguen al padre)
    const glm::mat4 world = parentWorld * local;

    // Propiedades que se heredan del padre
    const bool isSelected = inheritedSelected || (obj == selectedObject);
    const Texture* nodeTexture = (obj->overrideTexture && obj->overrideTexture->IsLoaded())
                                 ? obj->overrideTexture.get() : inheritedTexture;
    const bool showTex   = inheritedShowTexture && obj->showTexture;
    const bool wireframe = inheritedWireframe || obj->wireframe;

    if (obj->mesh && obj->mesh->HasMesh() && !obj->meshIndices.empty()) {
        // Modo Wireframe por objeto o global (vale para los dos caminos)
        glPolygonMode(GL_FRONT_AND_BACK, (wireframe || globalWireframe) ? GL_LINE : GL_FILL);

        if (useShaders && shader.IsValid()) {
            // Camino con SHADER: se pasan la matriz del objeto y su color como uniforms
            glm::vec3 col = obj->color;
            if (isSelected) col = glm::clamp(col * 1.15f, 0.0f, 1.0f);
            shader.SetMat4("uModel", world);
            shader.SetMat3("uNormalMatrix", glm::transpose(glm::inverse(glm::mat3(world))));
            shader.SetVec3("uColor", col);
            obj->mesh->DrawShader(obj->meshIndices, globalShowTextures && showTex, nodeTexture,
                                  shader.GetUniformLocation("uHasTexture"));
        } else {
            // Camino antiguo: pipeline fijo de OpenGL
            glPushMatrix();
            glLoadMatrixf(glm::value_ptr(view * world));

            // Configurar material
            const bool useTex = globalShowTextures && showTex;
            const bool texturedNow = useTex && obj->mesh->HasUVs() &&
                (obj->mesh->HasAnyTexture() || nodeTexture != nullptr);
            // Con textura usamos blanco para no oscurecerla (GL_MODULATE multiplica textura * color)
            glm::vec3 objCol = texturedNow ? glm::vec3(1.0f) : obj->color;
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

            obj->mesh->Draw(obj->meshIndices, useTex, nodeTexture);

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
static void AccumulateWorldAABB(const ModelObject* o, const glm::mat4& world, bool recursive,
                                glm::vec3& mn, glm::vec3& mx, bool& any) {
    glm::vec3 lmin, lmax;
    if (o->mesh && o->mesh->GetLocalAABB(o->meshIndices, lmin, lmax)) {
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

bool Renderer::ComputeWorldAABB(const ModelObject* obj, bool includeChildren, glm::vec3& outMin, glm::vec3& outMax) const {
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
    const ModelObject* sel = selectedObject.get();
    if (!sel) return;
    // Si el objeto o alguno de sus padres está oculto, no hay nada que resaltar
    for (const ModelObject* p = sel; p; p = p->parent) if (!p->visible) return;

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

void Renderer::RenderHighlightNode(const ModelObject* obj, const glm::mat4& world, const glm::mat4& view, int pass) {
    if (!obj || !obj->visible) return;

    if (obj->mesh && obj->mesh->HasMesh() && !obj->meshIndices.empty()) {
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
                const float t = (float)ImGui::GetTime();
                alpha = 0.15f + 0.25f * (0.5f + 0.5f * std::sin(t * 4.0f));
            }
            glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
            glColor4f(c.r, c.g, c.b, alpha);
        } else {
            glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
            glLineWidth(1.5f);
            glColor4f(c.r, c.g, c.b, 0.9f);
        }

        obj->mesh->Draw(obj->meshIndices, false, nullptr);   // sin texturas: sólo el color
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
    std::shared_ptr<ModelObject> best;
    float bestT = FLT_MAX;
    std::function<void(const std::shared_ptr<ModelObject>&, const glm::mat4&)> visit =
        [&](const std::shared_ptr<ModelObject>& obj, const glm::mat4& parentWorld) {
            if (!obj || !obj->visible) return;   // lo oculto no se puede seleccionar
            const glm::mat4 world = parentWorld * LocalMatrix(*obj);

            glm::vec3 lmin, lmax;
            if (obj->mesh && obj->mesh->GetLocalAABB(obj->meshIndices, lmin, lmax)) {
                const glm::mat4 invWorld = glm::inverse(world);
                const glm::vec3 lo = glm::vec3(invWorld * glm::vec4(rayOrigin, 1.0f));
                const glm::vec3 ld = glm::vec3(invWorld * glm::vec4(rayDir, 0.0f));

                // Primero la caja (rápido); sólo si la toca se comprueban los triángulos
                float tBox = 0.0f;
                if (RayAABB(lo, ld, lmin, lmax, tBox) && tBox < bestT) {
                    float tTri = 0.0f;
                    if (obj->mesh->RaycastLocal(obj->meshIndices, lo, ld, tTri) && tTri < bestT) {
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
    revealSelectionInHierarchy = true;
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

bool Renderer::RenderDebugNode(const std::shared_ptr<ModelObject>& obj, const glm::mat4& parentWorld,
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
    if (obj->mesh && obj->mesh->GetLocalAABB(obj->meshIndices, lmin, lmax)) {
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
    if (draw && obj->mesh && (debugShowVertexNormals || debugShowFaceNormals)) {
        std::vector<glm::vec3> vLines, fLines;
        obj->mesh->BuildNormalLines(obj->meshIndices, world, debugNormalLength,
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

void Renderer::DrawHierarchyNode(const std::shared_ptr<ModelObject>& obj) {
    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick |
                               ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_DefaultOpen;
    if (obj->children.empty()) flags |= ImGuiTreeNodeFlags_Leaf;
    if (obj == selectedObject) flags |= ImGuiTreeNodeFlags_Selected;

    // Si la selección viene de la vista 3D, abrir los padres para que se vea en el árbol
    if (revealSelectionInHierarchy && selectedObject && obj != selectedObject &&
        IsSelfOrDescendant(selectedObject.get(), obj.get())) {
        ImGui::SetNextItemOpen(true);
    }

    const char* visTag = obj->visible ? "[v] " : "[x] ";
    const char* typeTag = obj->mesh ? "" : "(o) "; // (o) = GameObject vacío
    const bool open = ImGui::TreeNodeEx((void*)obj.get(), flags, "%s%s%s", visTag, typeTag, obj->name.c_str());
    const ImVec2 rowMin = ImGui::GetItemRectMin();
    const ImVec2 rowMax = ImGui::GetItemRectMax();
    if (revealSelectionInHierarchy && obj == selectedObject) {
        ImGui::SetScrollHereY(0.5f);   // desplazar la lista hasta el objeto seleccionado
    }

    // Clic sobre el nombre (no sobre la flecha) = seleccionar
    if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen()) {
        selectedObject = obj;
    }

    // --- Arrastrar este objeto ---
    if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None)) {
        ModelObject* raw = obj.get();
        ImGui::SetDragDropPayload("HIERARCHY_NODE", &raw, sizeof(ModelObject*));
        ImGui::Text("Mover: %s", obj->name.c_str());
        ImGui::EndDragDropSource();
    }

    // --- Soltar otro objeto encima de este ---
    //   franja superior (25%) = colocar ANTES (como hermano)
    //   centro                = meter DENTRO (como hijo)
    //   franja inferior (25%) = colocar DESPUÉS (como hermano)
    if (ImGui::BeginDragDropTarget()) {
        const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("HIERARCHY_NODE",
            ImGuiDragDropFlags_AcceptBeforeDelivery | ImGuiDragDropFlags_AcceptNoDrawDefaultRect);
        if (payload) {
            ModelObject* dragged = *(ModelObject* const*)payload->Data;
            const float h = rowMax.y - rowMin.y;
            const float my = ImGui::GetMousePos().y;
            enum { BEFORE, INSIDE, AFTER } zone = INSIDE;
            if (my < rowMin.y + h * 0.25f)      zone = BEFORE;
            else if (my > rowMax.y - h * 0.25f) zone = AFTER;

            ModelObject* newParent = (zone == INSIDE) ? obj.get() : obj->parent;
            const bool valid = dragged && dragged != obj.get() &&
                               !(newParent && IsSelfOrDescendant(newParent, dragged));

            // Indicador visual de dónde va a caer
            ImDrawList* dl = ImGui::GetWindowDrawList();
            const ImU32 col = valid ? IM_COL32(90, 170, 255, 255) : IM_COL32(230, 80, 80, 255);
            if (zone == INSIDE) dl->AddRect(rowMin, rowMax, col, 3.0f, 0, 2.0f);
            else {
                const float y = (zone == BEFORE) ? rowMin.y : rowMax.y;
                dl->AddLine(ImVec2(rowMin.x, y), ImVec2(rowMax.x, y), col, 2.0f);
            }

            if (payload->IsDelivery() && valid) {
                int index = -1;
                if (zone != INSIDE) {
                    auto& siblings = SiblingList(obj->parent);
                    auto it = std::find(siblings.begin(), siblings.end(), obj);
                    index = (int)std::distance(siblings.begin(), it) + (zone == AFTER ? 1 : 0);
                }
                pendingReparent = { dragged, newParent, index, true };
            }
        }
        ImGui::EndDragDropTarget();
    }

    // Menú contextual con clic derecho sobre el objeto
    if (ImGui::BeginPopupContextItem()) {
        selectedObject = obj;
        if (ImGui::MenuItem("Crear GameObject Vacio (hijo)")) {
            pendingAction = PendingAction::CREATE_EMPTY_CHILD;
        }
        if (obj->parent && ImGui::MenuItem("Sacar a la raiz")) {
            pendingReparent = { obj.get(), nullptr, -1, true };
        }
        ImGui::Separator();
        if (ImGui::MenuItem(obj->visible ? "Ocultar" : "Mostrar")) {
            obj->visible = !obj->visible;
        }
        if (ImGui::MenuItem("Duplicar")) {
            pendingAction = PendingAction::DUPLICATE;
        }
        if (ImGui::MenuItem("Eliminar")) {
            pendingAction = PendingAction::DELETE_OBJ;
        }
        ImGui::EndPopup();
    }

    if (open) {
        for (const auto& child : obj->children) {
            DrawHierarchyNode(child);
        }
        ImGui::TreePop();
    }
}

// ─────────────────────────────────────────────
//  Shaders GLSL: carga y editor en directo
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
    shaderStatusOk = shader.IsValid() && shader.GetLastError().empty();
    shaderStatus = shaderStatusOk ? "Shader compilado correctamente." : shader.GetLastError();
    CopyShaderSourcesToEditor();
}

void Renderer::CopyShaderSourcesToEditor() {
    auto fill = [](std::vector<char>& buf, const std::string& src) {
        buf.assign(src.begin(), src.end());
        buf.resize((std::max)(src.size() + 8192, (size_t)32768), '\0');   // margen para escribir
    };
    fill(vertexEditBuf, shader.GetVertexSource());
    fill(fragmentEditBuf, shader.GetFragmentSource());
}

void Renderer::CompileShaderFromEditor() {
    const std::string vs(vertexEditBuf.data());
    const std::string fs(fragmentEditBuf.data());
    if (shader.Compile(vs, fs)) {
        shaderStatusOk = true;
        shaderStatus = "Compilado correctamente.";
    } else {
        shaderStatusOk = false;
        shaderStatus = shader.GetLastError() + "\n(Se sigue usando la ultima version que funcionaba.)";
    }
}

void Renderer::DrawShaderEditor() {
    if (!showShaderEditor) return;

    ImGui::SetNextWindowSize(ImVec2(620.0f, 560.0f), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Editor de Shaders (GLSL)", &showShaderEditor)) {
        ImGui::End();
        return;
    }

    // --- Ajustes rápidos ---
    ImGui::Checkbox("Usar Shaders", &useShaders);
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Desactivado = pipeline fijo de OpenGL (como antes), para comparar.");
    ImGui::SameLine();
    const char* modes[] = { "Blinn-Phong (luz por pixel)", "Normales como color", "Toon (dibujo animado)",
                            "Sin iluminacion", "Coordenadas UV como color" };
    ImGui::SetNextItemWidth(230.0f);
    ImGui::Combo("Modo", &shaderMode, modes, IM_ARRAYSIZE(modes));

    if (!useShaders) ImGui::BeginDisabled();
    ImGui::SliderFloat("Ambiente", &shaderAmbient, 0.0f, 1.0f, "%.2f");
    ImGui::SliderFloat("Especular", &shaderSpecular, 0.0f, 1.0f, "%.2f");
    ImGui::SliderFloat("Brillo (shininess)", &shaderShininess, 2.0f, 256.0f, "%.0f", ImGuiSliderFlags_Logarithmic);
    ImGui::DragFloat3("Direccion luz", glm::value_ptr(lightDirection), 0.1f);
    ImGui::ColorEdit3("Color luz", glm::value_ptr(lightColor), ImGuiColorEditFlags_NoInputs);
    if (!useShaders) ImGui::EndDisabled();

    ImGui::Separator();

    // --- Código ---
    ImGui::TextWrapped("Edita el codigo GLSL y pulsa Compilar. Si hay errores, salen abajo en rojo "
                       "y se sigue usando la ultima version que funcionaba.");
    if (ImGui::BeginTabBar("ShaderTabs")) {
        const ImVec2 editorSize(-1.0f, ImGui::GetContentRegionAvail().y - 110.0f);
        if (ImGui::BeginTabItem("Vertex shader")) {
            ImGui::InputTextMultiline("##vs", vertexEditBuf.data(), vertexEditBuf.size(),
                                      editorSize, ImGuiInputTextFlags_AllowTabInput);
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Fragment shader")) {
            ImGui::InputTextMultiline("##fs", fragmentEditBuf.data(), fragmentEditBuf.size(),
                                      editorSize, ImGuiInputTextFlags_AllowTabInput);
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }

    if (ImGui::Button("Compilar", ImVec2(100, 26))) {
        CompileShaderFromEditor();
    }
    ImGui::SameLine();
    const bool hasFiles = !vertexShaderPath.empty();
    if (!hasFiles) ImGui::BeginDisabled();
    if (ImGui::Button("Guardar en ficheros", ImVec2(150, 26))) {
        std::ofstream v(vertexShaderPath, std::ios::binary), f(fragmentShaderPath, std::ios::binary);
        v << vertexEditBuf.data();
        f << fragmentEditBuf.data();
        shaderStatusOk = (bool)v && (bool)f;
        shaderStatus = shaderStatusOk ? "Guardado en " + vertexShaderPath + " y .frag" : "Error al guardar los ficheros.";
        LOG("%s", shaderStatus.c_str());
    }
    ImGui::SameLine();
    if (ImGui::Button("Recargar ficheros", ImVec2(140, 26))) {
        if (shader.LoadFromFiles(vertexShaderPath, fragmentShaderPath)) {
            shaderStatusOk = true;
            shaderStatus = "Recargado desde los ficheros.";
        } else {
            shaderStatusOk = false;
            shaderStatus = shader.GetLastError();
        }
        CopyShaderSourcesToEditor();
    }
    if (!hasFiles) ImGui::EndDisabled();
    ImGui::SameLine();
    if (ImGui::Button("Restaurar por defecto", ImVec2(160, 26))) {
        shader.Compile(Shader::DefaultVertexSource(), Shader::DefaultFragmentSource());
        shaderStatusOk = shader.GetLastError().empty();
        shaderStatus = shaderStatusOk ? "Restaurado el shader por defecto." : shader.GetLastError();
        CopyShaderSourcesToEditor();
    }
    if (hasFiles) {
        ImGui::TextDisabled("Ficheros: %s", vertexShaderPath.c_str());
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Ojo: al recompilar el proyecto, CMake vuelve a copiar la carpeta assets\n"
                              "del proyecto encima de esta. Copia tus cambios a assets/shaders del proyecto.");
    }

    // Estado / errores de compilación
    ImGui::PushStyleColor(ImGuiCol_Text, shaderStatusOk ? ImVec4(0.4f, 1.0f, 0.5f, 1.0f)
                                                        : ImVec4(1.0f, 0.4f, 0.4f, 1.0f));
    ImGui::TextWrapped("%s", shaderStatus.c_str());
    ImGui::PopStyleColor();

    ImGui::End();
}

// ─────────────────────────────────────────────
//  DrawUI: Interfaz estilo aplicación 3D / modelado
// ─────────────────────────────────────────────

void Renderer::DrawUI(Window& window) {
    // Las zonas para soltar texturas se vuelven a calcular en este frame
    textureDropZones.clear();

    // F1 = activar / desactivar el Modo Debug (si no se está escribiendo en un campo de texto)
    if (!ImGui::GetIO().WantTextInput && ImGui::IsKeyPressed(ImGuiKey_F1, false)) {
        debugMode = !debugMode;
        LOG("Modo Debug %s", debugMode ? "activado" : "desactivado");
    }

    // 1. Barra de Menú Principal Superior
    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("Archivo")) {
            if (ImGui::MenuItem("Cargar Modelo (FBX/OBJ)...")) {
                showLoadModal = true;
            }
            if (ImGui::MenuItem("Limpiar Escena")) {
                ClearScene();
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Salir")) {
                SDL_Event quitEvent;
                quitEvent.type = SDL_EVENT_QUIT;
                SDL_PushEvent(&quitEvent);
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Objeto")) {
            bool hasSelection = (selectedObject != nullptr);
            if (ImGui::MenuItem("Crear GameObject Vacio")) {
                CreateEmptyObject(nullptr);
            }
            if (ImGui::MenuItem("Crear GameObject Vacio Hijo", nullptr, false, hasSelection)) {
                CreateEmptyObject(selectedObject.get());
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Duplicar", "Ctrl+D", false, hasSelection)) {
                DuplicateSelected();
            }
            if (ImGui::MenuItem("Eliminar", "Supr", false, hasSelection)) {
                DeleteSelected();
            }
            if (ImGui::MenuItem("Resetear Transform", nullptr, false, hasSelection)) {
                if (auto sel = GetSelectedObject()) {
                    sel->position = glm::vec3(0.0f);
                    sel->rotation = glm::vec3(0.0f);
                    sel->scale = glm::vec3(1.0f);
                }
            }
            if (ImGui::MenuItem("Deseleccionar", nullptr, false, hasSelection)) {
                selectedObject.reset();
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Vista")) {
            ImGui::MenuItem("Mostrar Rejilla 3D (Grid)", nullptr, &showGrid);
            ImGui::MenuItem("Modo Malla Alambrica Global", nullptr, &globalWireframe);
            ImGui::MenuItem("Mostrar Texturas", nullptr, &globalShowTextures);
            ImGui::MenuItem("Usar Shaders (GLSL)", nullptr, &useShaders, shader.IsValid());
            ImGui::MenuItem("Editor de Shaders...", nullptr, &showShaderEditor);
            ImGui::Separator();
            ImGui::MenuItem("Resaltar Seleccion", nullptr, &showSelectionHighlight);
            ImGui::MenuItem("Resaltado con pulso", nullptr, &selectionPulse);
            ImGui::ColorEdit3("Color seleccion", glm::value_ptr(selectionColor),
                              ImGuiColorEditFlags_NoInputs);
            ImGui::Separator();
            if (ImGui::MenuItem("Resetear Posicion Camara")) {
                ResetCamera();
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Debug")) {
            ImGui::MenuItem("Modo Debug", "F1", &debugMode);
            ImGui::Separator();
            if (!debugMode) ImGui::BeginDisabled();
            ImGui::MenuItem("AABB (verde)", nullptr, &debugShowAABB);
            ImGui::MenuItem("OBB orientada (amarillo)", nullptr, &debugShowOBB);
            ImGui::MenuItem("Caja de grupo / hijos (azul)", nullptr, &debugShowGroupAABB);
            ImGui::Separator();
            ImGui::MenuItem("Normales de vertice (cian)", nullptr, &debugShowVertexNormals);
            ImGui::MenuItem("Normales de cara (magenta)", nullptr, &debugShowFaceNormals);
            ImGui::SetNextItemWidth(120.0f);
            ImGui::SliderFloat("Longitud normales", &debugNormalLength, 0.005f, 0.5f, "%.3f",
                               ImGuiSliderFlags_Logarithmic);
            ImGui::Separator();
            ImGui::MenuItem("Solo objeto seleccionado", nullptr, &debugOnlySelected);
            ImGui::MenuItem("Ver a traves de los objetos", nullptr, &debugThroughObjects);
            ImGui::SetNextItemWidth(120.0f);
            ImGui::SliderFloat("Grosor linea", &debugLineWidth, 1.0f, 5.0f, "%.1f");
            if (!debugMode) ImGui::EndDisabled();
            ImGui::EndMenu();
        }

        ImGui::EndMainMenuBar();
    }

    // 2. Panel Izquierdo: Jerarquía de Escena
    ImGui::SetNextWindowPos(ImVec2(10.0f, 30.0f), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(290.0f, 360.0f), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Jerarquia de Escena", nullptr)) {
        ImGui::Text("Modelos en Escena: %zu", objects.size());
        ImGui::Separator();

        // Botones de acción rápida
        if (ImGui::Button("+ Cargar FBX/OBJ", ImVec2(130, 26))) {
            showLoadModal = true;
        }
        ImGui::SameLine();
        if (ImGui::Button("+ Crear Vacio", ImVec2(134, 26))) {
            CreateEmptyObject(nullptr);
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Crea un GameObject vacio en la raiz.\n"
                              "Arrastra modelos encima para agruparlos dentro.");
        }
        bool hasSelection = (selectedObject != nullptr);
        if (!hasSelection) ImGui::BeginDisabled();
        if (ImGui::Button("Duplicar", ImVec2(65, 26))) {
            DuplicateSelected();
        }
        ImGui::SameLine();
        if (ImGui::Button("Eliminar", ImVec2(65, 26))) {
            DeleteSelected();
        }
        if (!hasSelection) ImGui::EndDisabled();

        ImGui::Separator();

        // Árbol de objetos: cada FBX con sus nodos (p.ej. BakerHouse.fbx > Baker_house, Chimney)
        ImGui::BeginChild("ObjetosList", ImVec2(0, 0), true);
        if (objects.empty()) {
            ImGui::TextDisabled("No hay modelos cargados.");
            ImGui::TextWrapped("Arrastra un archivo .FBX o .OBJ a esta ventana para agregarlo.");
        } else {
            for (const auto& obj : objects) {
                DrawHierarchyNode(obj);
            }
        }

        // Espacio libre bajo el árbol: soltar aquí un objeto lo saca a la raíz (al final)
        {
            ImVec2 avail = ImGui::GetContentRegionAvail();
            ImGui::InvisibleButton("##RaizEscena", ImVec2((std::max)(avail.x, 1.0f), (std::max)(avail.y, 24.0f)));
            if (ImGui::BeginDragDropTarget()) {
                if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("HIERARCHY_NODE")) {
                    ModelObject* dragged = *(ModelObject* const*)payload->Data;
                    pendingReparent = { dragged, nullptr, -1, true };
                }
                ImGui::EndDragDropTarget();
            }
            // Clic izquierdo en el vacío = deseleccionar
            if (ImGui::IsItemClicked(ImGuiMouseButton_Left)) {
                selectedObject.reset();
            }
        }

        // Clic derecho en una zona sin objetos: menú para crear un vacío en la raíz
        if (ImGui::BeginPopupContextWindow("##MenuJerarquia",
                ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems)) {
            if (ImGui::MenuItem("Crear GameObject Vacio")) {
                pendingAction = PendingAction::CREATE_EMPTY;
            }
            if (ImGui::MenuItem("Cargar FBX/OBJ...")) {
                showLoadModal = true;
            }
            ImGui::EndPopup();
        }
        revealSelectionInHierarchy = false;
        ImGui::EndChild();

        // Ejecutar ahora lo pedido desde el menú contextual o el arrastre
        // (ya no se está recorriendo el árbol, así que es seguro modificarlo)
        if (pendingReparent.active) {
            ReparentObject(pendingReparent.obj, pendingReparent.newParent, pendingReparent.index);
            pendingReparent = PendingReparent{};
        }
        if (pendingAction == PendingAction::DUPLICATE) DuplicateSelected();
        else if (pendingAction == PendingAction::DELETE_OBJ) DeleteSelected();
        else if (pendingAction == PendingAction::CREATE_EMPTY) CreateEmptyObject(nullptr);
        else if (pendingAction == PendingAction::CREATE_EMPTY_CHILD) CreateEmptyObject(selectedObject.get());
        pendingAction = PendingAction::NONE;
    }
    ImGui::End();

    // 3. Panel Derecho: Inspector de Propiedades
    ImGui::SetNextWindowPos(ImVec2((float)window.GetWidth() - 360.0f, 30.0f), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(350.0f, 580.0f), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Inspector de Modelo", nullptr)) {
        auto sel = GetSelectedObject();
        if (sel) {
            // Nombre del objeto (editable en directo)
            ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "IDENTIFICACION");
            char nameBuf[128];
            strncpy_s(nameBuf, sel->name.c_str(), sizeof(nameBuf));
            if (ImGui::InputText("Nombre", nameBuf, sizeof(nameBuf))) {
                sel->name = nameBuf;
            }
            ImGui::Text("Padre: %s", sel->parent ? sel->parent->name.c_str() : "(ninguno, raiz de escena)");
            ImGui::Text("Hijos: %zu", sel->children.size());

            ImGui::Spacing();
            ImGui::Separator();

            // Transform: Posición, Rotación (Girar), Escala
            ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "TRANSFORM");

            ImGui::DragFloat3("Posicion", glm::value_ptr(sel->position), 0.05f, 0.0f, 0.0f, "%.2f");

            // --- GIRAR EL MODELO ---
            ImGui::Text("Rotacion (Giro en Grados):");
            ImGui::DragFloat3("Rotacion", glm::value_ptr(sel->rotation), 1.0f, -360.0f, 360.0f, "%.1f deg");

            // Botones rápidos de giro para mayor comodidad de modelado
            ImGui::Text("Giro rapido:");
            if (ImGui::Button("+45 deg Y"))  sel->rotation.y += 45.0f;
            ImGui::SameLine();
            if (ImGui::Button("+90 deg Y"))  sel->rotation.y += 90.0f;
            ImGui::SameLine();
            if (ImGui::Button("+180 deg Y")) sel->rotation.y += 180.0f;
            ImGui::SameLine();
            if (ImGui::Button("Reset Rot"))  sel->rotation = glm::vec3(0.0f);

            // Escala
            ImGui::DragFloat3("Escala", glm::value_ptr(sel->scale), 0.02f, 0.001f, 1000.0f, "%.3f");

            if (ImGui::Button("Restablecer Transform Completo")) {
                sel->position = glm::vec3(0.0f);
                sel->rotation = glm::vec3(0.0f);
                sel->scale    = glm::vec3(1.0f);
            }

            ImGui::Spacing();
            ImGui::Separator();

            // Rotación Automática
            ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "ANIMACION DE GIRO");
            ImGui::Checkbox("Giro Continuo Automatico", &sel->autoRotate);
            if (sel->autoRotate) {
                ImGui::SliderFloat("Velocidad Giro", &sel->autoRotateSpeed, -180.0f, 180.0f, "%.1f deg/s");
            }

            ImGui::Spacing();
            ImGui::Separator();

            // Material y Visualización
            ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "MATERIAL Y VISUALIZACION");
            ImGui::ColorEdit3("Color Difuso", glm::value_ptr(sel->color));
            ImGui::Checkbox("Malla Alambrica (Wireframe)", &sel->wireframe);
            ImGui::Checkbox("Visible en Escena", &sel->visible);

            ImGui::Spacing();
            ImGui::Separator();

            // Un GameObject vacío no tiene malla: sólo se muestran Transform y acciones
            if (!sel->mesh) {
                ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "GAMEOBJECT VACIO");
                ImGui::TextWrapped("No tiene malla. Sirve de contenedor: arrastra objetos encima suyo en la "
                                   "Jerarquia para agruparlos. Al moverlo, girarlo o escalarlo, sus hijos le siguen.");
            } else {
                // Texturas del modelo
                ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "TEXTURA");
                ImGui::Checkbox("Mostrar Textura", &sel->showTexture);
                if (!globalShowTextures) {
                    ImGui::SameLine();
                    ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.2f, 1.0f), "(desactivadas en Vista)");
                }
                if (!sel->mesh->HasUVs()) {
                    ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.3f, 1.0f), "El modelo no tiene coordenadas UV.");
                }

                // Textura asignada a mano (arrastrando una imagen fuera de las miniaturas)
                if (sel->overrideTexture && sel->overrideTexture->IsLoaded()) {
                    ImGui::TextWrapped("Manual: %s", sel->overrideTexture->GetPath().c_str());
                    ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.2f, 1.0f), "(tapa las texturas de los materiales)");
                    if (ImGui::Button("Quitar textura manual")) {
                        sel->overrideTexture.reset();
                    }
                }

                // Texturas de los materiales del FBX.
                // Cada miniatura es una zona donde se puede soltar una imagen para SUSTITUIRLA.
                // Si el nodo tiene mallas propias, sólo se muestran los materiales que usan.
                const auto& mats = sel->mesh->GetMaterials();
                std::set<unsigned int> usedMats;
                for (unsigned int idx : sel->meshIndices) usedMats.insert(sel->mesh->GetMeshMaterial(idx));

                const float thumbSize = 96.0f;
                int shownMats = 0;
                for (size_t m = 0; m < mats.size(); ++m) {
                    if (!usedMats.empty() && usedMats.count((unsigned int)m) == 0) continue;
                    const auto& mt = mats[m];
                    ++shownMats;
                    ImGui::PushID((int)m);

                    // Miniatura (o recuadro vacío si el material no tiene textura)
                    if (mt.texture && mt.texture->IsLoaded()) {
                        ImGui::Image((ImTextureID)(intptr_t)mt.texture->GetID(), ImVec2(thumbSize, thumbSize),
                                     ImVec2(0, 1), ImVec2(1, 0)); // invertir V para verla derecha
                    } else {
                        ImGui::Button("Sin textura\n\nSuelta aqui\nuna imagen", ImVec2(thumbSize, thumbSize));
                    }

                    // Guardar el rectángulo de la miniatura como zona de soltado
                    ImVec2 rmin = ImGui::GetItemRectMin();
                    ImVec2 rmax = ImGui::GetItemRectMax();
                    textureDropZones.push_back({ glm::vec2(rmin.x, rmin.y), glm::vec2(rmax.x, rmax.y),
                                                 sel->mesh, (unsigned int)m });

                    // Resaltar la miniatura mientras se arrastra un fichero por encima
                    const bool hovered = isDraggingFile &&
                        dragPos.x >= rmin.x && dragPos.x <= rmax.x && dragPos.y >= rmin.y && dragPos.y <= rmax.y;
                    ImGui::GetWindowDrawList()->AddRect(rmin, rmax,
                        hovered ? IM_COL32(80, 220, 120, 255) : IM_COL32(110, 110, 130, 255),
                        4.0f, 0, hovered ? 3.0f : 1.0f);

                    // Información del material a la derecha de la miniatura
                    ImGui::SameLine();
                    ImGui::BeginGroup();
                    ImGui::Text("Material %zu", m);
                    if (mt.texture && mt.texture->IsLoaded()) {
                        ImGui::TextWrapped("%s", mt.resolvedPath.c_str());
                        ImGui::Text("%dx%d%s", mt.texture->GetWidth(), mt.texture->GetHeight(),
                                    mt.embedded ? " [embebida]" : "");
                    } else if (!mt.requestedPath.empty()) {
                        ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "NO encontrada:");
                        ImGui::TextWrapped("%s", mt.requestedPath.c_str());
                    } else {
                        ImGui::TextDisabled("El FBX no le asigna textura");
                    }
                    if (mt.replaced) {
                        ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.5f, 1.0f), "Sustituida");
                        if (ImGui::Button("Restaurar original")) {
                            sel->mesh->RestoreMaterialTexture((unsigned int)m);
                        }
                    }
                    ImGui::EndGroup();

                    ImGui::PopID();
                }
                if (shownMats == 0) {
                    ImGui::TextDisabled("Este nodo no tiene materiales.");
                }
                ImGui::TextWrapped("Tip: arrastra un .png/.dds/.jpg ENCIMA de una miniatura para sustituir esa textura. "
                                   "Si lo sueltas en otro sitio, se aplica como textura manual al objeto seleccionado.");

                ImGui::Spacing();
                ImGui::Separator();

                // Información técnica de la malla FBX
                ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "INFORMACION GEOMETRICA");
                unsigned int nodeVerts = 0, nodeTris = 0;
                for (unsigned int idx : sel->meshIndices) {
                    nodeVerts += sel->mesh->GetMeshVertices(idx);
                    nodeTris  += sel->mesh->GetMeshTriangles(idx);
                }
                ImGui::Text("Submallas en este nodo: %zu", sel->meshIndices.size());
                {
                    // Normales de las submallas de este nodo
                    unsigned int fromFile = 0, generated = 0, none = 0;
                    for (unsigned int idx : sel->meshIndices) {
                        if (!sel->mesh->MeshHasNormals(idx)) ++none;
                        else if (sel->mesh->MeshNormalsFromFile(idx)) ++fromFile;
                        else ++generated;
                    }
                    if (!sel->meshIndices.empty()) {
                        if (fromFile > 0)
                            ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.5f, 1.0f), "Normales: importadas del fichero (%u submallas)", fromFile);
                        if (generated > 0)
                            ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.3f, 1.0f), "Normales: generadas por Assimp (%u submallas)", generated);
                        if (none > 0)
                            ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "Sin normales: %u submallas", none);
                    }
                    if (ImGui::Checkbox("Ver normales de vertice", &debugShowVertexNormals) && debugShowVertexNormals)
                        debugMode = true;
                    ImGui::SameLine();
                    if (ImGui::Checkbox("De cara", &debugShowFaceNormals) && debugShowFaceNormals)
                        debugMode = true;
                    if (debugShowVertexNormals || debugShowFaceNormals) {
                        ImGui::SliderFloat("Longitud", &debugNormalLength, 0.005f, 0.5f, "%.3f",
                                           ImGuiSliderFlags_Logarithmic);
                    }
                }
                ImGui::Text("Vertices del nodo: %u", nodeVerts);
                ImGui::Text("Triangulos del nodo: %u", nodeTris);
                ImGui::Text("Vertices Totales (FBX): %u", sel->mesh->GetTotalVertices());
                ImGui::Text("Triangulos Totales (FBX): %u", sel->mesh->GetTotalTriangles());
                ImGui::Text("Radio AABB: %.2f", sel->mesh->GetRadius());
                glm::vec3 c = sel->mesh->GetCenter();
                ImGui::Text("Centro AABB: (%.2f, %.2f, %.2f)", c.x, c.y, c.z);
                ImGui::TextWrapped("Fichero: %s", sel->filePath.c_str());
            } // fin if (sel->mesh)

            ImGui::Spacing();
            ImGui::Separator();

            // Caja envolvente en coordenadas de mundo
            ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "BOUNDING BOX (MUNDO)");
            ImGui::Checkbox("Modo Debug (F1)", &debugMode);
            {
                glm::vec3 mn, mx;
                if (ComputeWorldAABB(sel.get(), false, mn, mx)) {
                    const glm::vec3 size = mx - mn;
                    ImGui::TextColored(ImVec4(0.3f, 0.9f, 0.4f, 1.0f), "AABB de su malla:");
                    ImGui::Text("  Min:    (%.2f, %.2f, %.2f)", mn.x, mn.y, mn.z);
                    ImGui::Text("  Max:    (%.2f, %.2f, %.2f)", mx.x, mx.y, mx.z);
                    ImGui::Text("  Tamano: (%.2f, %.2f, %.2f)", size.x, size.y, size.z);
                } else {
                    ImGui::TextDisabled("Este nodo no tiene malla propia.");
                }
                if (!sel->children.empty() && ComputeWorldAABB(sel.get(), true, mn, mx)) {
                    const glm::vec3 size = mx - mn;
                    const glm::vec3 center = (mn + mx) * 0.5f;
                    ImGui::TextColored(ImVec4(0.4f, 0.7f, 1.0f, 1.0f), "AABB con sus hijos:");
                    ImGui::Text("  Centro: (%.2f, %.2f, %.2f)", center.x, center.y, center.z);
                    ImGui::Text("  Tamano: (%.2f, %.2f, %.2f)", size.x, size.y, size.z);
                }
            }

            ImGui::Spacing();
            ImGui::Separator();

            // Acciones directas
            ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "ACCIONES");
            if (ImGui::Button("Duplicar Fichero/Objeto", ImVec2(160, 28))) {
                DuplicateSelected();
            }
            ImGui::SameLine();
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.7f, 0.2f, 0.2f, 1.0f));
            if (ImGui::Button("Eliminar de Escena", ImVec2(150, 28))) {
                DeleteSelected();
            }
            ImGui::PopStyleColor();

        } else {
            ImGui::TextDisabled("Ningun modelo seleccionado.");
            ImGui::Spacing();
            ImGui::TextWrapped("Selecciona un objeto de la 'Jerarquia de Escena' para modificar su transformacion, girarlo, cambiar su nombre o duplicarlo.");
            ImGui::Spacing();
            ImGui::TextWrapped("Tambien puedes arrastrar y soltar cualquier fichero .FBX o .OBJ en la ventana.");
        }
    }
    ImGui::End();

    // Etiqueta en pantalla con la pieza seleccionada (arriba, en el centro)
    if (selectedObject) {
        std::string path = selectedObject->name;
        for (const ModelObject* p = selectedObject->parent; p; p = p->parent) {
            path = p->name + "  >  " + path;
        }
        ImGui::SetNextWindowPos(ImVec2((float)window.GetWidth() * 0.5f, 34.0f), ImGuiCond_Always, ImVec2(0.5f, 0.0f));
        ImGui::SetNextWindowBgAlpha(0.65f);
        ImGuiWindowFlags labelFlags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize |
                                      ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing |
                                      ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoInputs;
        if (ImGui::Begin("EtiquetaSeleccion", nullptr, labelFlags)) {
            const glm::vec3& c = selectionColor;
            ImGui::TextColored(ImVec4(c.r, c.g, c.b, 1.0f), "Seleccionado:");
            ImGui::SameLine();
            ImGui::TextUnformatted(path.c_str());
        }
        ImGui::End();
    }

    // Leyenda del Modo Debug (esquina inferior izquierda)
    if (debugMode) {
        ImGui::SetNextWindowPos(ImVec2(10.0f, (float)window.GetHeight() - 50.0f), ImGuiCond_Always, ImVec2(0.0f, 1.0f));
        ImGui::SetNextWindowBgAlpha(0.6f);
        ImGuiWindowFlags legendFlags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize |
                                       ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing |
                                       ImGuiWindowFlags_NoNav;
        if (ImGui::Begin("LeyendaDebug", nullptr, legendFlags)) {
            ImGui::Text("MODO DEBUG (F1)");
            ImGui::Separator();
            ImGui::Checkbox("##aabb", &debugShowAABB);  ImGui::SameLine();
            ImGui::TextColored(ImVec4(0.1f, 0.8f, 0.25f, 1.0f), "AABB");
            ImGui::Checkbox("##obb", &debugShowOBB);    ImGui::SameLine();
            ImGui::TextColored(ImVec4(0.9f, 0.75f, 0.1f, 1.0f), "OBB (orientada)");
            ImGui::Checkbox("##grp", &debugShowGroupAABB); ImGui::SameLine();
            ImGui::TextColored(ImVec4(0.2f, 0.55f, 0.95f, 1.0f), "Grupo (padre + hijos)");
            ImGui::Checkbox("##vn", &debugShowVertexNormals); ImGui::SameLine();
            ImGui::TextColored(ImVec4(0.2f, 0.9f, 0.95f, 1.0f), "Normales de vertice");
            ImGui::Checkbox("##fn", &debugShowFaceNormals); ImGui::SameLine();
            ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.8f, 1.0f), "Normales de cara");
            ImGui::Checkbox("Solo seleccionado", &debugOnlySelected);
        }
        ImGui::End();
    }

    // 4. Barra Inferior de Información y Ayuda
    ImGui::SetNextWindowPos(ImVec2(10.0f, (float)window.GetHeight() - 40.0f), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2((float)window.GetWidth() - 20.0f, 30.0f), ImGuiCond_Always);
    ImGuiWindowFlags statusFlags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoBackground;
    if (ImGui::Begin("BarraEstado", nullptr, statusFlags)) {
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f),
            "Controles 3D: Clic Derecho + Arrastrar = Orbitar Camara | Rueda Raton = Zoom | Drag & Drop = Importar FBX | Clic Izq = Seleccionar pieza (Alt = modelo entero) | F1 = Modo Debug (AABB)");
    }
    ImGui::End();

    // Editor de Shaders (ventana flotante, se abre desde Vista)
    DrawShaderEditor();

    // 5. Diálogo Modal para Cargar Modelo Manualmente
    if (showLoadModal) {
        ImGui::OpenPopup("Cargar Archivo de Modelo");
    }
    if (ImGui::BeginPopupModal("Cargar Archivo de Modelo", &showLoadModal, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Introduce la ruta completa o relativa del archivo FBX u OBJ:");
        ImGui::InputText("Ruta", loadPathBuffer, sizeof(loadPathBuffer));

        ImGui::Spacing();
        if (ImGui::Button("Cargar", ImVec2(100, 28))) {
            if (std::strlen(loadPathBuffer) > 0) {
                AddModel(loadPathBuffer);
                showLoadModal = false;
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancelar", ImVec2(100, 28))) {
            showLoadModal = false;
        }
        ImGui::EndPopup();
    }
}

// ─────────────────────────────────────────────
//  CleanUp: liberar GPU e ImGui
// ─────────────────────────────────────────────

void Renderer::CleanUp() {
    shader.CleanUp();
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();

    objects.clear();
}
