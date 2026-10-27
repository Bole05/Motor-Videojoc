#pragma once

#include <SDL3/SDL.h>

class Window {
public:
    Window();
    ~Window();

    // Inicializa SDL3 (Video), configura atributos de OpenGL, crea la ventana y el contexto de OpenGL
    bool Init(const char* title, unsigned int width, unsigned int height, bool resizable = true, bool fullscreen = false);

    // Limpia y destruye el contexto OpenGL, la ventana y llama a SDL_Quit()
    void CleanUp();

    // Intercambia los búferes de la ventana (Double Buffering)
    void SwapBuffers();

    // Actualiza las dimensiones de la ventana
    void SetSize(unsigned int w, unsigned int h);

    // Getters
    SDL_Window* GetWindow() const { return window; }
    SDL_GLContext GetContext() const { return gl_context; }
    unsigned int GetWidth() const { return width; }
    unsigned int GetHeight() const { return height; }

private:
    SDL_Window* window = nullptr;
    SDL_GLContext gl_context = nullptr;
    unsigned int width = 0;
    unsigned int height = 0;
};
