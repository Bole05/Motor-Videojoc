#version 330 core
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
