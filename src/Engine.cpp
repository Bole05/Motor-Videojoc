#include "Engine.h"
#include <glad/glad.h>
#include <fmt/core.h>
#include "Log.h"
#include <imgui.h>
#include <imgui_impl_sdl3.h>

// ─────────────────────────────────────────────
//  Constructor / Destructor
// ─────────────────────────────────────────────

Engine::Engine() : isRunning(false), lastTime(0) {}

Engine::~Engine() {
    CleanUp();
}

// ─────────────────────────────────────────────
//  Init: ventana → GLAD → estado OpenGL → Renderer (con ImGui)
// ─────────────────────────────────────────────

bool Engine::Init() {
    // 1. Crear ventana y contexto OpenGL
    if (!window.Init("Motor 3D - Model Viewer & Editor", 1280, 720, true, false)) {
        return false;
    }

    // Habilitar eventos de drag & drop
    SDL_SetEventEnabled(SDL_EVENT_DROP_FILE, true);

    // 2. Inicializar GLAD (carga los punteros de función de OpenGL desde el driver)
    if (gladLoadGLLoader(reinterpret_cast<GLADloadproc>(SDL_GL_GetProcAddress)) == 0) {
        LOG("Error al cargar la libreria GLAD");
        return false;
    }

    // Información de la GPU
    LOG("GLAD cargado. Vendor: %s", (const char*)glGetString(GL_VENDOR));
    LOG("GPU: %s", (const char*)glGetString(GL_RENDERER));
    LOG("OpenGL version: %s", (const char*)glGetString(GL_VERSION));
    LOG("GLSL: %s", (const char*)glGetString(GL_SHADING_LANGUAGE_VERSION));

    // 3. Estado global de OpenGL
    glViewport(0, 0, window.GetWidth(), window.GetHeight());
    glEnable(GL_DEPTH_TEST);

    // 4. Inicializar el Renderer e ImGui (necesita contexto OpenGL y Window)
    if (!renderer.Init(window)) {
        return false;
    }

    lastTime = SDL_GetTicks();
    return true;
}

// ─────────────────────────────────────────────
//  Run: bucle principal
// ─────────────────────────────────────────────

void Engine::Run() {
    isRunning = true;
    while (isRunning) {
        const Uint64 frameStart = SDL_GetTicksNS();

        ProcessInput();
        Update();
        Render();

        // Límite de FPS (panel Configuración). 0 = sin límite.
        const int limit = renderer.GetFpsLimit();
        if (limit > 0) {
            const Uint64 target = 1000000000ull / (Uint64)limit;
            const Uint64 elapsed = SDL_GetTicksNS() - frameStart;
            if (elapsed < target) SDL_DelayNS(target - elapsed);
        }
    }
}

// ─────────────────────────────────────────────
//  ProcessInput: eventos SDL, ImGui, ratón y teclado
// ─────────────────────────────────────────────

