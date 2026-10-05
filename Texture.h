#pragma once

#include <glad/glad.h>
#include <string>

class Texture {
public:
    Texture();
    ~Texture();

    // Inicializa la configuración global de DevIL (ejes de coordenadas, etc.)
    static void InitDevIL();

    // Carga un archivo de imagen desde disco a RAM con DevIL y luego lo sube a la VRAM con OpenGL
    bool Load(const std::string& filePath);

    // Activa la textura en OpenGL para renderizar
    void Bind() const;

    // Desactiva la textura en OpenGL
    void Unbind() const;

    // Libera la memoria de la textura en la GPU
    void CleanUp();

    // Getters
    GLuint GetID() const { return id; }
    int GetWidth() const { return width; }
    int GetHeight() const { return height; }
    bool IsLoaded() const { return id != 0; }

private:
    GLuint id = 0;       // Identificador de textura generado por OpenGL
    int width = 0;       // Ancho de la textura en píxeles
    int height = 0;      // Alto de la textura en píxeles
};
