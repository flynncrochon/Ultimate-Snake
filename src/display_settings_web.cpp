#include "display_settings.hpp"
#include <raylib.h>
#include <algorithm>

namespace snake {
// The page owns canvas size/fullscreen. Browser requestAnimationFrame owns
// pacing: raylib software FPS waits must never block the JavaScript thread.
struct DisplayManager::Impl {
    bool initialized=false;
    DisplaySettings settings;
    std::vector<MonitorInfo> monitors{{0,"Browser canvas",800,500,60}};
    DisplaySettings normalized(DisplaySettings requested) const {
        Settings sane;sane.display=requested;sanitizeSettings(sane);
        auto value=sane.display;
        value.mode=WindowMode::Windowed;value.monitor=0;value.refreshRate=0;
        value.width=std::max(800,GetScreenWidth());value.height=std::max(500,GetScreenHeight());
        value.vsync=true;value.targetFps=0;value.limitToDisplay=true;
        return value;
    }
};
DisplayManager::DisplayManager(bool):impl(std::make_unique<Impl>()){}
DisplayManager::~DisplayManager()=default;
bool DisplayManager::initialize(const DisplaySettings& requested,std::string* error){
    if(error)error->clear();
    if(!IsWindowReady()){if(error)*error="The browser canvas is not ready.";return false;}
    impl->initialized=true;impl->settings=impl->normalized(requested);
    observeWindowResize();SetTargetFPS(0);return true;
}
bool DisplayManager::apply(const DisplaySettings& requested,bool,std::string* error){
    if(error)error->clear();
    if(!impl->initialized){if(error)*error="Display settings are not initialized.";return false;}
    impl->settings=impl->normalized(requested);return true;
}
bool DisplayManager::confirm(){return false;}
bool DisplayManager::revert(){return false;}
bool DisplayManager::update(float,bool,bool){observeWindowResize();return false;}
void DisplayManager::updateFrameLimit(bool,bool){}
void DisplayManager::observeWindowResize(){
    if(!impl->initialized)return;
    impl->settings.width=std::max(800,GetScreenWidth());impl->settings.height=std::max(500,GetScreenHeight());
    impl->monitors[0].width=impl->settings.width;impl->monitors[0].height=impl->settings.height;
}
bool DisplayManager::confirmationPending()const{return false;}
float DisplayManager::confirmationSeconds()const{return 0;}
const DisplaySettings& DisplayManager::current()const{return impl->settings;}
const std::vector<MonitorInfo>& DisplayManager::monitors()const{return impl->monitors;}
std::vector<DisplayMode> DisplayManager::videoModes(int)const{return {{impl->settings.width,impl->settings.height,0}};}
std::vector<DisplayMode> DisplayManager::windowSizes(int monitor)const{return videoModes(monitor);}
std::vector<int> DisplayManager::refreshRates(int,int,int)const{return {0};}
}
