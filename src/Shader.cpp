#include "Shader.h"
#include "Log.h"
#include <glm/gtc/type_ptr.hpp>
#include <fstream>
#include <sstream>

Shader::~Shader() {
    CleanUp();
}

void Shader::CleanUp() {
    if (program != 0) {
        glDeleteProgram(program);
        program = 0;
    }
    uniformCache.clear();
}

bool Shader::ReadTextFile(const std::string& path, std::string& out) {
    std::ifstream file(path, std::ios::in | std::ios::binary);
    if (!file) return false;
    std::stringstream ss;
    ss << file.rdbuf();
    out = ss.str();
    return true;
}

GLuint Shader::CompileStage(GLenum type, const std::string& src, std::string& errorOut) {
    GLuint id = glCreateShader(type);
    const char* code = src.c_str();
    glShaderSource(id, 1, &code, nullptr);
    glCompileShader(id);

    GLint ok = GL_FALSE;
    glGetShaderiv(id, GL_COMPILE_STATUS, &ok);
    if (ok != GL_TRUE) {
        GLint len = 0;
        glGetShaderiv(id, GL_INFO_LOG_LENGTH, &len);
        std::string log(len > 1 ? (size_t)len : 1, '\0');
        glGetShaderInfoLog(id, len, nullptr, log.data());
        errorOut += (type == GL_VERTEX_SHADER ? "[Vertex shader]\n" : "[Fragment shader]\n");
        errorOut += log.c_str();
        glDeleteShader(id);
        return 0;
    }
    return id;
}

bool Shader::Compile(const std::string& vertexSrc, const std::string& fragmentSrc) {
    // Guardamos el código aunque falle, para poder corregirlo en el editor
    vertexSource = vertexSrc;
    fragmentSource = fragmentSrc;
    lastError.clear();

    GLuint vs = CompileStage(GL_VERTEX_SHADER, vertexSrc, lastError);
    GLuint fs = CompileStage(GL_FRAGMENT_SHADER, fragmentSrc, lastError);
    if (!vs || !fs) {
        if (vs) glDeleteShader(vs);
        if (fs) glDeleteShader(fs);
        LOG("Error al compilar el shader:\n%s", lastError.c_str());
        return false;   // se mantiene el programa anterior (si había uno)
    }

    GLuint newProgram = glCreateProgram();
    glAttachShader(newProgram, vs);
    glAttachShader(newProgram, fs);
    glLinkProgram(newProgram);
    glDeleteShader(vs);   // ya están dentro del programa
    glDeleteShader(fs);

    GLint ok = GL_FALSE;
    glGetProgramiv(newProgram, GL_LINK_STATUS, &ok);
    if (ok != GL_TRUE) {
        GLint len = 0;
        glGetProgramiv(newProgram, GL_INFO_LOG_LENGTH, &len);
        std::string log(len > 1 ? (size_t)len : 1, '\0');
        glGetProgramInfoLog(newProgram, len, nullptr, log.data());
        lastError = "[Enlazado]\n" + std::string(log.c_str());
        glDeleteProgram(newProgram);
        LOG("Error al enlazar el shader:\n%s", lastError.c_str());
        return false;
    }

    // Todo bien: sustituir el programa anterior por el nuevo
    CleanUp();
    program = newProgram;
    LOG("Shader compilado y enlazado correctamente (programa %u).", program);
    return true;
}

bool Shader::LoadFromFiles(const std::string& vertexPath, const std::string& fragmentPath) {
    std::string vs, fs;
    if (!ReadTextFile(vertexPath, vs)) {
        lastError = "No se encuentra el fichero: " + vertexPath;
        return false;
    }
    if (!ReadTextFile(fragmentPath, fs)) {
        lastError = "No se encuentra el fichero: " + fragmentPath;
        return false;
    }
    LOG("Cargando shader: %s + %s", vertexPath.c_str(), fragmentPath.c_str());
    return Compile(vs, fs);
}

void Shader::Use() const {
    glUseProgram(program);
}

void Shader::Unuse() {
    glUseProgram(0);
}

GLint Shader::GetUniformLocation(const char* name) {
    if (!program) return -1;
    auto it = uniformCache.find(name);
    if (it != uniformCache.end()) return it->second;
    GLint loc = glGetUniformLocation(program, name);
    uniformCache[name] = loc;
    return loc;
}

void Shader::SetInt(const char* name, int v) {
    GLint loc = GetUniformLocation(name);
    if (loc >= 0) glUniform1i(loc, v);
}

void Shader::SetFloat(const char* name, float v) {
    GLint loc = GetUniformLocation(name);
    if (loc >= 0) glUniform1f(loc, v);
}

void Shader::SetVec3(const char* name, const glm::vec3& v) {
    GLint loc = GetUniformLocation(name);
    if (loc >= 0) glUniform3fv(loc, 1, glm::value_ptr(v));
}

void Shader::SetMat3(const char* name, const glm::mat3& m) {
    GLint loc = GetUniformLocation(name);
    if (loc >= 0) glUniformMatrix3fv(loc, 1, GL_FALSE, glm::value_ptr(m));
}

void Shader::SetMat4(const char* name, const glm::mat4& m) {
    GLint loc = GetUniformLocation(name);
    if (loc >= 0) glUniformMatrix4fv(loc, 1, GL_FALSE, glm::value_ptr(m));
}

