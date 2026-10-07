#include "Engine.h"
#include <glad/glad.h>
#include <fmt/core.h>

// ─────────────────────────────────────────────
//  Constructor / Destructor
// ─────────────────────────────────────────────

Engine::Engine() : isRunning(false) {}

Engine::~Engine() {
    CleanUp();
}

// ─────────────────────────────────────────────
//  Init: ventana → GLAD → estado OpenGL → Renderer
// ─────────────────────────────────────────────

bool Engine::Init() {
    // 1. Crear ventana y contexto OpenGL
    if (!window.Init("Engine", 1280, 720, true, false)) {
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
    glClearColor(0.1f, 0.1f, 0.15f, 1.0f);
    glEnable(GL_DEPTH_TEST);

    // 4. Inicializar el Renderer (necesita contexto OpenGL activo)
    return renderer.Init();
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
//  ProcessInput: eventos SDL
// ─────────────────────────────────────────────

void Engine::ProcessInput() {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {

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
        else if (event.type == SDL_EVENT_DROP_FILE) {
            // En SDL3, event.drop.data es un puntero interno → copiar inmediatamente
            std::string path = event.drop.data;
            LOG("Fichero arrastrado: %s", path.c_str());
            renderer.LoadModel(path);
        }
    }
}

// ─────────────────────────────────────────────
//  Update / Render: delegan al Renderer
// ─────────────────────────────────────────────

void Engine::Update() {
    renderer.Update();
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
