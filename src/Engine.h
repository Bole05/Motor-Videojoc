#pragma once

#include <SDL3/SDL.h>
#include "Window.h"
#include "Renderer.h"
#include "Editor.h"
#include "Log.h"

// Engine: gestiona el ciclo de vida del motor (bucle principal, ventana, eventos).
//  - Renderer (motor): escena de GameObjects, cámara y render 3D con OpenGL.
//  - Editor   (editor): toda la interfaz con ImGui, encima de la escena.
class Engine {
public:
    Engine();
    ~Engine();

    // Inicializa ventana, contexto OpenGL (GLAD), el Renderer y el Editor
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
    void ProcessInput();  // Gestiona eventos SDL (cierre, resize, drag & drop, ratón, teclado)
    void Update();        // Avanza la lógica del frame con deltaTime
    void Render();        // Escena 3D (Renderer) + interfaz (Editor) y swap de buffers

    Window   window;
    Renderer renderer;
    Editor   editor{ renderer };   // se declara después de renderer: lo usa desde el constructor
    bool isRunning = false;
    Uint64 lastTime = 0;
};
