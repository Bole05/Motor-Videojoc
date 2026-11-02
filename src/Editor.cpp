#include "Editor.h"
#include "Renderer.h"
#include "GameObject.h"
#include "Geometria.h"
#include "Texture.h"
#include "Shader.h"
#include "Log.h"
#include "SystemInfo.h"
#include <glad/glad.h>
#include <glm/gtc/type_ptr.hpp>
#include <IL/il.h>
#include <assimp/version.h>
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <functional>
#include <set>

#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_opengl3.h>

// ─────────────────────────────────────────────
//  Inicialización / cierre de Dear ImGui
// ─────────────────────────────────────────────

Editor::~Editor() {
    CleanUp();
}

bool Editor::Init(Window& window) {
    // Comprueba que la versión de ImGui de las cabeceras y de la librería encaja
    IMGUI_CHECKVERSION();
    // Crea el contexto de ImGui: a partir de aquí guarda el estado de la interfaz
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    // Habilita la navegación por teclado en los paneles
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    // Estilo visual oscuro con esquinas redondeadas
    ImGui::StyleColorsDark();
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 6.0f;
    style.FrameRounding = 4.0f;
    style.GrabRounding = 4.0f;
    style.PopupRounding = 4.0f;
    style.ScrollbarRounding = 4.0f;

    // Conecta ImGui con SDL3 (input) y con OpenGL (render)
    if (!ImGui_ImplSDL3_InitForOpenGL(window.GetWindow(), window.GetContext())) {
        LOG("Error al inicializar ImGui_ImplSDL3");
        return false;
    }
    if (!ImGui_ImplOpenGL3_Init("#version 130")) {
        LOG("Error al inicializar ImGui_ImplOpenGL3");
        return false;
    }
    initialized = true;
    LOG("Editor (Dear ImGui %s) inicializado correctamente.", IMGUI_VERSION);

    // El editor de shaders empieza mostrando el código que ha cargado el Renderer
    shaderStatusOk = renderer.shader.IsValid() && renderer.shader.GetLastError().empty();
    shaderStatus = shaderStatusOk ? "Shader compilado correctamente." : renderer.shader.GetLastError();
    CopyShaderSourcesToEditor();
    return true;
}

void Editor::CleanUp() {
    if (!initialized) return;
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();
    initialized = false;
}

void Editor::ProcessEvent(const SDL_Event& event) {
    ImGui_ImplSDL3_ProcessEvent(&event);
}

bool Editor::WantCaptureMouse() const {
    return ImGui::GetIO().WantCaptureMouse;
}

bool Editor::WantTextInput() const {
    return ImGui::GetIO().WantTextInput;
}

void Editor::RecordFrameTime(float deltaTime) {
    if (deltaTime <= 0.0f) return;
    fpsHistory[fpsHistoryIndex] = 1.0f / deltaTime;
    msHistory[fpsHistoryIndex] = deltaTime * 1000.0f;
    fpsHistoryIndex = (fpsHistoryIndex + 1) % FPS_HISTORY;
}

void Editor::BeginFrame() {
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();
}

void Editor::Draw(Window& window) {
    // Si la selección ha cambiado fuera de la jerarquía (vista 3D, duplicar, cargar...),
    // la jerarquía abrirá los padres y se desplazará hasta el objeto
    if (renderer.selectedObject.get() != lastSelected) {
        lastSelected = renderer.selectedObject.get();
        revealSelectionInHierarchy = (lastSelected != nullptr);
    }
    DrawUI(window);
}

