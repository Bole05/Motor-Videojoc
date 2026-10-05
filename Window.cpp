#include "Window.h"
#include <fmt/core.h>

Window::Window()
    : window(nullptr), gl_context(nullptr), width(0), height(0) {
}

Window::~Window() {
    CleanUp();
}

bool Window::Init(const char* title, unsigned int w, unsigned int h, bool resizable, bool fullscreen) {
    width = w;
    height = h;

    // 1. Inicializar el subsistema de video de SDL3
    // SDL_Init enciende la librería SDL3 para la parte de video
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        fmt::print("Error al inicializar SDL3: {}\n", SDL_GetError());
        return false;
    }
    fmt::print("SDL3 inicializado correctamente.\n");

    // 2. Configuración de atributos de OpenGL antes de crear la ventana
    // Usamos el perfil Compatibility
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_COMPATIBILITY);

    // Activar Double Buffer para evitar parpadeos preparando el siguiente frame
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

    // Reservar un búfer de profundidad (Z-Buffer) de 24 bits
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);

    // Reservar búfer de plantilla (stencil) de 8 bits
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);

    // Versión de OpenGL requerida (4.6)
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 6);

    // Flags para la ventana
    Uint32 flags = SDL_WINDOW_OPENGL;
    if (fullscreen) {
        flags |= SDL_WINDOW_FULLSCREEN;
    }
    if (resizable) {
        flags |= SDL_WINDOW_RESIZABLE;
    }

    // 3. Crear la ventana
    window = SDL_CreateWindow(title, width, height, flags);
    if (!window) {
        fmt::print("Error al crear la ventana: {}\n", SDL_GetError());
        CleanUp();
        return false;
    }
    fmt::print("Ventana creada con éxito ({}x{}).\n", width, height);

    // 4. Crear el contexto de OpenGL asociado a la ventana
    // El contexto es el espacio de trabajo que conecta la tarjeta gráfica con nuestra ventana
    gl_context = SDL_GL_CreateContext(window);
    if (!gl_context) {
        fmt::print("Error al crear el contexto OpenGL: {}\n", SDL_GetError());
        CleanUp();
        return false;
    }
    fmt::print("Contexto OpenGL creado con éxito.\n");

    return true;
}

void Window::CleanUp() {
    if (gl_context) {
        SDL_GL_DestroyContext(gl_context);
        gl_context = nullptr;
    }

    if (window) {
        SDL_DestroyWindow(window);
        window = nullptr;
    }

    SDL_Quit();
}

void Window::SwapBuffers() {
    if (window) {
        SDL_GL_SwapWindow(window);
    }
}

void Window::SetSize(unsigned int w, unsigned int h) {
    width = w;
    height = h;
}
