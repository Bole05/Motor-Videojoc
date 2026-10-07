#pragma once

#include <SDL3/SDL.h>
#include "Window.h"
#include "Renderer.h"
#include "Log.h"

// Engine: gestiona el ciclo de vida del motor (bucle principal, ventana, eventos).
// Toda la lógica de renderizado vive en Renderer.
class Engine {
public:
    Engine();
    ~Engine();

    // Inicializa ventana, contexto OpenGL (GLAD) y el Renderer
    bool Init();

    // Ejecuta el bucle principal hasta que el usuario cierre la ventana
    void Run();

    // Libera todos los recursos y cierra la ventana
    void CleanUp();

    // Getters
    Window& GetWindow() { return window; }
    const Window& GetWindow() const { return window; }
    bool IsRunning() const { return isRunning; }

private:
    void ProcessInput();  // Gestiona eventos SDL (cierre, resize, drag & drop)
    void Update();        // Avanza la lógica del frame
    void Render();        // Delega el render al Renderer

    Window   window;
    Renderer renderer;
    bool isRunning = false;
};
