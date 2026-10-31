#pragma once

#ifdef __EMSCRIPTEN__
namespace snake::web {
// The shell loads this in-memory directory from IndexedDB before main runs.
constexpr const char* SettingsPath="/persistent/snake-settings.json";
constexpr const char* ProfilePath="/persistent/snake-survivors.save";
bool pageHidden();
// Packed CSS-pixel width/height, or zero when the canvas already matches.
// Uses cached viewport/canvas attributes, never layout or GPU readback.
int pendingViewportSize();
// Coalesces asynchronous IndexedDB writes; never blocks the simulation.
void flushSaves();
void showError(const char* message);
}
#endif