void Editor::EndFrame() {
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

static std::string ToLowerExt(const std::string& path) {
    size_t dot = path.find_last_of('.');
    if (dot == std::string::npos) return "";
    std::string ext = path.substr(dot + 1);
    for (auto& ch : ext) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    return ext;
}

const Editor::TextureDropZone* Editor::FindDropZone(const glm::vec2& p) const {
    for (const auto& zone : textureDropZones) {
        if (p.x >= zone.min.x && p.x <= zone.max.x && p.y >= zone.min.y && p.y <= zone.max.y) {
            return &zone;
        }
    }
    return nullptr;
}

void Editor::HandleDroppedFile(const std::string& path, float x, float y) {
    const std::string ext = ToLowerExt(path);
    const bool isImage = (ext == "png" || ext == "jpg" || ext == "jpeg" || ext == "dds" ||
                          ext == "tga" || ext == "bmp" || ext == "tif" || ext == "tiff");
    if (!isImage) {
        isDraggingFile = false;
        renderer.AddModel(path);
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
    renderer.ApplyTextureToSelected(path);
}


void Editor::DrawHierarchyNode(const std::shared_ptr<GameObject>& obj) {
    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick |
                               ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_DefaultOpen;
    if (obj->children.empty()) flags |= ImGuiTreeNodeFlags_Leaf;
    if (obj == renderer.selectedObject) flags |= ImGuiTreeNodeFlags_Selected;

    // Si la selección viene de la vista 3D, abrir los padres para que se vea en el árbol
    if (revealSelectionInHierarchy && renderer.selectedObject && obj != renderer.selectedObject &&
        (renderer.selectedObject.get())->IsSelfOrDescendantOf(obj.get())) {
        ImGui::SetNextItemOpen(true);
    }

    const char* visTag = obj->visible ? "[v] " : "[x] ";
    const char* typeTag = obj->mesh->geometry ? "" : "(o) "; // (o) = GameObject vacío
    const bool open = ImGui::TreeNodeEx((void*)obj.get(), flags, "%s%s%s", visTag, typeTag, obj->name.c_str());
    const ImVec2 rowMin = ImGui::GetItemRectMin();
    const ImVec2 rowMax = ImGui::GetItemRectMax();
    if (revealSelectionInHierarchy && obj == renderer.selectedObject) {
        ImGui::SetScrollHereY(0.5f);   // desplazar la lista hasta el objeto seleccionado
    }

    // Clic sobre el nombre (no sobre la flecha) = seleccionar
    if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen()) {
        renderer.selectedObject = obj;
        lastSelected = obj.get();   // seleccionado desde la propia jerarquía: no hace falta desplazarla
    }

    // --- Arrastrar este objeto ---
    if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None)) {
        GameObject* raw = obj.get();
        ImGui::SetDragDropPayload("HIERARCHY_NODE", &raw, sizeof(GameObject*));
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
            GameObject* dragged = *(GameObject* const*)payload->Data;
            const float h = rowMax.y - rowMin.y;
            const float my = ImGui::GetMousePos().y;
            enum { BEFORE, INSIDE, AFTER } zone = INSIDE;
            if (my < rowMin.y + h * 0.25f)      zone = BEFORE;
            else if (my > rowMax.y - h * 0.25f) zone = AFTER;

            GameObject* newParent = (zone == INSIDE) ? obj.get() : obj->parent;
            const bool valid = dragged && dragged != obj.get() &&
                               !(newParent && (newParent)->IsSelfOrDescendantOf(dragged));

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
                    auto& siblings = renderer.SiblingList(obj->parent);
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
        renderer.selectedObject = obj;
        lastSelected = obj.get();   // seleccionado desde la propia jerarquía: no hace falta desplazarla
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


void Editor::CopyShaderSourcesToEditor() {
    auto fill = [](std::vector<char>& buf, const std::string& src) {
        buf.assign(src.begin(), src.end());
        buf.resize((std::max)(src.size() + 8192, (size_t)32768), '\0');   // margen para escribir
    };
    fill(vertexEditBuf, renderer.shader.GetVertexSource());
    fill(fragmentEditBuf, renderer.shader.GetFragmentSource());
}

void Editor::CompileShaderFromEditor() {
    const std::string vs(vertexEditBuf.data());
    const std::string fs(fragmentEditBuf.data());
    if (renderer.shader.Compile(vs, fs)) {
        shaderStatusOk = true;
        shaderStatus = "Compilado correctamente.";
    } else {
        shaderStatusOk = false;
        shaderStatus = renderer.shader.GetLastError() + "\n(Se sigue usando la ultima version que funcionaba.)";
    }
}

void Editor::DrawShaderEditor() {
    if (!showShaderEditor) return;

    ImGui::SetNextWindowSize(ImVec2(620.0f, 560.0f), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Editor de Shaders (GLSL)", &showShaderEditor)) {
        ImGui::End();
        return;
    }

    // --- Ajustes rápidos ---
    ImGui::Checkbox("Usar Shaders", &renderer.useShaders);
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Desactivado = pipeline fijo de OpenGL (como antes), para comparar.");
    ImGui::SameLine();
    const char* modes[] = { "Blinn-Phong (luz por pixel)", "Normales como color", "Toon (dibujo animado)",
                            "Sin iluminacion", "Coordenadas UV como color" };
    ImGui::SetNextItemWidth(230.0f);
    ImGui::Combo("Modo", &renderer.shaderMode, modes, IM_ARRAYSIZE(modes));

    if (!renderer.useShaders) ImGui::BeginDisabled();
    ImGui::SliderFloat("Ambiente", &renderer.shaderAmbient, 0.0f, 1.0f, "%.2f");
    ImGui::SliderFloat("Especular", &renderer.shaderSpecular, 0.0f, 1.0f, "%.2f");
    ImGui::SliderFloat("Brillo (shininess)", &renderer.shaderShininess, 2.0f, 256.0f, "%.0f", ImGuiSliderFlags_Logarithmic);
    ImGui::DragFloat3("Direccion luz", glm::value_ptr(renderer.lightDirection), 0.1f);
    ImGui::ColorEdit3("Color luz", glm::value_ptr(renderer.lightColor), ImGuiColorEditFlags_NoInputs);
    if (!renderer.useShaders) ImGui::EndDisabled();

    ImGui::Separator();

    // --- Código ---
    ImGui::TextWrapped("Edita el codigo GLSL y pulsa Compilar. Si hay errores, salen abajo en rojo "
                       "y se sigue usando la ultima version que funcionaba.");
    if (ImGui::BeginTabBar("ShaderTabs")) {
        const ImVec2 editorSize(-1.0f, ImGui::GetContentRegionAvail().y - 110.0f);
        if (ImGui::BeginTabItem("Vertex renderer.shader")) {
            ImGui::InputTextMultiline("##vs", vertexEditBuf.data(), vertexEditBuf.size(),
                                      editorSize, ImGuiInputTextFlags_AllowTabInput);
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Fragment renderer.shader")) {
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
    const bool hasFiles = !renderer.vertexShaderPath.empty();
    if (!hasFiles) ImGui::BeginDisabled();
    if (ImGui::Button("Guardar en ficheros", ImVec2(150, 26))) {
        std::ofstream v(renderer.vertexShaderPath, std::ios::binary), f(renderer.fragmentShaderPath, std::ios::binary);
        v << vertexEditBuf.data();
        f << fragmentEditBuf.data();
        shaderStatusOk = (bool)v && (bool)f;
        shaderStatus = shaderStatusOk ? "Guardado en " + renderer.vertexShaderPath + " y .frag" : "Error al guardar los ficheros.";
        LOG("%s", shaderStatus.c_str());
    }
    ImGui::SameLine();
    if (ImGui::Button("Recargar ficheros", ImVec2(140, 26))) {
        if (renderer.shader.LoadFromFiles(renderer.vertexShaderPath, renderer.fragmentShaderPath)) {
            shaderStatusOk = true;
            shaderStatus = "Recargado desde los ficheros.";
        } else {
            shaderStatusOk = false;
            shaderStatus = renderer.shader.GetLastError();
        }
        CopyShaderSourcesToEditor();
    }
    if (!hasFiles) ImGui::EndDisabled();
    ImGui::SameLine();
    if (ImGui::Button("Restaurar por defecto", ImVec2(160, 26))) {
        renderer.shader.Compile(Shader::DefaultVertexSource(), Shader::DefaultFragmentSource());
        shaderStatusOk = renderer.shader.GetLastError().empty();
        shaderStatus = shaderStatusOk ? "Restaurado el renderer.shader por defecto." : renderer.shader.GetLastError();
        CopyShaderSourcesToEditor();
    }
    if (hasFiles) {
        ImGui::TextDisabled("Ficheros: %s", renderer.vertexShaderPath.c_str());
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
//  Geometrías básicas y textura de comprobación
// ─────────────────────────────────────────────


void Editor::DrawConsolePanel() {
    ImGuiViewport* vp = ImGui::GetMainViewport();
    const float width = (std::max)(400.0f, vp->Size.x - 310.0f - 370.0f);
    ImGui::SetNextWindowPos(ImVec2(310.0f, vp->Size.y - 250.0f), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(width, 200.0f), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Consola", &showConsole)) {
        ImGui::End();
        return;
    }

    // Barra de herramientas
    if (ImGui::Button("Limpiar")) ClearLog();
    ImGui::SameLine();
    bool copy = ImGui::Button("Copiar");
    ImGui::SameLine();
    char label[64];
    snprintf(label, sizeof(label), "Info (%d)", GetLogCount(LogLevel::Info));
    ImGui::Checkbox(label, &consoleShowInfo);
    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.8f, 0.3f, 1.0f));
    snprintf(label, sizeof(label), "Avisos (%d)", GetLogCount(LogLevel::Warning));
    ImGui::Checkbox(label, &consoleShowWarnings);
    ImGui::PopStyleColor();
    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.4f, 0.4f, 1.0f));
    snprintf(label, sizeof(label), "Errores (%d)", GetLogCount(LogLevel::Error));
    ImGui::Checkbox(label, &consoleShowErrors);
    ImGui::PopStyleColor();
    ImGui::SameLine();
    ImGui::Checkbox("Auto-scroll", &consoleAutoScroll);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(-1.0f);
    ImGui::InputTextWithHint("##filtro", "Filtrar (p.ej. Assimp, DevIL, textura...)", consoleFilter, sizeof(consoleFilter));
    ImGui::Separator();

    // Mensajes que pasan los filtros
    const auto& entries = GetLogEntries();
    std::string filter(consoleFilter);
    std::transform(filter.begin(), filter.end(), filter.begin(), [](unsigned char c) { return (char)std::tolower(c); });
    std::vector<int> visible;
    visible.reserve(entries.size());
    for (int i = 0; i < (int)entries.size(); ++i) {
        const LogEntry& e = entries[i];
        if (e.level == LogLevel::Info && !consoleShowInfo) continue;
        if (e.level == LogLevel::Warning && !consoleShowWarnings) continue;
        if (e.level == LogLevel::Error && !consoleShowErrors) continue;
        if (!filter.empty()) {
            std::string low(e.message);
            std::transform(low.begin(), low.end(), low.begin(), [](unsigned char c) { return (char)std::tolower(c); });
            if (low.find(filter) == std::string::npos) continue;
        }
        visible.push_back(i);
    }

    ImGui::BeginChild("ConsolaScroll", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);
    if (copy) ImGui::LogToClipboard();
    ImGuiListClipper clipper;
    clipper.Begin((int)visible.size());
    while (clipper.Step()) {
        for (int k = clipper.DisplayStart; k < clipper.DisplayEnd; ++k) {
            const LogEntry& e = entries[visible[k]];
            ImVec4 col = ImVec4(0.85f, 0.85f, 0.85f, 1.0f);
            const char* tag = "[Info] ";
            if (e.level == LogLevel::Warning) { col = ImVec4(1.0f, 0.8f, 0.3f, 1.0f); tag = "[Aviso] "; }
            if (e.level == LogLevel::Error)   { col = ImVec4(1.0f, 0.4f, 0.4f, 1.0f); tag = "[Error] "; }
            ImGui::PushStyleColor(ImGuiCol_Text, col);
            ImGui::TextUnformatted(tag);
            ImGui::SameLine(0.0f, 0.0f);
            ImGui::TextUnformatted(e.message.c_str());
            ImGui::PopStyleColor();
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", e.source.c_str());
        }
    }
    if (copy) ImGui::LogFinish();

    // Bajar automáticamente al último mensaje cuando llegan mensajes nuevos
    if (consoleAutoScroll && entries.size() != consoleLastCount) {
        ImGui::SetScrollHereY(1.0f);
    }
    consoleLastCount = entries.size();
    ImGui::EndChild();
    ImGui::End();
}

// ─────────────────────────────────────────────
//  Panel Configuración
// ─────────────────────────────────────────────

static std::string FormatBytes(uint64_t bytes) {
    char buf[64];
    if (bytes >= (1ull << 30))      snprintf(buf, sizeof(buf), "%.2f GB", bytes / (double)(1ull << 30));
    else if (bytes >= (1ull << 20)) snprintf(buf, sizeof(buf), "%.1f MB", bytes / (double)(1ull << 20));
    else                            snprintf(buf, sizeof(buf), "%.1f KB", bytes / 1024.0);
    return buf;
}

static bool HasGLExtension(const char* name) {
    static std::vector<std::string> extensions;
    if (extensions.empty()) {
        GLint count = 0;
        glGetIntegerv(GL_NUM_EXTENSIONS, &count);
        for (GLint i = 0; i < count; ++i) {
            const char* ext = (const char*)glGetStringi(GL_EXTENSIONS, (GLuint)i);
            if (ext) extensions.emplace_back(ext);
        }
        if (extensions.empty()) extensions.emplace_back("");   // para no volver a preguntar
    }
    for (const auto& e : extensions) if (e == name) return true;
    return false;
}

void Editor::DrawConfigPanel(Window& window) {
    ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(ImVec2(vp->Size.x * 0.5f, vp->Size.y * 0.5f), ImGuiCond_FirstUseEver, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(470.0f, 560.0f), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Configuracion", &showConfig)) {
        ImGui::End();
        return;
    }
    const ImVec4 accent(0.4f, 0.8f, 1.0f, 1.0f);
    const ImVec4 value(1.0f, 0.85f, 0.3f, 1.0f);

    // --- Aplicación y rendimiento ---
    if (ImGui::CollapsingHeader("Aplicacion", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Text("Motor:"); ImGui::SameLine(); ImGui::TextColored(value, "Motor-Videojoc");
        ImGui::Text("Equipo:"); ImGui::SameLine(); ImGui::TextColored(value, "Bole Wu, Yangfeng Xu");
        ImGui::SliderInt("Limite FPS", &renderer.fpsLimit, 0, 240, renderer.fpsLimit == 0 ? "Sin limite" : "%d");

        // Las gráficas empiezan por el dato más antiguo
        float fpsOrdered[FPS_HISTORY], msOrdered[FPS_HISTORY];
        for (int i = 0; i < FPS_HISTORY; ++i) {
            fpsOrdered[i] = fpsHistory[(fpsHistoryIndex + i) % FPS_HISTORY];
            msOrdered[i] = msHistory[(fpsHistoryIndex + i) % FPS_HISTORY];
        }
        const float currentFps = fpsOrdered[FPS_HISTORY - 1];
        const float currentMs = msOrdered[FPS_HISTORY - 1];
        char overlay[64];
        snprintf(overlay, sizeof(overlay), "Framerate %.1f", currentFps);
        ImGui::PlotHistogram("##fps", fpsOrdered, FPS_HISTORY, 0, overlay, 0.0f, 160.0f, ImVec2(-1.0f, 80.0f));
        snprintf(overlay, sizeof(overlay), "Milisegundos %.2f", currentMs);
        ImGui::PlotHistogram("##ms", msOrdered, FPS_HISTORY, 0, overlay, 0.0f, 40.0f, ImVec2(-1.0f, 80.0f));
    }

    // --- Renderer ---
    if (ImGui::CollapsingHeader("Renderer")) {
        if (ImGui::Checkbox("VSync", &renderer.vsync)) {
            if (!SDL_GL_SetSwapInterval(renderer.vsync ? 1 : 0)) LOG("Error al cambiar VSync: %s", SDL_GetError());
        }
        ImGui::Checkbox("Depth test (Z-Buffer)", &renderer.depthTest);
        ImGui::Checkbox("Cull face (no dibujar caras traseras)", &renderer.cullFace);
        ImGui::Checkbox("Usar shaders (GLSL)", &renderer.useShaders);
        if (!renderer.useShaders) ImGui::Checkbox("Iluminacion (pipeline fijo)", &renderer.fixedLighting);
        ImGui::Checkbox("Wireframe global", &renderer.globalWireframe);
        ImGui::Checkbox("Mostrar texturas", &renderer.globalShowTextures);
        ImGui::Checkbox("Mostrar rejilla", &renderer.showGrid);
        ImGui::ColorEdit3("Color de fondo", glm::value_ptr(renderer.clearColor));
    }

    // --- Ventana ---
    if (ImGui::CollapsingHeader("Ventana")) {
        SDL_Window* win = window.GetWindow();
        const SDL_WindowFlags flags = SDL_GetWindowFlags(win);
        if (ImGui::InputText("Titulo", windowTitle, sizeof(windowTitle))) SDL_SetWindowTitle(win, windowTitle);

        bool fullscreen = (flags & SDL_WINDOW_FULLSCREEN) != 0;
        bool resizable = (flags & SDL_WINDOW_RESIZABLE) != 0;
        bool borderless = (flags & SDL_WINDOW_BORDERLESS) != 0;
        if (ImGui::Checkbox("Pantalla completa", &fullscreen)) SDL_SetWindowFullscreen(win, fullscreen);
        ImGui::SameLine();
        if (ImGui::Checkbox("Redimensionable", &resizable)) SDL_SetWindowResizable(win, resizable);
        ImGui::SameLine();
        if (ImGui::Checkbox("Sin bordes", &borderless)) SDL_SetWindowBordered(win, !borderless);

        int w = 0, h = 0;
        SDL_GetWindowSize(win, &w, &h);
        static int newW = 0, newH = 0;
        if (newW == 0) { newW = w; newH = h; }
        ImGui::SliderInt("Ancho", &newW, 640, 3840);
        ImGui::SliderInt("Alto", &newH, 360, 2160);
        if (ImGui::Button("Aplicar tamano")) SDL_SetWindowSize(win, newW, newH);
        ImGui::SameLine();
        ImGui::Text("Actual: %dx%d", w, h);

        const SDL_DisplayID display = SDL_GetDisplayForWindow(win);
        if (const SDL_DisplayMode* mode = SDL_GetCurrentDisplayMode(display)) {
            ImGui::Text("Monitor:"); ImGui::SameLine();
            ImGui::TextColored(value, "%dx%d a %.0f Hz", mode->w, mode->h, mode->refresh_rate);
        }
    }

    // --- Input ---
    if (ImGui::CollapsingHeader("Input")) {
        ImGuiIO& io = ImGui::GetIO();
        ImGui::Text("Posicion raton:"); ImGui::SameLine(); ImGui::TextColored(value, "%.0f, %.0f", io.MousePos.x, io.MousePos.y);
        ImGui::Text("Movimiento:");     ImGui::SameLine(); ImGui::TextColored(value, "%.0f, %.0f", io.MouseDelta.x, io.MouseDelta.y);
        ImGui::Text("Rueda:");          ImGui::SameLine(); ImGui::TextColored(value, "%.1f", io.MouseWheel);
        ImGui::Text("Botones:");        ImGui::SameLine();
        ImGui::TextColored(value, "%s%s%s", io.MouseDown[0] ? "Izq " : "", io.MouseDown[2] ? "Central " : "",
                           io.MouseDown[1] ? "Der" : "");
        std::string keys;
        for (int k = ImGuiKey_NamedKey_BEGIN; k < ImGuiKey_NamedKey_END; ++k) {
            if (k >= ImGuiKey_MouseLeft && k <= ImGuiKey_MouseWheelY) continue;
            if (ImGui::IsKeyDown((ImGuiKey)k)) { keys += ImGui::GetKeyName((ImGuiKey)k); keys += " "; }
        }
        ImGui::Text("Teclas pulsadas:"); ImGui::SameLine(); ImGui::TextColored(value, "%s", keys.empty() ? "-" : keys.c_str());
        ImGui::Separator();
        ImGui::TextColored(accent, "Camara");
        ImGui::SliderFloat("Velocidad (WASD)", &renderer.cameraMoveSpeed, 0.5f, 20.0f, "%.1f u/s");
        ImGui::SliderFloat("Sensibilidad mirar", &renderer.cameraLookSensitivity, 0.05f, 1.0f, "%.2f");
        ImGui::SliderFloat("Sensibilidad orbitar", &renderer.cameraOrbitSensitivity, 0.05f, 1.0f, "%.2f");
    }

    // --- Texturas ---
    if (ImGui::CollapsingHeader("Texturas")) {
        // Texturas distintas que hay en la escena (de los materiales y las asignadas a mano)
        std::set<const Texture*> textures;
        std::function<void(const std::shared_ptr<GameObject>&)> collect = [&](const std::shared_ptr<GameObject>& o) {
            if (o->texture->overrideTexture && o->texture->overrideTexture->IsLoaded()) textures.insert(o->texture->overrideTexture.get());
            if (o->mesh->geometry) for (const auto& mt : o->mesh->geometry->GetMaterials())
                if (mt.texture && mt.texture->IsLoaded()) textures.insert(mt.texture.get());
            for (const auto& c : o->children) collect(c);
        };
        for (const auto& o : renderer.objects) collect(o);

        ImGui::Text("DevIL version:"); ImGui::SameLine(); ImGui::TextColored(value, "%d", (int)ilGetInteger(IL_VERSION_NUM));
        ImGui::Text("Texturas en escena:"); ImGui::SameLine(); ImGui::TextColored(value, "%zu", textures.size());
        size_t bytes = 0;
        for (const Texture* t : textures) bytes += (size_t)t->GetWidth() * t->GetHeight() * 4;
        ImGui::Text("Memoria aprox. (RGBA):"); ImGui::SameLine(); ImGui::TextColored(value, "%s", FormatBytes(bytes).c_str());
        for (const Texture* t : textures) {
            ImGui::Image((ImTextureID)(intptr_t)t->GetID(), ImVec2(32, 32), ImVec2(0, 1), ImVec2(1, 0));
            ImGui::SameLine();
            ImGui::Text("%dx%d  %s", t->GetWidth(), t->GetHeight(), t->GetPath().c_str());
        }
        if (renderer.checkerTexture) {
            ImGui::Image((ImTextureID)(intptr_t)renderer.checkerTexture->GetID(), ImVec2(32, 32));
            ImGui::SameLine();
            ImGui::Text("%dx%d  Textura de comprobacion (checker)", renderer.checkerTexture->GetWidth(), renderer.checkerTexture->GetHeight());
        }
    }

    // --- Hardware, memoria y versiones ---
    if (ImGui::CollapsingHeader("Hardware y versiones", ImGuiTreeNodeFlags_DefaultOpen)) {
        const int sdlv = SDL_GetVersion();
        ImGui::Text("SDL:");    ImGui::SameLine(); ImGui::TextColored(value, "%d.%d.%d", SDL_VERSIONNUM_MAJOR(sdlv), SDL_VERSIONNUM_MINOR(sdlv), SDL_VERSIONNUM_MICRO(sdlv));
        ImGui::Text("OpenGL:"); ImGui::SameLine(); ImGui::TextColored(value, "%s", (const char*)glGetString(GL_VERSION));
        ImGui::Text("GLSL:");   ImGui::SameLine(); ImGui::TextColored(value, "%s", (const char*)glGetString(GL_SHADING_LANGUAGE_VERSION));
        ImGui::Text("DevIL:");  ImGui::SameLine(); ImGui::TextColored(value, "%d", (int)ilGetInteger(IL_VERSION_NUM));
        ImGui::Text("Assimp:"); ImGui::SameLine(); ImGui::TextColored(value, "%u.%u.%u", aiGetVersionMajor(), aiGetVersionMinor(), aiGetVersionPatch());
        ImGui::Text("ImGui:");  ImGui::SameLine(); ImGui::TextColored(value, "%s", IMGUI_VERSION);
        ImGui::Text("glm:");    ImGui::SameLine(); ImGui::TextColored(value, "%d.%d.%d", GLM_VERSION_MAJOR, GLM_VERSION_MINOR, GLM_VERSION_PATCH);
        ImGui::Separator();

        ImGui::Text("CPUs:"); ImGui::SameLine();
        ImGui::TextColored(value, "%d (Cache: %d bytes)", SDL_GetNumLogicalCPUCores(), SDL_GetCPUCacheLineSize());
        ImGui::Text("RAM del sistema:"); ImGui::SameLine();
        ImGui::TextColored(value, "%.1f GB", SDL_GetSystemRAM() / 1024.0f);
        std::string caps;
        if (SDL_HasSSE())     caps += "SSE ";
        if (SDL_HasSSE2())    caps += "SSE2 ";
        if (SDL_HasSSE3())    caps += "SSE3 ";
        if (SDL_HasSSE41())   caps += "SSE41 ";
        if (SDL_HasSSE42())   caps += "SSE42 ";
        if (SDL_HasAVX())     caps += "AVX ";
        if (SDL_HasAVX2())    caps += "AVX2 ";
        if (SDL_HasAVX512F()) caps += "AVX512F ";
        if (SDL_HasNEON())    caps += "NEON ";
        ImGui::Text("Caps:"); ImGui::SameLine(); ImGui::TextColored(value, "%s", caps.c_str());
        ImGui::Separator();

        ImGui::Text("GPU:");    ImGui::SameLine(); ImGui::TextColored(value, "%s", (const char*)glGetString(GL_RENDERER));
        ImGui::Text("Marca:");  ImGui::SameLine(); ImGui::TextColored(value, "%s", (const char*)glGetString(GL_VENDOR));
        if (HasGLExtension("GL_NVX_gpu_memory_info")) {
            GLint totalKB = 0, availKB = 0;
            glGetIntegerv(0x9048, &totalKB);   // GL_GPU_MEMORY_INFO_TOTAL_AVAILABLE_MEMORY_NVX
            glGetIntegerv(0x9049, &availKB);   // GL_GPU_MEMORY_INFO_CURRENT_AVAILABLE_VIDMEM_NVX
            ImGui::Text("VRAM total:");      ImGui::SameLine(); ImGui::TextColored(value, "%s", FormatBytes((uint64_t)totalKB * 1024).c_str());
            ImGui::Text("VRAM usada:");      ImGui::SameLine(); ImGui::TextColored(value, "%s", FormatBytes((uint64_t)(totalKB - availKB) * 1024).c_str());
            ImGui::Text("VRAM disponible:"); ImGui::SameLine(); ImGui::TextColored(value, "%s", FormatBytes((uint64_t)availKB * 1024).c_str());
        } else if (HasGLExtension("GL_ATI_meminfo")) {
            GLint info[4] = { 0, 0, 0, 0 };
            glGetIntegerv(0x87FC, info);       // GL_TEXTURE_FREE_MEMORY_ATI
            ImGui::Text("VRAM libre (texturas):"); ImGui::SameLine(); ImGui::TextColored(value, "%s", FormatBytes((uint64_t)info[0] * 1024).c_str());
        } else {
            ImGui::TextDisabled("VRAM: el driver no permite consultarla (solo NVIDIA / AMD).");
        }
        ImGui::Separator();

        const uint64_t used = SystemInfo::GetProcessMemoryUsed();
        if (used > 0) {
            ImGui::Text("Memoria del motor:"); ImGui::SameLine(); ImGui::TextColored(value, "%s", FormatBytes(used).c_str());
            ImGui::Text("Pico de memoria:");   ImGui::SameLine(); ImGui::TextColored(value, "%s", FormatBytes(SystemInfo::GetProcessMemoryPeak()).c_str());
        }
    }
    ImGui::End();
}

// ─────────────────────────────────────────────
//  Ventana About
// ─────────────────────────────────────────────

void Editor::DrawAboutWindow() {
    ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(ImVec2(vp->Size.x * 0.5f, vp->Size.y * 0.5f), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(460.0f, 0.0f), ImGuiCond_Appearing);
    if (!ImGui::Begin("About", &showAbout, ImGuiWindowFlags_NoCollapse)) {
        ImGui::End();
        return;
    }
    const ImVec4 accent(0.4f, 0.8f, 1.0f, 1.0f);
    ImGui::TextColored(accent, "Motor-Videojoc");
    ImGui::TextWrapped("Motor de videojuegos minimalista en C++ hecho desde cero: carga de modelos FBX/OBJ, "
                       "texturas, jerarquia de GameObjects, shaders GLSL y editor con ImGui.");
    ImGui::Spacing();

    ImGui::TextColored(accent, "Equipo");
    ImGui::BulletText("Bole Wu");
    ImGui::BulletText("Yangfeng Xu");
    if (ImGui::Button("Repositorio en GitHub")) SDL_OpenURL("https://github.com/Bole05/Motor-Videojoc");
    ImGui::Spacing();

    ImGui::TextColored(accent, "Librerias");
    const int sdlv = SDL_GetVersion();
    ImGui::BulletText("SDL %d.%d.%d (ventana, input)", SDL_VERSIONNUM_MAJOR(sdlv), SDL_VERSIONNUM_MINOR(sdlv), SDL_VERSIONNUM_MICRO(sdlv));
    ImGui::BulletText("OpenGL %s (render) + glad", (const char*)glGetString(GL_VERSION));
    ImGui::BulletText("Dear ImGui %s (interfaz)", IMGUI_VERSION);
    ImGui::BulletText("Assimp %u.%u.%u (modelos 3D)", aiGetVersionMajor(), aiGetVersionMinor(), aiGetVersionPatch());
    ImGui::BulletText("DevIL %d (texturas)", (int)ilGetInteger(IL_VERSION_NUM));
    ImGui::BulletText("glm %d.%d.%d (matematicas)", GLM_VERSION_MAJOR, GLM_VERSION_MINOR, GLM_VERSION_PATCH);
    ImGui::BulletText("fmt (formato de texto)");
    ImGui::Spacing();

    ImGui::TextColored(accent, "Licencia");
    ImGui::Text("MIT License - Copyright (c) 2026 Yangfeng Xu and Bole Wu");
    if (ImGui::CollapsingHeader("Ver texto completo de la licencia")) {
        ImGui::TextWrapped(
            "Permission is hereby granted, free of charge, to any person obtaining a copy of this software and "
            "associated documentation files (the \"Software\"), to deal in the Software without restriction, including "
            "without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell "
            "copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the "
            "following conditions:\n\n"
            "The above copyright notice and this permission notice shall be included in all copies or substantial "
            "portions of the Software.\n\n"
            "THE SOFTWARE IS PROVIDED \"AS IS\", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT "
            "LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO "
            "EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER "
            "IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE "
            "USE OR OTHER DEALINGS IN THE SOFTWARE.");
    }
    ImGui::End();
}

// ─────────────────────────────────────────────
//  Menú principal
// ─────────────────────────────────────────────

void Editor::DrawMainMenuBar(Window& window) {
    if (!ImGui::BeginMainMenuBar()) return;

    // File: cargar, limpiar y SALIR del programa
    if (ImGui::BeginMenu("File")) {
        if (ImGui::MenuItem("Cargar Modelo (FBX/OBJ)...")) {
            showLoadModal = true;
        }
        if (ImGui::MenuItem("Limpiar Escena")) {
            renderer.ClearScene();
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Salir", "Alt+F4")) {
            SDL_Event quitEvent;
            SDL_zero(quitEvent);
            quitEvent.type = SDL_EVENT_QUIT;
            SDL_PushEvent(&quitEvent);
        }
        ImGui::EndMenu();
    }

    // GameObject: acciones sobre los objetos de la escena
        if (ImGui::BeginMenu("GameObject")) {
            bool hasSelection = (renderer.selectedObject != nullptr);
            if (ImGui::MenuItem("Crear GameObject Vacio")) {
                renderer.CreateEmptyObject(nullptr);
            }
            if (ImGui::MenuItem("Crear GameObject Vacio Hijo", nullptr, false, hasSelection)) {
                renderer.CreateEmptyObject(renderer.selectedObject.get());
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Duplicar", "Ctrl+D", false, hasSelection)) {
                renderer.DuplicateSelected();
            }
            if (ImGui::MenuItem("Eliminar", "Supr", false, hasSelection)) {
                renderer.DeleteSelected();
            }
            if (ImGui::MenuItem("Resetear Transform", nullptr, false, hasSelection)) {
                if (auto sel = renderer.GetSelectedObject()) {
                    sel->transform->position = glm::vec3(0.0f);
                    sel->transform->rotation = glm::vec3(0.0f);
                    sel->transform->scale = glm::vec3(1.0f);
                }
            }
            if (ImGui::MenuItem("Deseleccionar", nullptr, false, hasSelection)) {
                renderer.selectedObject.reset();
            }
            ImGui::EndMenu();
        }


    // Geometrías básicas: crear primitivas sin cargar ficheros
    if (ImGui::BeginMenu("Geometrias Basicas")) {
        const PrimitiveType types[] = { PrimitiveType::Cube, PrimitiveType::Sphere, PrimitiveType::Cylinder,
                                        PrimitiveType::Cone, PrimitiveType::Pyramid, PrimitiveType::Plane };
        for (PrimitiveType t : types) {
            if (ImGui::MenuItem(Geometria::PrimitiveName(t))) {
                renderer.AddPrimitive(t);
            }
        }
        ImGui::EndMenu();
    }

    // View: mostrar / ocultar las ventanas del editor y opciones de visualización
    if (ImGui::BeginMenu("View")) {
        ImGui::TextDisabled("Ventanas");
        ImGui::MenuItem("Escena (vista 3D)", nullptr, &renderer.showScene);
        ImGui::MenuItem("Jerarquia", nullptr, &showHierarchy);
        ImGui::MenuItem("Inspector", nullptr, &showInspector);
        ImGui::MenuItem("Consola", nullptr, &showConsole);
        ImGui::MenuItem("Configuracion", nullptr, &showConfig);
        ImGui::MenuItem("Editor de Shaders", nullptr, &showShaderEditor);
        ImGui::Separator();
        if (ImGui::BeginMenu("Opciones de render")) {
            ImGui::MenuItem("Mostrar Rejilla 3D (Grid)", nullptr, &renderer.showGrid);
            ImGui::MenuItem("Modo Malla Alambrica Global", nullptr, &renderer.globalWireframe);
            ImGui::MenuItem("Mostrar Texturas", nullptr, &renderer.globalShowTextures);
            ImGui::MenuItem("Usar Shaders (GLSL)", nullptr, &renderer.useShaders, renderer.shader.IsValid());
            ImGui::Separator();
            ImGui::MenuItem("Resaltar Seleccion", nullptr, &renderer.showSelectionHighlight);
            ImGui::MenuItem("Resaltado con pulso", nullptr, &renderer.selectionPulse);
            ImGui::ColorEdit3("Color seleccion", glm::value_ptr(renderer.selectionColor), ImGuiColorEditFlags_NoInputs);
            ImGui::EndMenu();
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Enfocar Seleccion", "F", false, renderer.selectedObject != nullptr)) {
            renderer.FocusSelected();
        }
        if (ImGui::MenuItem("Resetear Posicion Camara")) {
            renderer.ResetCamera();
        }
        ImGui::EndMenu();
    }

        if (ImGui::BeginMenu("Debug")) {
            ImGui::MenuItem("Modo Debug", "F1", &renderer.debugMode);
            ImGui::Separator();
            if (!renderer.debugMode) ImGui::BeginDisabled();
            ImGui::MenuItem("AABB (verde)", nullptr, &renderer.debugShowAABB);
            ImGui::MenuItem("OBB orientada (amarillo)", nullptr, &renderer.debugShowOBB);
            ImGui::MenuItem("Caja de grupo / hijos (azul)", nullptr, &renderer.debugShowGroupAABB);
            ImGui::Separator();
            ImGui::MenuItem("Normales de vertice (cian)", nullptr, &renderer.debugShowVertexNormals);
            ImGui::MenuItem("Normales de cara (magenta)", nullptr, &renderer.debugShowFaceNormals);
            ImGui::SetNextItemWidth(120.0f);
            ImGui::SliderFloat("Longitud normales", &renderer.debugNormalLength, 0.005f, 0.5f, "%.3f",
                               ImGuiSliderFlags_Logarithmic);
            ImGui::Separator();
            ImGui::MenuItem("Solo objeto seleccionado", nullptr, &renderer.debugOnlySelected);
            ImGui::MenuItem("Ver a traves de los objetos", nullptr, &renderer.debugThroughObjects);
            ImGui::SetNextItemWidth(120.0f);
            ImGui::SliderFloat("Grosor linea", &renderer.debugLineWidth, 1.0f, 5.0f, "%.1f");
            if (!renderer.debugMode) ImGui::EndDisabled();
            ImGui::EndMenu();
        }


    // Help: documentación, bugs, descargas y About
    if (ImGui::BeginMenu("Help")) {
        const std::string repo = "https://github.com/Bole05/Motor-Videojoc";
        if (ImGui::MenuItem("Documentacion (GitHub /docs)"))  SDL_OpenURL((repo + "/tree/main/docs").c_str());
        if (ImGui::MenuItem("Reportar un bug (/issues)"))     SDL_OpenURL((repo + "/issues").c_str());
        if (ImGui::MenuItem("Descargar ultima version (/releases)")) SDL_OpenURL((repo + "/releases").c_str());
        ImGui::Separator();
        ImGui::MenuItem("About...", nullptr, &showAbout);
        ImGui::EndMenu();
    }

    (void)window;
    ImGui::EndMainMenuBar();
}

// ─────────────────────────────────────────────
//  DrawUI: Interfaz estilo aplicación 3D / modelado
// ─────────────────────────────────────────────

void Editor::DrawUI(Window& window) {
    // Las zonas para soltar texturas se vuelven a calcular en este frame
    textureDropZones.clear();

    // F1 = activar / desactivar el Modo Debug (si no se está escribiendo en un campo de texto)
    if (!ImGui::GetIO().WantTextInput && ImGui::IsKeyPressed(ImGuiKey_F1, false)) {
        renderer.debugMode = !renderer.debugMode;
        LOG("Modo Debug %s", renderer.debugMode ? "activado" : "desactivado");
    }

    // 1. Barra de Menú Principal Superior (File / GameObject / Geometrias Basicas / View / Debug / Help)
    DrawMainMenuBar(window);

    // 2. Panel Izquierdo: Jerarquía de Escena
    if (showHierarchy) DrawHierarchyPanel();

    // 3. Panel Derecho: Inspector del GameObject seleccionado
    if (showInspector) DrawInspectorPanel(window);

    // Consola, Configuración y About (menú View / Help)
    if (showConsole) DrawConsolePanel();
    if (showConfig)  DrawConfigPanel(window);
    if (showAbout)   DrawAboutWindow();

    // Etiqueta en pantalla con la pieza seleccionada (arriba, en el centro)
    if (renderer.selectedObject) {
        std::string path = renderer.selectedObject->name;
        for (const GameObject* p = renderer.selectedObject->parent; p; p = p->parent) {
            path = p->name + "  >  " + path;
        }
        ImGui::SetNextWindowPos(ImVec2((float)window.GetWidth() * 0.5f, 34.0f), ImGuiCond_Always, ImVec2(0.5f, 0.0f));
        ImGui::SetNextWindowBgAlpha(0.65f);
        ImGuiWindowFlags labelFlags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize |
                                      ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing |
                                      ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoInputs;
        if (ImGui::Begin("EtiquetaSeleccion", nullptr, labelFlags)) {
            const glm::vec3& c = renderer.selectionColor;
            ImGui::TextColored(ImVec4(c.r, c.g, c.b, 1.0f), "Seleccionado:");
            ImGui::SameLine();
            ImGui::TextUnformatted(path.c_str());
        }
        ImGui::End();
    }

    // Leyenda del Modo Debug (arriba a la izquierda de la vista 3D, junto a la Jerarquía)
    if (renderer.debugMode) {
        ImGui::SetNextWindowPos(ImVec2(310.0f, 34.0f), ImGuiCond_Always, ImVec2(0.0f, 0.0f));
        ImGui::SetNextWindowBgAlpha(0.6f);
        ImGuiWindowFlags legendFlags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize |
                                       ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing |
                                       ImGuiWindowFlags_NoNav;
        if (ImGui::Begin("LeyendaDebug", nullptr, legendFlags)) {
            ImGui::Text("MODO DEBUG (F1)");
            ImGui::Separator();
            ImGui::Checkbox("##aabb", &renderer.debugShowAABB);  ImGui::SameLine();
            ImGui::TextColored(ImVec4(0.1f, 0.8f, 0.25f, 1.0f), "AABB");
            ImGui::Checkbox("##obb", &renderer.debugShowOBB);    ImGui::SameLine();
            ImGui::TextColored(ImVec4(0.9f, 0.75f, 0.1f, 1.0f), "OBB (orientada)");
            ImGui::Checkbox("##grp", &renderer.debugShowGroupAABB); ImGui::SameLine();
            ImGui::TextColored(ImVec4(0.2f, 0.55f, 0.95f, 1.0f), "Grupo (padre + hijos)");
            ImGui::Checkbox("##vn", &renderer.debugShowVertexNormals); ImGui::SameLine();
            ImGui::TextColored(ImVec4(0.2f, 0.9f, 0.95f, 1.0f), "Normales de vertice");
            ImGui::Checkbox("##fn", &renderer.debugShowFaceNormals); ImGui::SameLine();
            ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.8f, 1.0f), "Normales de cara");
            ImGui::Checkbox("Solo seleccionado", &renderer.debugOnlySelected);
        }
        ImGui::End();
    }

    // 4. Barra Inferior de Información y Ayuda
    ImGui::SetNextWindowPos(ImVec2(10.0f, (float)window.GetHeight() - 40.0f), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2((float)window.GetWidth() - 20.0f, 30.0f), ImGuiCond_Always);
    ImGuiWindowFlags statusFlags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoBackground;
    if (ImGui::Begin("BarraEstado", nullptr, statusFlags)) {
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f),
            "Camara: Clic Der + WASD = Mover (SHIFT x2) | Clic Der + Raton = Mirar | Rueda = Zoom | ALT + Clic Izq = Orbitar | F = Enfocar | Clic Izq = Seleccionar pieza (CTRL = modelo entero) | F1 = Debug");
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
                renderer.AddModel(loadPathBuffer);
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
//  renderer.CleanUp: liberar GPU e ImGui
// ─────────────────────────────────────────────


