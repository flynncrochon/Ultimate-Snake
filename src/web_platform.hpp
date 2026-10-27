#pragma once

#ifdef __EMSCRIPTEN__
namespace snake::web {
// The shell loads this in-memory directory from IndexedDB before main runs.
constexpr const char* SettingsPath="/persistent/snake-settings.json";
constexpr const char* ProfilePath="/persistent/snake-survivors.save";
bool pageHidden();
// Coalesces asynchronous IndexedDB writes; never blocks the simulation.
void flushSaves();
void showError(const char* message);
}
#endif
