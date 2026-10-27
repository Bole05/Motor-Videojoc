#version 330 core
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
