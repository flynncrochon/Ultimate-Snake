#pragma once

#include <array>
#include <cstddef>
#include <string>

namespace snake {
enum class WindowMode { Windowed, Borderless, Fullscreen };
enum class Action { Up, Down, Left, Right, Pause, Settings, Codex, Mute, Reroll, Banish, Count };
constexpr std::size_t ActionCount=std::size_t(Action::Count);

struct DisplaySettings {
    WindowMode mode=WindowMode::Windowed;
    int width=1600,height=1000,monitor=0,refreshRate=0,targetFps=0;
    bool vsync=true,limitToDisplay=true;
    int integerScale=0,backgroundFps=30;
    bool pauseUnfocused=true;
};
struct AudioSettings {bool enabled=true;float master=1,effects=1,combat=1,interfaceVolume=1;};
struct RenderSettings {int quality=2;bool particles=true,damageNumbers=true,weaponEffects=true,showFps=false,minimap=true;};
struct ControlSettings {
    // raylib/GLFW key values; this model has no window or graphics dependency.
    std::array<int,ActionCount> keys{{87,83,65,68,256,299,258,77,82,66}};
    bool alternateArrows=true;
};
struct Settings {DisplaySettings display;AudioSettings audio;RenderSettings render;ControlSettings controls;};

bool operator==(const DisplaySettings&,const DisplaySettings&);
bool operator==(const AudioSettings&,const AudioSettings&);
bool operator==(const RenderSettings&,const RenderSettings&);
bool operator==(const ControlSettings&,const ControlSettings&);
bool operator==(const Settings&,const Settings&);
inline bool operator!=(const DisplaySettings& a,const DisplaySettings& b){return !(a==b);}
inline bool operator!=(const Settings& a,const Settings& b){return !(a==b);}

Settings defaultSettings();
void sanitizeSettings(Settings&);
bool loadSettings(const std::string& path,Settings&,std::string* error=nullptr);
bool saveSettings(const std::string& path,const Settings&,std::string* error=nullptr);
const char* windowModeName(WindowMode);
const char* actionName(Action);
bool validBindingKey(int key);
bool bindKey(ControlSettings&,Action,int key,std::string* error=nullptr);

// Zero means no software limiter. VSync owns foreground pacing when enabled.
int effectiveFrameLimit(const DisplaySettings&,int monitorHz,bool focused=true,bool minimized=false);
// Preserve square source pixels and cap a manual integer scale to what fits.
int canvasScale(const DisplaySettings&,int clientWidth,int clientHeight);
bool requiresDisplayConfirmation(const DisplaySettings& before,const DisplaySettings& after);

// Pure rollback state used by the native adapter and device-free tests.
class DisplayTrial {
public:
    explicit DisplayTrial(DisplaySettings initial={});
    bool apply(DisplaySettings next,bool preview=true);
    bool confirm();
    bool revert();
    bool advance(float seconds);
    void recordWindowedSize(int width,int height);
    bool pending() const{return awaiting;}
    float secondsRemaining() const{return remaining;}
    const DisplaySettings& current() const{return applied;}
private:
    DisplaySettings applied,previous;
    bool awaiting=false;
    float remaining=0;
};
}
