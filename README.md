# Motor de Videojuego en C++

## a. Descripción del Motor y Enlaces

Este proyecto consiste en un **motor de videojuegos minimalista desarrollado en C++** diseñado desde cero, utilizando CMake como sistema de construcción y librerías modernas para la gestión de ventanas, renderizado gráfico y gestión de recursos (como texturas y modelos 3D).

* **Enlace al repositorio de GitHub:** [Inserta aquí el enlace a tu repositorio][cite: 2]

### Integrantes del Grupo
* **Bole Wu** - [GitHub del Integrante ](https://github.com/)
* **Yangfeng Xu** - [GitHub del Integrante ](https://github.com/)


---

## b. Cómo Usar el Motor y Controles

### Requisitos previos
* Compilador compatible con C++17 o superior.
* CMake (versión 3.15 o superior).
* Gestor de paquetes `vcpkg` (utilizado para la gestión de dependencias configurado en el proyecto).

### Controles de cámara
* **Clic Derecho (Mantener) + WASD:** Movimiento tipo FPS (First Person Shooter) libre por el espacio tridimensional.
* **Clic Derecho (Mantener) + Movimiento de Ratón:** *Free look around* (rotación de la vista de la cámara).
* **Rueda del Ratón:** Realiza zoom in/out sobre la escena.
* **ALT + Clic Izquierdo (Mantener) + Arrastrar:** Orbita la cámara alrededor del objeto seleccionado.
* **Tecla "F":** Realiza *focus* automático de la cámara hacia la geometría actualmente seleccionada.
* **SHIFT (Mantener):** Duplica la velocidad de movimiento de la cámara mientras se combina con los controles de desplazamiento.

### Gestión de Modelos y Texturas (Drag & Drop)
* **Carga automática inicial:** Al arrancar el programa, el modelo por defecto `Baker_house` se cargará automáticamente en la escena[cite: 2].
* **Carga de modelos FBX:** El motor acepta *Drag & Drop* de ficheros en formato `.fbx` (con canal difuso de textura) desde cualquier ubicación de tu disco directamente a la aplicación[cite: 2].
* **Aplicación de texturas:** Puedes arrastrar ficheros de texturas (`.dds` o `.png`) mediante *Drag & Drop* para aplicarlos automáticamente a los `GameObjects` que se encuentren seleccionados en ese momento[cite: 2].

### Ventanas del Editor e Interfaz
El editor está compuesto por diferentes paneles interactivos que se pueden mostrar u ocultar desde el menú superior **View**:
* **Escena:** Renderiza el espacio de trabajo con los modelos 3D cargados[cite: 2].
* **Consola:** Muestra el registro (*LOG*) detallado del proceso de carga de geometría (ASSIMP), texturas (DevIL), inicialización de librerías y posibles errores[cite: 2].
* **Configuración:** Panel con gráficos de FPS, configuración de variables de los módulos (renderer, window, input, texturas) e información de hardware, memoria y versiones de software (SDL3, OpenGL, DevIL)[cite: 2].
* **Jerarquía:** Lista desplegable con todos los `GameObjects` activos en la escena para su selección[cite: 2].
* **Inspector:** Muestra y detalla los componentes del objeto seleccionado (*Transform*, *Mesh* y *Textura*), incluyendo opciones avanzadas como visualizar normales o alternar la textura de comprobación (cuadrados blancos y negros)[cite: 2].

### Menú Superior y Herramientas
* **File:** Contiene la opción para salir del programa[cite: 2].
* **View:** Permite alternar la visibilidad de las diferentes ventanas del editor[cite: 2].
* **Help:** Enlaces directos a la documentación de GitHub (`/docs`), reporte de bugs (`/issues`), descargas (`/releases`) y la ventana **About** con los créditos del equipo, librerías y licencia MIT[cite: 2].
* **Menú de Geometrías Básicas:** Permite instanciar rápidamente figuras geométricas primitivas predefinidas en la escena[cite: 2].