void Engine::ProcessInput() {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        // Enviar eventos a Dear ImGui
        ImGui_ImplSDL3_ProcessEvent(&event);

        ImGuiIO& io = ImGui::GetIO();

        if (event.type == SDL_EVENT_QUIT) {
            isRunning = false;
            LOG("Cerrando el motor...");
        }
        else if (event.type == SDL_EVENT_WINDOW_RESIZED) {
            unsigned int w = event.window.data1;
            unsigned int h = event.window.data2;
            window.SetSize(w, h);
            glViewport(0, 0, w, h);
        }
        else if (event.type == SDL_EVENT_DROP_BEGIN) {
            renderer.OnFileDragBegin();
        }
        else if (event.type == SDL_EVENT_DROP_POSITION) {
            // Posición del ratón mientras se arrastra un fichero sobre la ventana
            renderer.OnFileDragMove(event.drop.x, event.drop.y);
        }
        else if (event.type == SDL_EVENT_DROP_COMPLETE) {
            renderer.OnFileDragEnd();
        }
        else if (event.type == SDL_EVENT_DROP_FILE) {
            std::string path = event.drop.data;
            LOG("Fichero arrastrado: %s (en %.0f, %.0f)", path.c_str(), event.drop.x, event.drop.y);
            // Se pasa también el punto donde se ha soltado (para sustituir texturas del Inspector)
            renderer.HandleDroppedFile(path, event.drop.x, event.drop.y);
        }
        else if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
            if (!io.WantCaptureMouse) {
                const SDL_Keymod mod = SDL_GetModState();
                if (event.button.button == SDL_BUTTON_RIGHT) {
                    // Clic derecho (mantener): mirar alrededor + moverse con WASD
                    renderer.BeginFreeLook();
                    SDL_SetWindowRelativeMouseMode(window.GetWindow(), true);   // ocultar y fijar el cursor
                }
                else if (event.button.button == SDL_BUTTON_MIDDLE) {
                    renderer.BeginPan();
                }
                else if (event.button.button == SDL_BUTTON_LEFT) {
                    if (mod & SDL_KMOD_ALT) {
                        // ALT + clic izquierdo (mantener) + arrastrar: orbitar alrededor del objeto seleccionado
                        renderer.BeginOrbit();
                    } else {
                        // Clic izquierdo = seleccionar la pieza bajo el ratón (con CTRL, el modelo entero)
                        const bool wholeModel = (mod & SDL_KMOD_CTRL) != 0;
                        renderer.PickAt(event.button.x, event.button.y,
                                        (float)window.GetWidth(), (float)window.GetHeight(), wholeModel);
                    }
                }
            }
        }
        else if (event.type == SDL_EVENT_MOUSE_BUTTON_UP) {
            if (event.button.button == SDL_BUTTON_RIGHT && renderer.IsFreeLookActive()) {
                renderer.EndFreeLook();
                SDL_SetWindowRelativeMouseMode(window.GetWindow(), false);
            }
            else if (event.button.button == SDL_BUTTON_LEFT) {
                renderer.EndOrbit();
            }
            else if (event.button.button == SDL_BUTTON_MIDDLE) {
                renderer.EndPan();
            }
        }
        else if (event.type == SDL_EVENT_MOUSE_MOTION) {
            if (renderer.IsFreeLookActive()) {
                renderer.FreeLook(event.motion.xrel, event.motion.yrel);
            }
            else if (renderer.IsOrbitActive()) {
                renderer.Orbit(event.motion.xrel, event.motion.yrel);
            }
            else if (renderer.IsPanActive()) {
                renderer.Pan(event.motion.xrel, event.motion.yrel);
            }
        }
        else if (event.type == SDL_EVENT_MOUSE_WHEEL) {
            if (!io.WantCaptureMouse) {
                renderer.OnMouseWheel(event.wheel.y);
            }
        }
        else if (event.type == SDL_EVENT_KEY_DOWN) {
            if (!io.WantTextInput) {
                // Atajos de modelado tipo Blender / Unity
                if (event.key.key == SDLK_DELETE) {
                    renderer.DeleteSelected();
                }
                else if (event.key.key == SDLK_D && (event.key.mod & SDL_KMOD_CTRL)) {
                    renderer.DuplicateSelected();
                }
                else if (event.key.key == SDLK_F && !(event.key.mod & (SDL_KMOD_CTRL | SDL_KMOD_ALT))) {
                    // Tecla F: enfocar la cámara en el objeto seleccionado
                    renderer.FocusSelected();
                }
            }
        }
    }
}

// ─────────────────────────────────────────────
//  Update / Render
// ─────────────────────────────────────────────

void Engine::Update() {
    Uint64 now = SDL_GetTicks();
    float deltaTime = (lastTime > 0) ? static_cast<float>(now - lastTime) / 1000.0f : 0.016f;
    lastTime = now;

    renderer.Update(deltaTime);
}

void Engine::Render() {
    renderer.Render(window);
}

// ─────────────────────────────────────────────
//  CleanUp: recursos y ventana
// ─────────────────────────────────────────────

void Engine::CleanUp() {
    renderer.CleanUp();
    window.CleanUp();
    LOG("Motor apagado correctamente.");
}
