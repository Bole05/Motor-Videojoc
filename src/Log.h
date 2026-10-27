#ifndef __LOG_H__
#define __LOG_H__

#include <cstdio>
#include <cstdarg>
#include <string>
#include <vector>

#define LOG(format, ...) Log(__FILE__, __LINE__, format, ##__VA_ARGS__)

void Log(const char file[], int line, const char* format, ...);

// ─────────────────────────────────────────────
//  Historial de mensajes para el panel "Consola"
// ─────────────────────────────────────────────
enum class LogLevel { Info, Warning, Error };

struct LogEntry {
    LogLevel level = LogLevel::Info;
    std::string message;   // texto del mensaje
    std::string source;    // fichero(línea) desde donde se llamó a LOG
};

// Todos los mensajes guardados (como máximo los últimos MAX_LOG_ENTRIES)
const std::vector<LogEntry>& GetLogEntries();
void ClearLog();
// Número de mensajes de cada tipo (para los filtros de la consola)
int GetLogCount(LogLevel level);

#endif  // __LOG_H__
