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

void Renderer::HandleDroppedFile(const std::string& path) {
    const std::string ext = ToLowerExt(path);
    const bool isImage = (ext == "png" || ext == "jpg" || ext == "jpeg" || ext == "dds" ||
                          ext == "tga" || ext == "bmp" || ext == "tif" || ext == "tiff");
    if (isImage) {
        ApplyTextureToSelected(path);
    } else {
        AddModel(path);
    }
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

    glMatrixMode(GL_MODELVIEW);
    glLoadMatrixf(glm::value_ptr(view));

    // Posición de luz fija en el mundo
    GLfloat lightPos[] = { 5.0f, 10.0f, 8.0f, 0.0f };
    glLightfv(GL_LIGHT0, GL_POSITION, lightPos);

    // Dibujar Rejilla 3D de suelo
    if (showGrid) {
        RenderGrid();
    }

    // Dibujar Objetos FBX / OBJ de la escena
    if (!objects.empty()) {
        RenderObjects(view);
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
    glEnable(GL_LIGHTING);
    glEnable(GL_NORMALIZE);

    for (const auto& obj : objects) {
        RenderNode(obj, glm::mat4(1.0f), view, nullptr, true, false, false);
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

    for (const auto& child : obj->children) {
        RenderNode(child, world, view, nodeTexture, showTex, wireframe, isSelected);
    }
}

// ─────────────────────────────────────────────
//  DrawHierarchyNode: un nodo del árbol de la jerarquía
// ─────────────────────────────────────────────

void Renderer::DrawHierarchyNode(const std::shared_ptr<ModelObject>& obj) {
    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick |
                               ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_DefaultOpen;
    if (obj->children.empty()) flags |= ImGuiTreeNodeFlags_Leaf;
    if (obj == selectedObject) flags |= ImGuiTreeNodeFlags_Selected;

    const char* visTag = obj->visible ? "[v] " : "[x] ";
    const bool open = ImGui::TreeNodeEx((void*)obj.get(), flags, "%s%s", visTag, obj->name.c_str());

    // Clic sobre el nombre (no sobre la flecha) = seleccionar
    if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen()) {
        selectedObject = obj;
    }

    // Menú contextual con clic derecho sobre el objeto
    if (ImGui::BeginPopupContextItem()) {
        selectedObject = obj;
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
//  DrawUI: Interfaz estilo aplicación 3D / modelado
// ─────────────────────────────────────────────

void Renderer::DrawUI(Window& window) {
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
            ImGui::Separator();
            if (ImGui::MenuItem("Resetear Posicion Camara")) {
                ResetCamera();
            }
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
        ImGui::EndChild();

        // Ejecutar ahora lo pedido desde el menú contextual (ya no se está recorriendo el árbol)
        if (pendingAction == PendingAction::DUPLICATE) DuplicateSelected();
        else if (pendingAction == PendingAction::DELETE_OBJ) DeleteSelected();
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

            // Textura asignada a mano (arrastrando una imagen)
            const Texture* previewTex = nullptr;
            if (sel->overrideTexture && sel->overrideTexture->IsLoaded()) {
                previewTex = sel->overrideTexture.get();
                ImGui::TextWrapped("Manual: %s", sel->overrideTexture->GetPath().c_str());
                if (ImGui::Button("Quitar textura manual")) {
                    sel->overrideTexture.reset();
                    previewTex = nullptr;
                }
            }

            // Texturas que vienen del FBX
            // Si el nodo tiene mallas propias, sólo se muestran los materiales que usan
            const auto& mats = sel->mesh->GetMaterials();
            std::set<unsigned int> usedMats;
            for (unsigned int idx : sel->meshIndices) usedMats.insert(sel->mesh->GetMeshMaterial(idx));
            int texCount = 0;
            for (size_t m = 0; m < mats.size(); ++m) {
                if (!usedMats.empty() && usedMats.count((unsigned int)m) == 0) continue;
                const auto& mt = mats[m];
                if (mt.requestedPath.empty()) continue;
                ++texCount;
                if (mt.texture && mt.texture->IsLoaded()) {
                    ImGui::TextWrapped("Mat %zu: %s (%dx%d)%s", m, mt.resolvedPath.c_str(),
                        mt.texture->GetWidth(), mt.texture->GetHeight(), mt.embedded ? " [embebida]" : "");
                    if (!previewTex) previewTex = mt.texture.get();
                } else {
                    ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "Mat %zu: NO encontrada", m);
                    ImGui::TextWrapped("  %s", mt.requestedPath.c_str());
                }
            }
            if (texCount == 0 && !sel->overrideTexture) {
                ImGui::TextDisabled("El FBX no referencia ninguna textura.");
            }
            ImGui::TextWrapped("Tip: con el objeto seleccionado, arrastra un .png/.dds/.jpg a la ventana para asignarle textura.");

            if (previewTex) {
                ImGui::Image((ImTextureID)(intptr_t)previewTex->GetID(), ImVec2(128, 128),
                             ImVec2(0, 1), ImVec2(1, 0)); // invertir V para verla derecha
            }

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
            ImGui::Text("Vertices del nodo: %u", nodeVerts);
            ImGui::Text("Triangulos del nodo: %u", nodeTris);
            ImGui::Text("Vertices Totales (FBX): %u", sel->mesh->GetTotalVertices());
            ImGui::Text("Triangulos Totales (FBX): %u", sel->mesh->GetTotalTriangles());
            ImGui::Text("Radio AABB: %.2f", sel->mesh->GetRadius());
            glm::vec3 c = sel->mesh->GetCenter();
            ImGui::Text("Centro AABB: (%.2f, %.2f, %.2f)", c.x, c.y, c.z);
            ImGui::TextWrapped("Fichero: %s", sel->filePath.c_str());

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

    // 4. Barra Inferior de Información y Ayuda
    ImGui::SetNextWindowPos(ImVec2(10.0f, (float)window.GetHeight() - 40.0f), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2((float)window.GetWidth() - 20.0f, 30.0f), ImGuiCond_Always);
    ImGuiWindowFlags statusFlags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoBackground;
    if (ImGui::Begin("BarraEstado", nullptr, statusFlags)) {
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f),
            "Controles 3D: Clic Derecho + Arrastrar = Orbitar Camara | Rueda Raton = Zoom | Drag & Drop = Importar FBX");
    }
    ImGui::End();

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
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();

    objects.clear();
}