// ─────────────────────────────────────────────
//  Panel Jerarquía
// ─────────────────────────────────────────────

void Editor::DrawHierarchyPanel() {
    ImGui::SetNextWindowPos(ImVec2(10.0f, 30.0f), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(290.0f, 360.0f), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Jerarquia de Escena", &showHierarchy)) {
        ImGui::Text("Modelos en Escena: %zu", renderer.objects.size());
        ImGui::Separator();

        // Botones de acción rápida
        if (ImGui::Button("+ Cargar FBX/OBJ", ImVec2(130, 26))) {
            showLoadModal = true;
        }
        ImGui::SameLine();
        if (ImGui::Button("+ Crear Vacio", ImVec2(134, 26))) {
            renderer.CreateEmptyObject(nullptr);
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Crea un GameObject vacio en la raiz.\n"
                              "Arrastra modelos encima para agruparlos dentro.");
        }
        bool hasSelection = (renderer.selectedObject != nullptr);
        if (!hasSelection) ImGui::BeginDisabled();
        if (ImGui::Button("Duplicar", ImVec2(65, 26))) {
            renderer.DuplicateSelected();
        }
        ImGui::SameLine();
        if (ImGui::Button("Eliminar", ImVec2(65, 26))) {
            renderer.DeleteSelected();
        }
        if (!hasSelection) ImGui::EndDisabled();

        ImGui::Separator();

        // Árbol de objetos: cada FBX con sus nodos (p.ej. BakerHouse.fbx > Baker_house, Chimney)
        ImGui::BeginChild("ObjetosList", ImVec2(0, 0), true);
        if (renderer.objects.empty()) {
            ImGui::TextDisabled("No hay modelos cargados.");
            ImGui::TextWrapped("Arrastra un archivo .FBX o .OBJ a esta ventana para agregarlo.");
        } else {
            for (const auto& obj : renderer.objects) {
                DrawHierarchyNode(obj);
            }
        }

        // Espacio libre bajo el árbol: soltar aquí un objeto lo saca a la raíz (al final)
        {
            ImVec2 avail = ImGui::GetContentRegionAvail();
            ImGui::InvisibleButton("##RaizEscena", ImVec2((std::max)(avail.x, 1.0f), (std::max)(avail.y, 24.0f)));
            if (ImGui::BeginDragDropTarget()) {
                if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("HIERARCHY_NODE")) {
                    GameObject* dragged = *(GameObject* const*)payload->Data;
                    pendingReparent = { dragged, nullptr, -1, true };
                }
                ImGui::EndDragDropTarget();
            }
            // Clic izquierdo en el vacío = deseleccionar
            if (ImGui::IsItemClicked(ImGuiMouseButton_Left)) {
                renderer.selectedObject.reset();
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
            renderer.ReparentObject(pendingReparent.obj, pendingReparent.newParent, pendingReparent.index);
            pendingReparent = PendingReparent{};
        }
        if (pendingAction == PendingAction::DUPLICATE) renderer.DuplicateSelected();
        else if (pendingAction == PendingAction::DELETE_OBJ) renderer.DeleteSelected();
        else if (pendingAction == PendingAction::CREATE_EMPTY) renderer.CreateEmptyObject(nullptr);
        else if (pendingAction == PendingAction::CREATE_EMPTY_CHILD) renderer.CreateEmptyObject(renderer.selectedObject.get());
        pendingAction = PendingAction::NONE;
    }
    ImGui::End();
}

