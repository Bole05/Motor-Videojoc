#include "Engine.h"
#include <SDL3/SDL.h>
#include <fmt/core.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

// Cabeceras de Assimp listas para futuros módulos de carga de modelos 3D
#include <assimp/cimport.h>
#include <Assimp/scene.h>
#include <Assimp/postprocess.h>

Engine::Engine()
    : isRunning(false), angle(0.0f) {
    buffs[0] = buffs[1] = buffs[2] = 0;
}

Engine::~Engine() {
    CleanUp();
}

bool Engine::Init() {
    // 1. Inicializar la ventana y el contexto OpenGL
    unsigned int initialWidth = 1280;
    unsigned int initialHeight = 720;
    if (!window.Init("Engine", initialWidth, initialHeight, true, false)) {
        return false;
    }
    // Arrasgar fichero
    SDL_SetEventEnabled(SDL_EVENT_DROP_FILE, true);

    // 2. Inicializar Glad (para cargar los punteros de función de OpenGL desde el driver)
    // reinterpret_cast<GLADloadproc>(...): convierte el puntero de SDL al formato esperado por Glad
    if (gladLoadGLLoader(reinterpret_cast<GLADloadproc>(SDL_GL_GetProcAddress)) == 0) {
        fmt::print("Error al cargar la librería GLAD\n");
        return false;
    }

    // Imprimir la información del driver de la GPU instalada
    fmt::print("Vendor: {}\n", (const char*)glGetString(GL_VENDOR));
    fmt::print("Renderer: {}\n", (const char*)glGetString(GL_RENDERER));
    fmt::print("OpenGL version supported: {}\n", (const char*)glGetString(GL_VERSION));
    fmt::print("GLSL: {}\n", (const char*)glGetString(GL_SHADING_LANGUAGE_VERSION));

    // 3. Configuración inicial del Viewport y OpenGL
    // glViewport: define el área de renderizado en píxeles (origen en 0,0 esquina inferior izquierda)
    glViewport(0, 0, window.GetWidth(), window.GetHeight());

    // Color del fondo RGBA
    glClearColor(0.1f, 0.1f, 0.15f, 1.0f);

    // Activar Z-Buffer (test de profundidad para que los objetos cercanos tapen a los lejanos)
    glEnable(GL_DEPTH_TEST);

    // 4. Cargar la textura mediante el módulo Texture
    if (!texture.Load("assets/Lenna.dds")) {
        texture.Load("assets/Lenna.png");
    }

    // 5. Crear la geometría (VBOs para el cubo)
    InitGeometry();

    return true;
}

void Engine::InitGeometry() {
    // Definición de vértices de las 6 caras del Cubo
    float v[] = {
        // Cara frontal
        -0.5f, -0.5f,  0.5f,   0.5f, -0.5f,  0.5f,   0.5f,  0.5f,  0.5f,
        -0.5f, -0.5f,  0.5f,   0.5f,  0.5f,  0.5f,  -0.5f,  0.5f,  0.5f,
        // Cara trasera
        -0.5f, -0.5f, -0.5f,  -0.5f,  0.5f, -0.5f,   0.5f,  0.5f, -0.5f,
        -0.5f, -0.5f, -0.5f,   0.5f,  0.5f, -0.5f,   0.5f, -0.5f, -0.5f,
        // Cara izquierda
        -0.5f,  0.5f,  0.5f,  -0.5f,  0.5f, -0.5f,  -0.5f, -0.5f, -0.5f,
        -0.5f,  0.5f,  0.5f,  -0.5f, -0.5f, -0.5f,  -0.5f, -0.5f,  0.5f,
        // Cara derecha
         0.5f,  0.5f,  0.5f,   0.5f, -0.5f, -0.5f,   0.5f,  0.5f, -0.5f,
         0.5f, -0.5f, -0.5f,   0.5f,  0.5f,  0.5f,   0.5f, -0.5f,  0.5f,
        // Cara superior
        -0.5f,  0.5f, -0.5f,  -0.5f,  0.5f,  0.5f,   0.5f,  0.5f,  0.5f,
        -0.5f,  0.5f, -0.5f,   0.5f,  0.5f,  0.5f,   0.5f,  0.5f, -0.5f,
        // Cara inferior
        -0.5f, -0.5f, -0.5f,   0.5f, -0.5f, -0.5f,   0.5f, -0.5f,  0.5f,
        -0.5f, -0.5f, -0.5f,   0.5f, -0.5f,  0.5f,  -0.5f, -0.5f,  0.5f
    };

    // Colores para cada vértice (un color distinto por cara)
    float c[36 * 3];
    float colorsPerFace[6][3] = {
        {1.0f, 0.0f, 0.0f}, // Rojo
        {0.0f, 1.0f, 0.0f}, // Verde
        {0.0f, 0.0f, 1.0f}, // Azul
        {1.0f, 1.0f, 0.0f}, // Amarillo
        {1.0f, 0.0f, 1.0f}, // Magenta
        {0.0f, 1.0f, 1.0f}  // Cian
    };

    for (int face = 0; face < 6; face++) {
        for (int vertex = 0; vertex < 6; vertex++) {
            int index = (face * 6 + vertex) * 3;
            c[index + 0] = colorsPerFace[face][0];
            c[index + 1] = colorsPerFace[face][1];
            c[index + 2] = colorsPerFace[face][2];
        }
    }

    // Coordenadas de textura UV del cubo
    float uvs[] = {
        // Cara frontal (XY)
        0.0f, 0.0f,  1.0f, 0.0f,  1.0f, 1.0f,
        0.0f, 0.0f,  1.0f, 1.0f,  0.0f, 1.0f,
        // Cara trasera (-XY)
        1.0f, 0.0f,  1.0f, 1.0f,  0.0f, 1.0f,
        1.0f, 0.0f,  0.0f, 1.0f,  0.0f, 0.0f,
        // Cara izquierda (ZY)
        1.0f, 1.0f,  1.0f, 0.0f,  0.0f, 0.0f,
        1.0f, 1.0f,  0.0f, 0.0f,  0.0f, 1.0f,
        // Cara derecha (-ZY)
        0.0f, 1.0f,  1.0f, 0.0f,  0.0f, 0.0f,
        1.0f, 0.0f,  0.0f, 1.0f,  1.0f, 1.0f,
        // Cara superior (XZ)
        0.0f, 1.0f,  0.0f, 0.0f,  1.0f, 0.0f,
        0.0f, 1.0f,  1.0f, 0.0f,  1.0f, 1.0f,
        // Cara inferior (-XZ)
        0.0f, 1.0f,  1.0f, 1.0f,  1.0f, 0.0f,
        0.0f, 1.0f,  1.0f, 0.0f,  0.0f, 0.0f
    };

    // Crear y rellenar los 3 buffers en la GPU: VBO 0 (posiciones), VBO 1 (colores), VBO 2 (UVs)
    glGenBuffers(3, buffs);

    glBindBuffer(GL_ARRAY_BUFFER, buffs[0]);
    glBufferData(GL_ARRAY_BUFFER, sizeof(v), v, GL_STATIC_DRAW);

    glBindBuffer(GL_ARRAY_BUFFER, buffs[1]);
    glBufferData(GL_ARRAY_BUFFER, sizeof(c), c, GL_STATIC_DRAW);

    glBindBuffer(GL_ARRAY_BUFFER, buffs[2]);
    glBufferData(GL_ARRAY_BUFFER, sizeof(uvs), uvs, GL_STATIC_DRAW);
}

