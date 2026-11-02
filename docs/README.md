# Documentación - Motor-Videojoc

Motor de videojuegos minimalista en C++17 (SDL3, OpenGL, Dear ImGui, Assimp, DevIL, glm).
Entrega 1: **visualizador de geometría** con drag & drop, cámara tipo Unity y editor con ImGui.

* **Cómo usar el motor, controles y funcionalidades extra:** [README principal](../README.md)
* **Lista de tareas del equipo (estimación y horas):** [Lista_de_Tareas.md](Lista_de_Tareas.md)
* **Descargas:** [Releases](https://github.com/Bole05/Motor-Videojoc/releases)
* **Reportar un bug:** [Issues](https://github.com/Bole05/Motor-Videojoc/issues)

## Arquitectura

```
Engine ──┬── Window      (SDL3 + contexto OpenGL)
         ├── Renderer    MOTOR: escena de GameObjects, cámara, selección (raycast), render 3D
         │     └── GameObject ── ComponentTransform  (posición, rotación, escala)
         │                    ├─ ComponentMesh       (Geometria compartida + submallas)
         │                    └─ ComponentTexture    (textura manual, checker)
         └── Editor      EDITOR: toda la interfaz con Dear ImGui
```

| Módulo | Ficheros | Responsabilidad |
|---|---|---|
| Engine | `Engine.h/.cpp`, `Main.cpp` | Bucle principal, eventos de SDL, límite de FPS |
| Window | `Window.h/.cpp` | Ventana SDL3 y contexto OpenGL 4.6 (compatibility) |
| Renderer | `Renderer.h/.cpp` | Escena, cámara, selección, render (shaders y pipeline fijo), modo debug |
| GameObjects | `GameObject.h/.cpp`, `Component*.h/.cpp` | Patrón Component: Transform, Mesh y Textura |
| Geometría | `Geometria.h/.cpp` | Importación con Assimp, VBOs, materiales, primitivas, raycast |
| Texturas | `Texture.h/.cpp` | Carga con DevIL y subida a la GPU |
| Shaders | `Shader.h/.cpp`, `assets/shaders/` | Compilación de GLSL y uniforms |
| Editor | `Editor.h/.cpp` | Menús y paneles (Jerarquía, Inspector, Consola, Configuración, About, Shaders) |
| Log | `Log.h/.cpp` | Historial de mensajes para la Consola |

## Frame

1. `Engine::ProcessInput` – eventos de SDL (primero pasan por ImGui; si ImGui no usa el ratón, van a la cámara / selección).
2. `Engine::Update` – cámara WASD y `GameObject::Update` (cada componente).
3. `Engine::Render` – `Editor::BeginFrame` → `Editor::Draw` → `Renderer::Render` (escena) → `Editor::EndFrame` (ImGui encima) → swap.
