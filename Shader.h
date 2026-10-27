#pragma once

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <string>
#include <unordered_map>

// Shader: un programa de GPU formado por un vertex shader y un fragment shader (GLSL).
//  - Compila y enlaza el código GLSL.
//  - Si hay errores los guarda (GetLastError) y los escribe en el Log.
//  - Si una recompilación falla, se sigue usando el programa anterior que funcionaba.
class Shader {
public:
    Shader() = default;
    ~Shader();

    // No copiable (posee un recurso de GPU)
    Shader(const Shader&) = delete;
    Shader& operator=(const Shader&) = delete;

    // Compila y enlaza a partir del código fuente
    bool Compile(const std::string& vertexSrc, const std::string& fragmentSrc);
    // Lee los ficheros .vert / .frag y los compila
    bool LoadFromFiles(const std::string& vertexPath, const std::string& fragmentPath);

    void Use() const;
    static void Unuse();
    void CleanUp();

    bool IsValid() const { return program != 0; }
    GLuint GetID() const { return program; }
    const std::string& GetLastError() const { return lastError; }
    const std::string& GetVertexSource() const { return vertexSource; }
    const std::string& GetFragmentSource() const { return fragmentSource; }

    // Uniforms (si el shader no usa ese uniform, no hace nada)
    GLint GetUniformLocation(const char* name);
    void SetInt(const char* name, int v);
    void SetBool(const char* name, bool v) { SetInt(name, v ? 1 : 0); }
    void SetFloat(const char* name, float v);
    void SetVec3(const char* name, const glm::vec3& v);
    void SetMat3(const char* name, const glm::mat3& m);
    void SetMat4(const char* name, const glm::mat4& m);

    // Código GLSL por defecto incluido en el ejecutable (por si no se encuentran los ficheros)
    static const char* DefaultVertexSource();
    static const char* DefaultFragmentSource();

    // Utilidad: lee un fichero de texto entero (false si no existe)
    static bool ReadTextFile(const std::string& path, std::string& out);

private:
    static GLuint CompileStage(GLenum type, const std::string& src, std::string& errorOut);

    GLuint program = 0;
    std::string vertexSource;
    std::string fragmentSource;
    std::string lastError;
    std::unordered_map<std::string, GLint> uniformCache;
};