void Engine::Run() {
    isRunning = true;

    // Bucle Principal: se repite cada frame hasta que isRunning sea false
    while (isRunning) {
        ProcessInput();
        Update();
        Render();
    }
}

void Engine::ProcessInput() {
    SDL_Event event;
    // Procesar todos los eventos de la ventana (teclado, ratón, cerrar, redimensionar)
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_EVENT_QUIT) {
            isRunning = false;
            fmt::print("Cerrando el motor...\n");
        }
        else if (event.type == SDL_EVENT_WINDOW_RESIZED) {
            unsigned int newWidth = event.window.data1;
            unsigned int newHeight = event.window.data2;
            window.SetSize(newWidth, newHeight);

            // Ajustar el área de renderizado al nuevo tamaño de ventana
            glViewport(0, 0, newWidth, newHeight);
        }
        else if (event.type == SDL_EVENT_DROP_FILE) {
            std::string path = event.drop.data;  // ruta del fichero arrastrado
            LOG("Fichero arrastrado: %s", path.c_str());
            geometria.Load(path);                // cargar el FBX
            SDL_free((void*)event.drop.data);           // liberar memoria de SDL
        }
    }
}

void Engine::Update() {
    // Incrementar el ángulo de rotación para la animación del cubo
    angle += 0.01f;
}

void Engine::Render() {
    // Limpiar buffers de color y de profundidad
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // 1. Matriz de Proyección Perspectiva (FoV: 45 grados, aspect ratio, plano cercano: 0.1, lejano: 100.0)
    float aspect = (float)window.GetWidth() / (float)window.GetHeight();
    glm::mat4 projection = glm::perspective(glm::radians(45.0f), aspect, 0.1f, 100.0f);
    glMatrixMode(GL_PROJECTION);
    glLoadMatrixf(glm::value_ptr(projection));

    // 2. Matriz de Vista / Modelo (Modelview)
    glm::mat4 modelview = glm::mat4(1.0f);
    modelview = glm::translate(modelview, glm::vec3(0.0f, 0.0f, -3.5f)); // Alejar el cubo en Z
    modelview = glm::rotate(modelview, angle, glm::vec3(0.5f, 1.0f, 0.2f)); // Rotación 3D en X, Y, Z
    glMatrixMode(GL_MODELVIEW);
    glLoadMatrixf(glm::value_ptr(modelview));

    // 3. Renderizar el cubo con VBOs y Textura
    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_TEXTURE_COORD_ARRAY);

    // Enlazar buffer de vértices (posiciones)
    glBindBuffer(GL_ARRAY_BUFFER, buffs[0]);
    glVertexPointer(3, GL_FLOAT, 0, NULL);

    // Enlazar buffer de coordenadas UV
    glBindBuffer(GL_ARRAY_BUFFER, buffs[2]);
    glTexCoordPointer(2, GL_FLOAT, 0, NULL);
    glColor3f(1.0f, 1.0f, 1.0f);

    // Activar y enlazar textura
    glEnable(GL_TEXTURE_2D);
    texture.Bind();
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);

    // Dibujar 36 vértices (12 triángulos = 6 caras)
    glDrawArrays(GL_TRIANGLES, 0, 36);

    // Desactivar estados después de su uso
    glDisableClientState(GL_TEXTURE_COORD_ARRAY);
    glDisableClientState(GL_VERTEX_ARRAY);

    // 4. Intercambiar búferes de la ventana (Double Buffering)
    window.SwapBuffers();
}

void Engine::CleanUp() {
    // Liberar los buffers de la GPU
    if (buffs[0] != 0) {
        glDeleteBuffers(3, buffs);
        buffs[0] = buffs[1] = buffs[2] = 0;
    }

    // Liberar recursos de textura y ventana
    texture.CleanUp();
    window.CleanUp();
    fmt::print("Motor apagado correctamente.\n");
}
