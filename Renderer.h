#pragma once

#include <glad/glad.h>
#include <string>
#include "Window.h"
#include "Texture.h"
#include "Geometria.h"

// Renderer: responsable exclusivo de toda la lógica OpenGL.
// Engine le llama cada frame; Renderer no sabe nada del bucle principal.
class Renderer {
public:
    Renderer();
    ~Renderer();

    // Configura la luz, carga la textura y crea los VBOs del cubo.
    // Debe llamarse después de que el contexto OpenGL esté activo.
    bool Init();

    // Avanza la animación del cubo (ángulo de rotación).
    void Update();

    // Dibuja la escena completa e intercambia los búferes.
    void Render(Window& window);

    // Carga un modelo FBX/OBJ desde disco (reemplaza el anterior).
    void LoadModel(const std::string& path);

    // Libera todos los recursos de la GPU.
    void CleanUp();

private:
    // --- Métodos internos de render ---
    void InitCube();                  // Crea los VBOs del cubo con textura
    void RenderCube(Window& window);  // Dibuja el cubo giratorio con textura
    void RenderModel();               // Dibuja el modelo Assimp con iluminación

    // --- Recursos ---
    Texture   texture;    // Textura del cubo (Lenna)
    Geometria geometria;  // Malla 3D cargada por Assimp

    // VBOs del cubo: 0 = posiciones, 1 = colores, 2 = UVs
    GLuint buffs[3] = {0, 0, 0};

    // Ángulo de rotación del cubo (radianes, incrementa cada frame)
    float angle = 0.0f;
};
