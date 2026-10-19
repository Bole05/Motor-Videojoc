#pragma once

#include <glad/glad.h>
#include <string>

class Texture {
public:
    Texture();
    ~Texture();

    // No copiable (posee un recurso de GPU)
    Texture(const Texture&) = delete;
    Texture& operator=(const Texture&) = delete;

    // Inicializa la configuración global de DevIL (ejes de coordenadas, etc.)
    static void InitDevIL();

    // Carga un archivo de imagen desde disco a RAM con DevIL y luego lo sube a la VRAM con OpenGL
    bool Load(const std::string& filePath);

    // Carga una imagen comprimida (png, jpg, dds...) que ya está en memoria
    // (por ejemplo, una textura embebida dentro de un FBX)
    bool LoadFromMemory(const void* data, unsigned int size, const std::string& debugName = "embedded");

    // Sube píxeles crudos BGRA de 8 bits (formato aiTexel de Assimp)
    bool LoadFromBGRA(const void* pixels, int w, int h, const std::string& debugName = "embedded");

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
    const std::string& GetPath() const { return path; }

private:
    // Sube la imagen DevIL actualmente enlazada a la GPU
    bool UploadBoundILImage();

    GLuint id = 0;       // Identificador de textura generado por OpenGL
    int width = 0;       // Ancho de la textura en píxeles
    int height = 0;      // Alto de la textura en píxeles
    std::string path;    // Ruta (o nombre) de origen, para mostrar en la UI
};
