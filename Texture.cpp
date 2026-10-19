#include "Texture.h"
#include <IL/il.h>
#include <SDL3/SDL.h>
#include <fmt/core.h>
#include <vector>

Texture::Texture()
    : id(0), width(0), height(0) {
}

Texture::~Texture() {
    CleanUp();
}

void Texture::InitDevIL() {
    static bool devilInitialized = false;
    if (!devilInitialized) {
        ilInit();
        // Configuración de DevIL para que el origen (0,0) esté abajo a la izquierda,
        // igual que el sistema de coordenadas UV de OpenGL. Esto evita que la imagen salga invertida.
        ilEnable(IL_ORIGIN_SET);
        ilOriginFunc(IL_ORIGIN_LOWER_LEFT);
        devilInitialized = true;
        fmt::print("DevIL inicializado correctamente con origen en LOWER_LEFT.\n");
    }
}

bool Texture::Load(const std::string& filePath) {
    // Si ya teníamos una textura asignada, liberamos la anterior
    CleanUp();

    // Nos aseguramos de que DevIL esté inicializado
    InitDevIL();

    // 1. Generar e identificar una imagen en la memoria RAM con DevIL
    ILuint imgName;
    ilGenImages(1, &imgName);
    ilBindImage(imgName);

    // 2. Sistema de búsqueda de rutas dinámico
    // Preguntamos a SDL la ruta del ejecutable para encontrar la carpeta 'assets' sin importar desde dónde se lance
    const char* sdlBasePath = SDL_GetBasePath();
    std::string basePath = sdlBasePath ? sdlBasePath : "";

    std::vector<std::string> candidatePaths = {
        filePath,
        basePath + filePath,
        "../" + filePath,
        "../../" + filePath
    };

    bool loaded = false;
    std::string successPath;
    for (const auto& p : candidatePaths) {
        if (ilLoadImage(p.c_str())) {
            loaded = true;
            successPath = p;
            break;
        }
    }

    if (!loaded) {
        ILenum error = ilGetError();
        fmt::print("Error al cargar la textura '{}' con DevIL. Código: 0x{:X} ({})\n", filePath, error, error);
        ilDeleteImages(1, &imgName);
        return false;
    }

    fmt::print("Textura cargada en RAM desde: {}\n", successPath);

    bool ok = UploadBoundILImage();
    ilDeleteImages(1, &imgName);
    if (ok) path = successPath;
    return ok;
}

bool Texture::LoadFromMemory(const void* data, unsigned int size, const std::string& debugName) {
    CleanUp();
    InitDevIL();

    ILuint imgName;
    ilGenImages(1, &imgName);
    ilBindImage(imgName);

    // IL_TYPE_UNKNOWN: DevIL detecta el formato (png, jpg, tga, dds...) por la cabecera
    if (!ilLoadL(IL_TYPE_UNKNOWN, data, size)) {
        ILenum error = ilGetError();
        fmt::print("Error al decodificar textura embebida '{}'. Código: 0x{:X}\n", debugName, error);
        ilDeleteImages(1, &imgName);
        return false;
    }

    bool ok = UploadBoundILImage();
    ilDeleteImages(1, &imgName);
    if (ok) path = debugName;
    return ok;
}

bool Texture::LoadFromBGRA(const void* pixels, int w, int h, const std::string& debugName) {
    CleanUp();
    if (!pixels || w <= 0 || h <= 0) return false;

    width = w;
    height = h;
    glGenTextures(1, &id);
    glBindTexture(GL_TEXTURE_2D, id);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_BGRA, GL_UNSIGNED_BYTE, pixels);
    glGenerateMipmap(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, 0);

    path = debugName;
    return true;
}

bool Texture::UploadBoundILImage() {
    // 3. Convertir la imagen a formato estándar RGBA (8 bits por canal)
    if (!ilConvertImage(IL_RGBA, IL_UNSIGNED_BYTE)) {
        fmt::print("Error al convertir la imagen a RGBA\n");
        return false;
    }
    width = ilGetInteger(IL_IMAGE_WIDTH);
    height = ilGetInteger(IL_IMAGE_HEIGHT);
    fmt::print("Dimensiones decodificadas: {}x{}\n", width, height);

    // 4. Crear el recurso en la GPU (OpenGL)
    glGenTextures(1, &id);
    glBindTexture(GL_TEXTURE_2D, id);

    // Parámetros de texturas (Wrapping y Filtering)
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);

    // Subir los píxeles a la VRAM de la GPU
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, ilGetData());

    // 5. Generar mipmaps (resoluciones menores para cuando la cámara esté lejos)
    glGenerateMipmap(GL_TEXTURE_2D);

    // Desenlazar la textura para evitar modificaciones accidentales
    glBindTexture(GL_TEXTURE_2D, 0);

    fmt::print("Textura subida con éxito a la GPU (OpenGL Texture ID: {})\n", id);
    return true;
}

void Texture::Bind() const {
    if (id != 0) {
        glBindTexture(GL_TEXTURE_2D, id);
    }
}

void Texture::Unbind() const {
    glBindTexture(GL_TEXTURE_2D, 0);
}

void Texture::CleanUp() {
    if (id != 0) {
        glDeleteTextures(1, &id);
        id = 0;
        width = 0;
        height = 0;
    }
    path.clear();
}
