#pragma once

#include <glad/glad.h>
#include <glm/glm.hpp>
#include "Window.h"
#include "Texture.h"
#include "Log.h"
#include "Geometria.h"

class Engine {
public:
    Engine();
    ~Engine();

    // Inicializa subsistemas: Ventana, Glad (OpenGL), Viewport, Recursos (Textura y Cubo)
    bool Init();

    // Bucle principal del motor (ProcessInput -> Update -> Render)
    void Run();

    // Libera memoria de la GPU (buffers, textura) y cierra la ventana
    void CleanUp();

    // Métodos del ciclo de vida del frame
    void ProcessInput();
    void Update();
    void Render();

    // Getters
    Window& GetWindow() { return window; }
    const Window& GetWindow() const { return window; }
    bool IsRunning() const { return isRunning; }

private:
    void InitGeometry();

    Window window;
    Texture texture;
    bool isRunning = false;

    // Buffers de OpenGL para el cubo: 0: Posiciones, 1: Colores, 2: Coordenadas UV
    GLuint buffs[3] = {0, 0, 0};

    // Ángulo de rotación del objeto 3D
    float angle = 0.0f;

    Geometria geometria;
};
