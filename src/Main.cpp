#include <SDL3/SDL_main.h>
#include <nlohmann/json.hpp>
#include "Engine.h"

int main(int argc, char* argv[]) {
    // 1. Instanciamos el Motor
    Engine engine;

    // 2. Inicializamos todos los subsistemas (Ventana, OpenGL/Glad, Texturas, Geometría)
    if (!engine.Init()) {
        return -1;
    }

    // Prueba si la libreria de nlohmann se inicializa bien o no
    nlohmann::json prueba;
    prueba["hola"] = "mundo";
    LOG("JSON: %s", prueba.dump().c_str());

    // 3. Ejecutamos el bucle principal del motor
    engine.Run();

    // 4. Limpieza de recursos al salir
    engine.CleanUp();

    return 0;
}