// ─────────────────────────────────────────────
//  Código GLSL por defecto (copia de assets/shaders/default.vert y default.frag)
// ─────────────────────────────────────────────

const char* Shader::DefaultVertexSource() {
    return R"GLSL(#version 330 core
// ─────────────────────────────────────────────
//  VERTEX SHADER: se ejecuta una vez por cada VÉRTICE del modelo.
//  Coloca el vértice en pantalla y pasa al fragment shader los datos
//  que necesita (posición en el mundo, normal y coordenadas UV).
// ─────────────────────────────────────────────

// Atributos que llegan desde los VBOs de Geometria
layout(location = 0) in vec3 aPos;      // posición (espacio local del nodo)
layout(location = 1) in vec3 aNormal;   // normal importada del FBX
layout(location = 2) in vec2 aUV;       // coordenadas de textura

// Uniforms: valores que pone el motor (iguales para todos los vértices de un objeto)
uniform mat4 uModel;         // local -> mundo (transform del objeto)
uniform mat4 uView;          // mundo -> cámara
uniform mat4 uProjection;    // cámara -> pantalla (perspectiva)
uniform mat3 uNormalMatrix;  // inversa traspuesta de uModel (para girar bien las normales)

// Salidas hacia el fragment shader (se interpolan entre los 3 vértices del triángulo)
out vec3 vWorldPos;
out vec3 vNormal;
out vec2 vUV;

void main()
{
    vec4 worldPos = uModel * vec4(aPos, 1.0);
    vWorldPos = worldPos.xyz;
    vNormal   = uNormalMatrix * aNormal;
    vUV       = aUV;

    gl_Position = uProjection * uView * worldPos;
}
)GLSL";
}

const char* Shader::DefaultFragmentSource() {
    return R"GLSL(#version 330 core
// ─────────────────────────────────────────────
//  FRAGMENT SHADER: se ejecuta una vez por cada PÍXEL que ocupa el modelo.
//  Decide el color final: textura o color del material + iluminación.
// ─────────────────────────────────────────────

in vec3 vWorldPos;
in vec3 vNormal;
in vec2 vUV;

uniform sampler2D uTexture;   // textura difusa (unidad 0)
uniform bool  uHasTexture;    // ¿esta submalla tiene textura?
uniform vec3  uColor;         // color difuso del objeto (Inspector)

uniform vec3  uLightDir;      // dirección HACIA la luz (luz direccional, en el mundo)
uniform vec3  uLightColor;
uniform float uAmbient;       // luz ambiente (0..1)
uniform float uSpecular;      // intensidad del brillo especular
uniform float uShininess;     // tamaño del brillo (más alto = más pequeño y nítido)
uniform vec3  uCameraPos;     // posición de la cámara (para el brillo especular)

// Modo de sombreado (se elige en el Editor de Shaders):
//  0 = Blinn-Phong (luz por píxel)   1 = Normales como color
//  2 = Toon / dibujo animado         3 = Sin iluminación (sólo textura/color)
//  4 = Coordenadas UV como color
uniform int   uMode;
uniform float uTime;          // segundos desde el inicio (para efectos animados)

out vec4 FragColor;

void main()
{
    // Normal del píxel. Si la malla no tiene normales, se calcula una plana a partir
    // de las derivadas de la posición (cómo cambia entre píxeles vecinos).
    vec3 N = vNormal;
    if (length(N) < 1e-4)
        N = cross(dFdx(vWorldPos), dFdy(vWorldPos));
    N = normalize(N);
    if (!gl_FrontFacing) N = -N;   // iluminar también las caras traseras

    // Color base: textura o color del material
    vec3 base = uHasTexture ? texture(uTexture, vUV).rgb : uColor;

    if (uMode == 1) { FragColor = vec4(N * 0.5 + 0.5, 1.0); return; }   // normales -> RGB
    if (uMode == 3) { FragColor = vec4(base, 1.0); return; }            // sin luz
    if (uMode == 4) { FragColor = vec4(fract(vUV), 0.0, 1.0); return; } // UVs -> RG

    vec3 L = normalize(uLightDir);
    vec3 V = normalize(uCameraPos - vWorldPos);
    float diff = max(dot(N, L), 0.0);

    if (uMode == 2) {
        // Toon: la luz se reparte en 4 escalones en lugar de un degradado suave
        float steps = 4.0;
        diff = floor(diff * steps) / steps;
        // Borde oscuro donde la superficie se pone de perfil respecto a la cámara
        float rim = dot(N, V) < 0.2 ? 0.35 : 1.0;
        vec3 color = base * (uAmbient + (1.0 - uAmbient) * diff) * rim;
        FragColor = vec4(color, 1.0);
        return;
    }

    // Blinn-Phong: ambiente + difusa + especular (brillo)
    vec3 H = normalize(L + V);   // vector "a medio camino" entre la luz y la cámara
    float spec = (diff > 0.0) ? pow(max(dot(N, H), 0.0), uShininess) : 0.0;

    vec3 ambient  = uAmbient * base;
    vec3 diffuse  = (1.0 - uAmbient) * diff * base * uLightColor;
    vec3 specular = uSpecular * spec * uLightColor;

    FragColor = vec4(ambient + diffuse + specular, 1.0);
}
)GLSL";
}
