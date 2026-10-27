#include "Log.h"
#include <iostream>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include <algorithm>
#include <cctype>

static const size_t MAX_LOG_ENTRIES = 5000;

static std::vector<LogEntry>& Entries() {
    static std::vector<LogEntry> entries;
    return entries;
}
static int levelCounts[3] = { 0, 0, 0 };

// Decide si un mensaje es un error, un aviso o información según su texto
// (los mensajes de Assimp empiezan por "Error,", "Warn,", "Info,"...)
static LogLevel DetectLevel(const std::string& msg) {
    std::string low(msg);
    std::transform(low.begin(), low.end(), low.begin(), [](unsigned char c) { return (char)std::tolower(c); });
    static const char* errorWords[] = { "error", "no se pudo", "no se ha podido", "no se encuentra",
                                        "no encontrada", "failed", "fallo" };
    static const char* warnWords[]  = { "warn", "aviso", "sin normales", "no tiene" };
    for (const char* w : errorWords) if (low.find(w) != std::string::npos) return LogLevel::Error;
    for (const char* w : warnWords)  if (low.find(w) != std::string::npos) return LogLevel::Warning;
    return LogLevel::Info;
}

void Log(const char file[], int line, const char* format, ...)
{
    static char tmpString1[4096];
    va_list ap;

    // Construct the string from variable arguments
    va_start(ap, format);
    vsnprintf(tmpString1, 4096, format, ap);
    va_end(ap);

    // Construct the final log message
    std::string logMessage = std::string("\n") + file + "(" + std::to_string(line) + ") : " + tmpString1;

    // Print the formatted string to the standard error stream
    std::cerr << logMessage << std::endl;

    // Guardar también en el historial para el panel "Consola"
    std::string msg(tmpString1);
    while (!msg.empty() && (msg.back() == '\n' || msg.back() == '\r')) msg.pop_back();
    if (msg.empty()) return;

    const char* base = file;   // sólo el nombre del fichero, sin la ruta
    for (const char* p = file; *p; ++p) if (*p == '/' || *p == '\\') base = p + 1;

    auto& entries = Entries();
    if (entries.size() >= MAX_LOG_ENTRIES) {
        levelCounts[(int)entries.front().level]--;
        entries.erase(entries.begin());
    }
    LogEntry e;
    e.level = DetectLevel(msg);
    e.message = msg;
    e.source = std::string(base) + "(" + std::to_string(line) + ")";
    levelCounts[(int)e.level]++;
    entries.push_back(std::move(e));
}

const std::vector<LogEntry>& GetLogEntries() {
    return Entries();
}

void ClearLog() {
    Entries().clear();
    levelCounts[0] = levelCounts[1] = levelCounts[2] = 0;
}

int GetLogCount(LogLevel level) {
    return levelCounts[(int)level];
}
