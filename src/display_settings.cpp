#include "display_settings.hpp"
#include <raylib.h>
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <algorithm>
#include <cmath>
#include <tuple>

namespace snake {
namespace {
void error(std::string* out,const std::string& value){if(out)*out=value;}
bool sameMode(const DisplayMode& a,const DisplayMode& b){return a.width==b.width&&a.height==b.height&&a.refreshRate==b.refreshRate;}
}
struct DisplayManager::Impl {
    explicit Impl(bool value):enabled(value){}
    bool enabled=true,initialized=false;
    DisplayTrial trial;
    std::vector<MonitorInfo> monitorInfo;
    std::vector<std::vector<DisplayMode>> modes;
    Vector2 windowPosition{};
    bool windowPositionKnown=false;
    int lastLimit=-1,lastVsync=-1;
    double lastClock=0;

    void enumerate(){
        monitorInfo.clear();modes.clear();int count=0;GLFWmonitor** all=glfwGetMonitors(&count);
        for(int i=0;i<count;++i){
            const auto* current=glfwGetVideoMode(all[i]);if(!current)continue;
            const char* name=glfwGetMonitorName(all[i]);monitorInfo.push_back({i,name?name:"Display",current->width,current->height,current->refreshRate});
            std::vector<DisplayMode> available;int n=0;const auto* list=glfwGetVideoModes(all[i],&n);
            for(int j=0;j<n;++j)if(list[j].width>=800&&list[j].height>=500)available.push_back({list[j].width,list[j].height,list[j].refreshRate});
            std::sort(available.begin(),available.end(),[](const auto& a,const auto& b){return std::tie(a.width,a.height,a.refreshRate)<std::tie(b.width,b.height,b.refreshRate);});
            available.erase(std::unique(available.begin(),available.end(),sameMode),available.end());modes.push_back(std::move(available));
        }
        if(monitorInfo.empty()){monitorInfo.push_back({0,"Display",std::max(800,GetScreenWidth()),std::max(500,GetScreenHeight()),60});modes.push_back({});}
    }
    DisplaySettings normalized(DisplaySettings value){
        Settings s;s.display=value;sanitizeSettings(s);value=s.display;
        value.monitor=std::clamp(value.monitor,0,int(monitorInfo.size())-1);
        if(value.mode==WindowMode::Windowed){
            int count=0;auto** all=glfwGetMonitors(&count);int x=0,y=0,width=monitorInfo[std::size_t(value.monitor)].width,height=monitorInfo[std::size_t(value.monitor)].height;
            if(value.monitor<count)glfwGetMonitorWorkarea(all[value.monitor],&x,&y,&width,&height);
            value.width=std::min(value.width,std::max(800,width));value.height=std::min(value.height,std::max(500,height));
        }
        if(value.refreshRate>0){
            const auto& monitor=monitorInfo[std::size_t(value.monitor)];bool supported=false;
            for(const auto& mode:modes[std::size_t(value.monitor)])if(mode.width==monitor.width&&mode.height==monitor.height&&mode.refreshRate==value.refreshRate){supported=true;break;}
            if(!supported)value.refreshRate=0;
        }
        return value;
    }
    DisplaySettings applyNative(const DisplaySettings& before,DisplaySettings after,bool force=false){
        if(!enabled)return after;
        if(force||requiresDisplayConfirmation(before,after)){
            if(!force&&!IsWindowFullscreen()&&!IsWindowState(FLAG_BORDERLESS_WINDOWED_MODE)){
                windowPosition=GetWindowPosition();windowPositionKnown=true;
            }
            if(IsWindowFullscreen())ToggleFullscreen();
            if(IsWindowState(FLAG_BORDERLESS_WINDOWED_MODE))ToggleBorderlessWindowed();
            SetWindowState(FLAG_WINDOW_RESIZABLE);SetWindowMinSize(800,500);
            if(after.mode==WindowMode::Windowed){
                int count=0;auto** all=glfwGetMonitors(&count);int x=0,y=0,w=GetMonitorWidth(after.monitor),h=GetMonitorHeight(after.monitor);
                if(after.monitor<count)glfwGetMonitorWorkarea(all[after.monitor],&x,&y,&w,&h);
                // Restore decorations and move to the target display before measuring
                // its DPI-dependent frame. GLFW positions refer to the client origin.
                SetWindowSize(after.width,after.height);
                SetWindowMonitor(after.monitor);
                int left=0,top=0,right=0,bottom=0;
                glfwGetWindowFrameSize(glfwGetCurrentContext(),&left,&top,&right,&bottom);
                after.width=std::min(after.width,std::max(800,w-left-right));
                after.height=std::min(after.height,std::max(500,h-top-bottom));
                SetWindowSize(after.width,after.height);
                const int minX=x+left,minY=y+top;
                const int maxX=std::max(minX,x+w-after.width-right),maxY=std::max(minY,y+h-after.height-bottom);
                if(windowPositionKnown&&before.monitor==after.monitor){
                    SetWindowPosition(std::clamp(int(windowPosition.x),minX,maxX),std::clamp(int(windowPosition.y),minY,maxY));
                }else SetWindowPosition(minX+(maxX-minX)/2,minY+(maxY-minY)/2);
            }else{
                SetWindowSize(after.width,after.height);SetWindowMonitor(after.monitor);
                if(after.mode==WindowMode::Borderless){ToggleBorderlessWindowed();}
                else{
                    const auto& monitor=monitorInfo[std::size_t(after.monitor)];
                    SetWindowSize(monitor.width,monitor.height);ToggleFullscreen();
                    int count=0;GLFWmonitor** all=glfwGetMonitors(&count);
                    if(after.monitor<count)glfwSetWindowMonitor(glfwGetCurrentContext(),all[after.monitor],0,0,monitor.width,monitor.height,after.refreshRate>0?after.refreshRate:GLFW_DONT_CARE);
                }
            }
            // Mode changes may recreate the swap interval. Reapply pacing once.
            lastVsync=-1;lastLimit=-1;
        }
        return after;
    }
};

DisplayManager::DisplayManager(bool enabled):impl(std::make_unique<Impl>(enabled)){}
DisplayManager::~DisplayManager()=default;
bool DisplayManager::initialize(const DisplaySettings& requested,std::string* out){
    error(out,{});if(!IsWindowReady()){error(out,"The game window is not ready.");return false;}
    impl->enumerate();const auto next=impl->applyNative({},impl->normalized(requested),true);
    impl->trial=DisplayTrial(next);impl->initialized=true;impl->lastClock=GetTime();updateFrameLimit();return true;
}
bool DisplayManager::apply(const DisplaySettings& requested,bool preview,std::string* out){
    error(out,{});if(!impl->initialized){error(out,"Display settings are not initialized.");return false;}
    const auto before=impl->trial.current();auto next=impl->normalized(requested);
    if(next!=before){next=impl->applyNative(before,next);impl->trial.apply(next,preview);impl->lastClock=GetTime();updateFrameLimit();}
    return true;
}
bool DisplayManager::confirm(){return impl->trial.confirm();}
bool DisplayManager::revert(){
    const auto before=impl->trial.current();if(!impl->trial.revert())return false;
    const auto restored=impl->applyNative(before,impl->trial.current());impl->trial.recordWindowedSize(restored.width,restored.height);updateFrameLimit();return true;
}
bool DisplayManager::update(float seconds,bool focused,bool minimized){
    const auto before=impl->trial.current();float elapsed=seconds;
    if(impl->enabled){const double clock=GetTime();elapsed=float(std::max(0.0,clock-impl->lastClock));impl->lastClock=clock;}
    const bool reverted=impl->trial.advance(elapsed);
    if(reverted){const auto restored=impl->applyNative(before,impl->trial.current());impl->trial.recordWindowedSize(restored.width,restored.height);}
    observeWindowResize();updateFrameLimit(focused,minimized);return reverted;
}
void DisplayManager::observeWindowResize(){
    if(impl->enabled&&impl->initialized&&!impl->trial.pending()&&impl->trial.current().mode==WindowMode::Windowed&&IsWindowResized()){
        impl->trial.recordWindowedSize(GetScreenWidth(),GetScreenHeight());impl->windowPosition=GetWindowPosition();impl->windowPositionKnown=true;
    }
}
void DisplayManager::updateFrameLimit(bool focused,bool minimized){
    if(!impl->enabled||!impl->initialized)return;const auto& settings=impl->trial.current();
    const int monitor=std::clamp(GetCurrentMonitor(),0,int(impl->monitorInfo.size())-1);
    // Querying the OS video mode every rendered frame is surprisingly costly
    // on Windows. Monitor metadata was cached once when the window was ready.
    const int hz=settings.mode==WindowMode::Fullscreen&&settings.monitor==monitor&&settings.refreshRate>0
        ?settings.refreshRate:impl->monitorInfo[std::size_t(monitor)].refreshRate;
    const int vsync=settings.vsync&&focused&&!minimized?1:0;
    if(vsync!=impl->lastVsync){if(vsync)SetWindowState(FLAG_VSYNC_HINT);else ClearWindowState(FLAG_VSYNC_HINT);impl->lastVsync=vsync;}
    const int limit=effectiveFrameLimit(settings,hz,focused,minimized);
    if(limit!=impl->lastLimit){SetTargetFPS(limit);impl->lastLimit=limit;}
}
bool DisplayManager::confirmationPending()const{return impl->trial.pending();}
float DisplayManager::confirmationSeconds()const{return impl->trial.secondsRemaining();}
const DisplaySettings& DisplayManager::current()const{return impl->trial.current();}
const std::vector<MonitorInfo>& DisplayManager::monitors()const{return impl->monitorInfo;}
std::vector<DisplayMode> DisplayManager::videoModes(int monitor)const {if(monitor<0||monitor>=int(impl->modes.size()))return {};return impl->modes[std::size_t(monitor)];}
std::vector<DisplayMode> DisplayManager::windowSizes(int monitor)const {
    constexpr DisplayMode candidates[]{{800,500,0},{960,600,0},{1280,720,0},{1280,800,0},{1600,900,0},{1600,1000,0},{1920,1080,0},{1920,1200,0},{2560,1440,0},{2560,1600,0},{3840,2160,0}};
    std::vector<DisplayMode> result;const int width=monitor>=0&&monitor<int(impl->monitorInfo.size())?impl->monitorInfo[std::size_t(monitor)].width:7680;
    const int height=monitor>=0&&monitor<int(impl->monitorInfo.size())?impl->monitorInfo[std::size_t(monitor)].height:4320;
    for(const auto& candidate:candidates)if(candidate.width<=width&&candidate.height<=height)result.push_back(candidate);
    const auto& current=impl->trial.current();result.push_back({current.width,current.height,0});
    std::sort(result.begin(),result.end(),[](const auto& a,const auto& b){return std::tie(a.width,a.height)<std::tie(b.width,b.height);});
    result.erase(std::unique(result.begin(),result.end(),[](const auto& a,const auto& b){return a.width==b.width&&a.height==b.height;}),result.end());return result;
}
std::vector<int> DisplayManager::refreshRates(int monitor,int width,int height)const{
    std::vector<int> rates{0};if(monitor<0||monitor>=int(impl->monitorInfo.size()))return rates;
    if(width<=0||height<=0){width=impl->monitorInfo[std::size_t(monitor)].width;height=impl->monitorInfo[std::size_t(monitor)].height;}
    for(const auto& mode:impl->modes[std::size_t(monitor)])if(mode.width==width&&mode.height==height)rates.push_back(mode.refreshRate);
    std::sort(rates.begin(),rates.end());rates.erase(std::unique(rates.begin(),rates.end()),rates.end());return rates;
}
}
