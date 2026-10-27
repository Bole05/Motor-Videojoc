#pragma once

#include <cstdint>

// Información del sistema que SDL no da (está separada para no incluir <windows.h> en el resto del motor)
namespace SystemInfo {
    // Memoria que está usando el proceso del motor, en bytes (0 si no se puede saber)
    uint64_t GetProcessMemoryUsed();
    // Pico máximo de memoria usada por el proceso desde que arrancó, en bytes
    uint64_t GetProcessMemoryPeak();
}
