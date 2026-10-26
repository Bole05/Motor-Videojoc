#include "Engine.h"
#include <glad/glad.h>
#include <fmt/core.h>
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
        fmt::print("Error al cargar la librería GLAD\n");
        return false;
    }

    // Información de la GPU
    fmt::print("Vendor: {}\n",             (const char*)glGetString(GL_VENDOR));
    fmt::print("Renderer: {}\n",           (const char*)glGetString(GL_RENDERER));
    fmt::print("OpenGL version: {}\n",     (const char*)glGetString(GL_VERSION));
    fmt::print("GLSL: {}\n",               (const char*)glGetString(GL_SHADING_LANGUAGE_VERSION));

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
        ProcessInput();
        Update();
        Render();
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
            fmt::print("Cerrando el motor...\n");
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
                if (event.button.button == SDL_BUTTON_RIGHT || event.button.button == SDL_BUTTON_MIDDLE) {
                    renderer.SetMouseDragging(true);
                }
            }
        }
        else if (event.type == SDL_EVENT_MOUSE_BUTTON_UP) {
            if (event.button.button == SDL_BUTTON_RIGHT || event.button.button == SDL_BUTTON_MIDDLE) {
                renderer.SetMouseDragging(false);
            }
        }
        else if (event.type == SDL_EVENT_MOUSE_MOTION) {
            if (renderer.IsMouseDragging()) {
                renderer.OnMouseMoved(event.motion.xrel, event.motion.yrel);
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
    fmt::print("Motor apagado correctamente.\n");
}
