# Motor de Videojuego en C++

## a. Descripción del Motor y Enlaces

Este proyecto consiste en un **motor de videojuegos minimalista en C++** diseñado desde cero, utilizando CMake como sistema de construcción y librerías modernas para la gestión de ventanas, renderizado gráfico y gestión de recursos (como texturas y modelos 3D).

* **Enlace al repositorio de GitHub:** [github.com/Bole05/Motor-Videojoc](https://github.com/Bole05/Motor-Videojoc)

### Integrantes del Grupo
* **Bole Wu** - [GitHub del Integrante ](https://github.com/)
* **Yangfeng Xu** - [GitHub del Integrante ](https://github.com/)

### Librerías utilizadas
SDL3 (ventana e input), OpenGL 4.6 + glad (render), Dear ImGui (interfaz), Assimp (modelos 3D), DevIL (texturas), glm (matemáticas) y fmt.

---

## b. Cómo Usar el Motor y Controles

### Requisitos previos
* Compilador compatible con C++17 o superior.
* CMake (versión 3.15 o superior).
* Gestor de paquetes `vcpkg` (utilizado para la gestión de dependencias configurado en el proyecto).

### Compilar
```
cmake -S . -B build
cmake --build build
```
El código fuente está en la carpeta `src/` y los recursos (modelos, texturas y shaders) en `assets/`.

### Controles de cámara
* **Clic Derecho (Mantener) + WASD:** Movimiento tipo FPS (First Person Shooter) libre por el espacio tridimensional.
* **Clic Derecho (Mantener) + Movimiento de Ratón:** *Free look around* (rotación de la vista de la cámara).
* **Rueda del Ratón:** Realiza zoom in/out sobre la escena.
* **ALT + Clic Izquierdo (Mantener) + Arrastrar:** Orbita la cámara alrededor del objeto seleccionado.
* **Tecla "F":** Realiza *focus* automático de la cámara hacia la geometría actualmente seleccionada.
* **SHIFT (Mantener):** Duplica la velocidad de movimiento de la cámara mientras se combina con los controles de desplazamiento.
* **Clic Central (Mantener) + Arrastrar:** Desplaza la cámara lateralmente (*pan*).

La velocidad y la sensibilidad de la cámara se pueden ajustar en el panel **Configuración → Input**.

### Selección y edición
* **Clic Izquierdo** sobre un modelo en la escena: selecciona la pieza que hay bajo el ratón (por ejemplo, la chimenea de la casa). La pieza seleccionada se resalta en naranja y su nombre aparece en la parte superior de la pantalla.
* **CTRL + Clic Izquierdo:** selecciona el modelo entero.
* **Clic Izquierdo en una zona vacía:** quita la selección.
* **Supr:** elimina el objeto seleccionado. **CTRL + D:** lo duplica.
* **F1:** activa o desactiva el **Modo Debug**.

### Gestión de Modelos y Texturas (Drag & Drop)
* **Carga automática inicial:** Al arrancar el programa, el modelo por defecto `Baker_house` se cargará automáticamente en la escena.
* **Carga de modelos FBX:** El motor acepta *Drag & Drop* de ficheros en formato `.fbx` (con canal difuso de textura) desde cualquier ubicación de tu disco directamente a la aplicación. También acepta `.obj`.
* **Aplicación de texturas:** Puedes arrastrar ficheros de texturas (`.dds` o `.png`) mediante *Drag & Drop* para aplicarlos automáticamente a los `GameObjects` que se encuentren seleccionados en ese momento. Si se sueltan encima de la miniatura de un material en el Inspector, sustituyen solo esa textura.
* **Normales:** se importan del fichero. Si el modelo no las trae, Assimp las calcula. El Inspector indica de dónde vienen.

### Ventanas del Editor e Interfaz
El editor está compuesto por diferentes paneles interactivos que se pueden mostrar u ocultar desde el menú superior **View**:
* **Escena:** Renderiza el espacio de trabajo con los modelos 3D cargados.
* **Consola:** Muestra el registro (*LOG*) detallado del proceso de carga de geometría (ASSIMP), texturas (DevIL), inicialización de librerías y posibles errores. Los avisos salen en amarillo y los errores en rojo, y se pueden filtrar.
* **Configuración:** Panel con gráficos de FPS, configuración de variables de los módulos (renderer, window, input, texturas) e información de hardware, memoria y versiones de software (SDL3, OpenGL, DevIL).
* **Jerarquía:** Lista desplegable con todos los `GameObjects` activos en la escena para su selección. Permite crear `GameObjects` vacíos y arrastrar unos objetos dentro de otros para organizar la jerarquía.
* **Inspector:** Muestra y detalla los componentes del objeto seleccionado (*Transform*, *Mesh* y *Textura*), incluyendo opciones avanzadas como visualizar normales o alternar la textura de comprobación (cuadrados blancos y negros).
* **Editor de Shaders:** Permite editar el código GLSL del vertex y del fragment shader y recompilarlo en directo, además de elegir el modo de sombreado (Blinn-Phong, normales, toon, sin luz, UVs).

### Menú Superior y Herramientas
* **File:** Contiene la opción para salir del programa, además de cargar un modelo y limpiar la escena.
* **GameObject:** Crear `GameObjects` vacíos, duplicar, eliminar y resetear el transform del objeto seleccionado.
* **Menú de Geometrías Básicas:** Permite instanciar rápidamente figuras geométricas primitivas predefinidas en la escena (cubo, esfera, cilindro, cono, pirámide y plano).
* **View:** Permite alternar la visibilidad de las diferentes ventanas del editor, además de opciones de render (rejilla, wireframe, texturas, shaders).
* **Debug:** Modo Debug con las cajas envolventes (AABB, OBB y caja de grupo) y la visualización de normales de vértice y de cara.
* **Help:** Enlaces directos a la documentación de GitHub (`/docs`), reporte de bugs (`/issues`), descargas (`/releases`) y la ventana **About** con los créditos del equipo, librerías y licencia MIT.