// ─────────────────────────────────────────────
//  Panel Inspector: un apartado por cada componente del GameObject seleccionado
// ─────────────────────────────────────────────

void Editor::DrawInspectorPanel(Window& window) {
    ImGui::SetNextWindowPos(ImVec2((float)window.GetWidth() - 360.0f, 30.0f), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(350.0f, 580.0f), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Inspector", &showInspector)) {
        ImGui::End();
        return;
    }

    const std::shared_ptr<GameObject> sel = renderer.GetSelectedObject();
    if (!sel) {
        ImGui::TextDisabled("Ningun GameObject seleccionado.");
        ImGui::Spacing();
        ImGui::TextWrapped("Selecciona un objeto en la 'Jerarquia de Escena' o haz clic sobre el en la vista 3D.");
        ImGui::Spacing();
        ImGui::TextWrapped("Tambien puedes arrastrar y soltar cualquier fichero .FBX o .OBJ en la ventana.");
        ImGui::End();
        return;
    }

    const ImVec4 accent(0.4f, 0.8f, 1.0f, 1.0f);

    // --- GameObject: nombre, visibilidad, jerarquía y lista de componentes ---
    char nameBuf[128];
    snprintf(nameBuf, sizeof(nameBuf), "%s", sel->name.c_str());
    ImGui::Checkbox("##visible", &sel->visible);
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Visible en escena (tambien oculta a sus hijos)");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(-1.0f);
    if (ImGui::InputText("##nombre", nameBuf, sizeof(nameBuf))) {
        sel->name = nameBuf;
    }
    ImGui::Text("Padre: %s", sel->parent ? sel->parent->name.c_str() : "(ninguno, raiz de escena)");
    ImGui::Text("Hijos: %zu   Componentes: %zu", sel->children.size(), sel->GetComponents().size());
    ImGui::Separator();

    // --- Componente Transform ---
    if (ComponentTransform* transform = sel->GetComponent<ComponentTransform>()) {
        if (ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::DragFloat3("Posicion", glm::value_ptr(transform->position), 0.05f, 0.0f, 0.0f, "%.2f");
            ImGui::DragFloat3("Rotacion", glm::value_ptr(transform->rotation), 1.0f, -360.0f, 360.0f, "%.1f deg");
            ImGui::DragFloat3("Escala", glm::value_ptr(transform->scale), 0.02f, 0.001f, 1000.0f, "%.3f");

            // Botones rápidos de giro
            if (ImGui::Button("+45 Y"))  transform->rotation.y += 45.0f;
            ImGui::SameLine();
            if (ImGui::Button("+90 Y"))  transform->rotation.y += 90.0f;
            ImGui::SameLine();
            if (ImGui::Button("+180 Y")) transform->rotation.y += 180.0f;
            ImGui::SameLine();
            if (ImGui::Button("Reset")) transform->Reset();

            ImGui::Checkbox("Giro continuo automatico", &transform->autoRotate);
            if (transform->autoRotate) {
                ImGui::SliderFloat("Velocidad giro", &transform->autoRotateSpeed, -180.0f, 180.0f, "%.1f deg/s");
            }
        }
    }

    // --- Componente Mesh ---
    if (ComponentMesh* mesh = sel->GetComponent<ComponentMesh>()) {
        if (ImGui::CollapsingHeader("Mesh", ImGuiTreeNodeFlags_DefaultOpen)) {
            if (!mesh->geometry) {
                ImGui::TextDisabled("Sin malla (GameObject vacio).");
                ImGui::TextWrapped("Sirve de contenedor: arrastra objetos encima suyo en la Jerarquia para "
                                   "agruparlos. Al moverlo, girarlo o escalarlo, sus hijos le siguen.");
            } else {
                const Geometria& geom = *mesh->geometry;
                ImGui::TextWrapped("Fichero: %s", sel->filePath.c_str());
                ImGui::Text("Submallas en este nodo: %zu", mesh->meshIndices.size());
                ImGui::Text("Vertices: %u   Triangulos: %u", mesh->GetVertexCount(), mesh->GetTriangleCount());
                ImGui::Text("Total del fichero: %u vertices, %u triangulos", geom.GetTotalVertices(), geom.GetTotalTriangles());
                ImGui::Text("UVs: %s", geom.HasUVs() ? "si" : "no");

                // De dónde vienen las normales de las submallas de este nodo
                unsigned int fromFile = 0, generated = 0, none = 0;
                for (unsigned int idx : mesh->meshIndices) {
                    if (!geom.MeshHasNormals(idx)) ++none;
                    else if (geom.MeshNormalsFromFile(idx)) ++fromFile;
                    else ++generated;
                }
                if (fromFile > 0)
                    ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.5f, 1.0f), "Normales: importadas del fichero (%u submallas)", fromFile);
                if (generated > 0 && geom.IsPrimitive())
                    ImGui::TextColored(accent, "Normales: calculadas por el motor (geometria basica)");
                else if (generated > 0)
                    ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.3f, 1.0f), "Normales: generadas por Assimp (%u submallas)", generated);
                if (none > 0)
                    ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "Sin normales: %u submallas", none);

                // Visualizar normales (por vértice y por cara / triángulo)
                if (ImGui::Checkbox("Ver normales de vertice", &renderer.debugShowVertexNormals) && renderer.debugShowVertexNormals)
                    renderer.debugMode = true;
                ImGui::SameLine();
                if (ImGui::Checkbox("De cara", &renderer.debugShowFaceNormals) && renderer.debugShowFaceNormals)
                    renderer.debugMode = true;
                if (renderer.debugShowVertexNormals || renderer.debugShowFaceNormals) {
                    ImGui::SliderFloat("Longitud", &renderer.debugNormalLength, 0.005f, 0.5f, "%.3f",
                                       ImGuiSliderFlags_Logarithmic);
                }
            }
            ImGui::ColorEdit3("Color difuso", glm::value_ptr(mesh->color));
            ImGui::Checkbox("Malla alambrica (wireframe)", &mesh->wireframe);
        }
    }

    // --- Componente Textura ---
    if (ComponentTexture* texture = sel->GetComponent<ComponentTexture>()) {
        if (ImGui::CollapsingHeader("Textura", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::Checkbox("Mostrar textura", &texture->showTexture);
            ImGui::Checkbox("Textura de comprobacion (checker)", &texture->useCheckerTexture);
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("Cuadros blancos y negros: sirve para comprobar que las UV\n"
                                  "no estan estiradas ni deformadas. Tambien se aplica a los hijos.");
            if (!renderer.globalShowTextures) {
                ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.2f, 1.0f), "(texturas desactivadas en View)");
            }

            // Textura asignada a mano (arrastrando una imagen fuera de las miniaturas)
            if (texture->HasOverride()) {
                ImGui::TextWrapped("Manual: %s", texture->overrideTexture->GetPath().c_str());
                ImGui::Text("Tamano: %dx%d", texture->overrideTexture->GetWidth(), texture->overrideTexture->GetHeight());
                ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.2f, 1.0f), "(tapa las texturas de los materiales)");
                if (ImGui::Button("Quitar textura manual")) {
                    texture->overrideTexture.reset();
                }
            }

            const ComponentMesh* mesh = sel->GetComponent<ComponentMesh>();
            if (mesh && mesh->geometry) {
                if (!mesh->geometry->HasUVs()) {
                    ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.3f, 1.0f), "El modelo no tiene coordenadas UV.");
                }

                // Texturas de los materiales del FBX.
                // Cada miniatura es una zona donde se puede soltar una imagen para SUSTITUIRLA.
                // Si el nodo tiene mallas propias, sólo se muestran los materiales que usan.
                const auto& mats = mesh->geometry->GetMaterials();
                std::set<unsigned int> usedMats;
                for (unsigned int idx : mesh->meshIndices) usedMats.insert(mesh->geometry->GetMeshMaterial(idx));

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
                    const ImVec2 rmin = ImGui::GetItemRectMin();
                    const ImVec2 rmax = ImGui::GetItemRectMax();
                    textureDropZones.push_back({ glm::vec2(rmin.x, rmin.y), glm::vec2(rmax.x, rmax.y),
                                                 mesh->geometry, (unsigned int)m });

                    // Resaltar la miniatura mientras se arrastra un fichero por encima
                    const bool hovered = isDraggingFile &&
                        dragPos.x >= rmin.x && dragPos.x <= rmax.x && dragPos.y >= rmin.y && dragPos.y <= rmax.y;
                    ImGui::GetWindowDrawList()->AddRect(rmin, rmax,
                        hovered ? IM_COL32(80, 220, 120, 255) : IM_COL32(110, 110, 130, 255),
                        4.0f, 0, hovered ? 3.0f : 1.0f);

                    // Información del material: ruta y tamaño de la textura
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
                            mesh->geometry->RestoreMaterialTexture((unsigned int)m);
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
            }
        }
    }

    // --- Caja envolvente en coordenadas de mundo ---
    if (ImGui::CollapsingHeader("Bounding Box (mundo)")) {
        ImGui::Checkbox("Modo Debug (F1)", &renderer.debugMode);
        glm::vec3 mn, mx;
        if (renderer.ComputeWorldAABB(sel.get(), false, mn, mx)) {
            const glm::vec3 size = mx - mn;
            ImGui::TextColored(ImVec4(0.3f, 0.9f, 0.4f, 1.0f), "AABB de su malla:");
            ImGui::Text("  Min:    (%.2f, %.2f, %.2f)", mn.x, mn.y, mn.z);
            ImGui::Text("  Max:    (%.2f, %.2f, %.2f)", mx.x, mx.y, mx.z);
            ImGui::Text("  Tamano: (%.2f, %.2f, %.2f)", size.x, size.y, size.z);
        } else {
            ImGui::TextDisabled("Este nodo no tiene malla propia.");
        }
        if (!sel->children.empty() && renderer.ComputeWorldAABB(sel.get(), true, mn, mx)) {
            const glm::vec3 size = mx - mn;
            const glm::vec3 center = (mn + mx) * 0.5f;
            ImGui::TextColored(ImVec4(0.4f, 0.7f, 1.0f, 1.0f), "AABB con sus hijos:");
            ImGui::Text("  Centro: (%.2f, %.2f, %.2f)", center.x, center.y, center.z);
            ImGui::Text("  Tamano: (%.2f, %.2f, %.2f)", size.x, size.y, size.z);
        }
    }

    // --- Acciones ---
    ImGui::Spacing();
    if (ImGui::Button("Duplicar", ImVec2(110, 28))) {
        renderer.DuplicateSelected();
    }
    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.7f, 0.2f, 0.2f, 1.0f));
    if (ImGui::Button("Eliminar", ImVec2(110, 28))) {
        renderer.DeleteSelected();
    }
    ImGui::PopStyleColor();

    ImGui::End();
}
