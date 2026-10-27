#include "settings.hpp"
#include "../third_party/nlohmann/json.hpp"
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <limits>
#include <set>
#include <sstream>
#include <tuple>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace snake {
namespace {
using Json=nlohmann::json;
void error(std::string* out,const std::string& value){if(out)*out=value;}
float volume(float value){return std::isfinite(value)?std::clamp(value,0.0f,1.0f):1;}
int integer(const Json& j,const char* key,int fallback){
    const auto it=j.find(key);if(it==j.end()||!it->is_number_integer())return fallback;
    try{const auto value=it->get<long long>();return int(std::clamp(value,static_cast<long long>(std::numeric_limits<int>::min()),static_cast<long long>(std::numeric_limits<int>::max())));}catch(...){return fallback;}
}
bool boolean(const Json& j,const char* key,bool fallback){const auto it=j.find(key);return it!=j.end()&&it->is_boolean()?it->get<bool>():fallback;}
float real(const Json& j,const char* key,float fallback){const auto it=j.find(key);if(it==j.end()||!it->is_number())return fallback;try{return it->get<float>();}catch(...){return fallback;}}
const Json& section(const Json& j,const char* key){static const Json empty=Json::object();const auto it=j.find(key);return it!=j.end()&&it->is_object()?*it:empty;}
}
bool operator==(const DisplaySettings& a,const DisplaySettings& b){return std::tie(a.mode,a.width,a.height,a.monitor,a.refreshRate,a.targetFps,a.vsync,a.limitToDisplay,a.integerScale,a.backgroundFps,a.pauseUnfocused)==std::tie(b.mode,b.width,b.height,b.monitor,b.refreshRate,b.targetFps,b.vsync,b.limitToDisplay,b.integerScale,b.backgroundFps,b.pauseUnfocused);}
bool operator==(const AudioSettings& a,const AudioSettings& b){return std::tie(a.enabled,a.master,a.effects,a.combat,a.interfaceVolume)==std::tie(b.enabled,b.master,b.effects,b.combat,b.interfaceVolume);}
bool operator==(const RenderSettings& a,const RenderSettings& b){return std::tie(a.quality,a.particles,a.damageNumbers,a.weaponEffects,a.showFps,a.minimap)==std::tie(b.quality,b.particles,b.damageNumbers,b.weaponEffects,b.showFps,b.minimap);}
bool operator==(const ControlSettings& a,const ControlSettings& b){return a.keys==b.keys&&a.alternateArrows==b.alternateArrows;}
bool operator==(const Settings& a,const Settings& b){return a.display==b.display&&a.audio==b.audio&&a.render==b.render&&a.controls==b.controls;}
Settings defaultSettings(){return {};}
const char* windowModeName(WindowMode mode){switch(mode){case WindowMode::Windowed:return "Windowed";case WindowMode::Borderless:return "Borderless";case WindowMode::Fullscreen:return "Fullscreen";}return "Windowed";}
const char* actionName(Action action){constexpr const char* names[]{"Move Up","Move Down","Move Left","Move Right","Pause","Settings","Evolution Codex","Mute","Reroll","Banish"};const auto i=std::size_t(action);return i<ActionCount?names[i]:"Unknown";}
bool validBindingKey(int key){return (key>=32&&key<=96)||(key>=256&&key<=269)||(key>=280&&key<=284)||(key>=290&&key<=314)||(key>=320&&key<=336)||(key>=340&&key<=348);}
bool bindKey(ControlSettings& controls,Action action,int key,std::string* out){
    const auto index=std::size_t(action);if(index>=ActionCount||!validBindingKey(key)){error(out,"Choose a keyboard key.");return false;}
    for(std::size_t i=0;i<ActionCount;++i)if(i!=index&&controls.keys[i]==key){error(out,std::string("Already assigned to ")+actionName(Action(i))+".");return false;}
    controls.keys[index]=key;error(out,{});return true;
}
void sanitizeSettings(Settings& s){
    auto& d=s.display;if(int(d.mode)<0||int(d.mode)>2)d.mode=WindowMode::Windowed;
    d.width=std::clamp(d.width,800,7680);d.height=std::clamp(d.height,500,4320);d.monitor=std::clamp(d.monitor,0,15);
    d.refreshRate=std::clamp(d.refreshRate,0,500);d.targetFps=d.targetFps==0?0:std::clamp(d.targetFps,30,1000);
    d.integerScale=std::clamp(d.integerScale,0,8);d.backgroundFps=std::clamp(d.backgroundFps,5,120);
    s.audio.master=volume(s.audio.master);s.audio.effects=volume(s.audio.effects);s.audio.combat=volume(s.audio.combat);s.audio.interfaceVolume=volume(s.audio.interfaceVolume);
    s.render.quality=std::clamp(s.render.quality,0,2);
    std::set<int> seen;for(const int key:s.controls.keys)if(!validBindingKey(key)||!seen.insert(key).second){const bool arrows=s.controls.alternateArrows;s.controls=ControlSettings{};s.controls.alternateArrows=arrows;break;}
}
bool loadSettings(const std::string& path,Settings& s,std::string* out){
    s=defaultSettings();error(out,{});
    try{
        std::ifstream input(std::filesystem::u8path(path),std::ios::binary);if(!input)return false;
        input.seekg(0,std::ios::end);const auto length=input.tellg();if(length<0||length>65536){error(out,"Settings file exceeds 64 KiB.");return false;}input.seekg(0);
        Json root=Json::parse(input);if(!root.is_object()){error(out,"Settings must be a JSON object.");return false;}
        const int version=integer(root,"version",1);
        if(version<1||version>2){error(out,"Unsupported settings version.");return false;}
        const auto& d=section(root,"display");const auto& a=section(root,"audio");const auto& r=section(root,"render");const auto& c=section(root,"controls");
        if(d.contains("mode")&&d["mode"].is_string()){const auto mode=d["mode"].get<std::string>();s.display.mode=mode=="borderless"?WindowMode::Borderless:mode=="fullscreen"?WindowMode::Fullscreen:WindowMode::Windowed;}
        s.display.width=integer(d,"width",s.display.width);s.display.height=integer(d,"height",s.display.height);s.display.monitor=integer(d,"monitor",s.display.monitor);s.display.refreshRate=integer(d,"refresh_rate",s.display.refreshRate);s.display.targetFps=integer(d,"target_fps",s.display.targetFps);
        s.display.vsync=boolean(d,"vsync",s.display.vsync);s.display.limitToDisplay=boolean(d,"limit_to_display",s.display.limitToDisplay);s.display.integerScale=integer(d,"integer_scale",s.display.integerScale);s.display.backgroundFps=integer(d,"background_fps",s.display.backgroundFps);s.display.pauseUnfocused=boolean(d,"pause_unfocused",s.display.pauseUnfocused);
        s.audio.enabled=boolean(a,"enabled",s.audio.enabled);s.audio.master=real(a,"master",s.audio.master);s.audio.effects=real(a,"effects",s.audio.effects);s.audio.combat=real(a,"combat",s.audio.combat);s.audio.interfaceVolume=real(a,"interface",s.audio.interfaceVolume);
        s.render.quality=integer(r,"quality",s.render.quality);s.render.particles=boolean(r,"particles",s.render.particles);s.render.damageNumbers=boolean(r,"damage_numbers",s.render.damageNumbers);s.render.weaponEffects=boolean(r,"weapon_effects",s.render.weaponEffects);s.render.showFps=boolean(r,"show_fps",s.render.showFps);s.render.minimap=boolean(r,"minimap",s.render.minimap);
        s.controls.alternateArrows=boolean(c,"alternate_arrows",s.controls.alternateArrows);
        const auto keys=c.find("keys");
        if(keys!=c.end()&&keys->is_array()){
            // Version 1 stored a dash binding between movement and pause.
            const bool legacy=version==1&&keys->size()==ActionCount+1;
            if(legacy||keys->size()==ActionCount){
                bool valid=true;
                for(std::size_t i=0;i<ActionCount;++i){
                    const auto& key=(*keys)[i+(legacy&&i>=std::size_t(Action::Pause)?1:0)];
                    if(!key.is_number_integer()){valid=false;break;}
                    try{s.controls.keys[i]=key.get<int>();}catch(...){valid=false;break;}
                }
                if(!valid)s.controls.keys=ControlSettings{}.keys;
            }
        }
        sanitizeSettings(s);return true;
    }catch(const std::exception& ex){s=defaultSettings();error(out,std::string("Could not read settings: ")+ex.what());return false;}
}
bool saveSettings(const std::string& path,const Settings& requested,std::string* out){
    Settings s=requested;sanitizeSettings(s);error(out,{});
    try{
        Json root;root["version"]=2;const auto& d=s.display;
        root["display"]={{"mode",d.mode==WindowMode::Borderless?"borderless":d.mode==WindowMode::Fullscreen?"fullscreen":"windowed"},{"width",d.width},{"height",d.height},{"monitor",d.monitor},{"refresh_rate",d.refreshRate},{"target_fps",d.targetFps},{"vsync",d.vsync},{"limit_to_display",d.limitToDisplay},{"integer_scale",d.integerScale},{"background_fps",d.backgroundFps},{"pause_unfocused",d.pauseUnfocused}};
        root["audio"]={{"enabled",s.audio.enabled},{"master",s.audio.master},{"effects",s.audio.effects},{"combat",s.audio.combat},{"interface",s.audio.interfaceVolume}};
        root["render"]={{"quality",s.render.quality},{"particles",s.render.particles},{"damage_numbers",s.render.damageNumbers},{"weapon_effects",s.render.weaponEffects},{"show_fps",s.render.showFps},{"minimap",s.render.minimap}};
        root["controls"]={{"keys",s.controls.keys},{"alternate_arrows",s.controls.alternateArrows}};
        const std::filesystem::path destination=std::filesystem::u8path(path);const auto parent=destination.parent_path();if(!parent.empty())std::filesystem::create_directories(parent);
        auto temporary=destination;temporary+=".tmp";
        {std::ofstream output(temporary,std::ios::binary|std::ios::trunc);if(!output)throw std::runtime_error("Settings file is not writable.");output<<root.dump(2)<<'\n';output.flush();if(!output)throw std::runtime_error("Settings write failed.");}
#ifdef _WIN32
        if(!MoveFileExW(temporary.c_str(),destination.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))throw std::runtime_error("Could not replace settings file.");
#else
        std::filesystem::rename(temporary,destination);
#endif
        return true;
    }catch(const std::exception& ex){error(out,ex.what());return false;}
}
int effectiveFrameLimit(const DisplaySettings& d,int monitorHz,bool focused,bool minimized){
    const int hz=monitorHz>0?monitorHz:60;
    if(minimized||!focused)return std::min(std::clamp(d.backgroundFps,5,120),hz);
    if(d.vsync)return 0;
    int target=d.targetFps>0?std::clamp(d.targetFps,30,1000):0;
    if(d.limitToDisplay)target=target?std::min(target,hz):hz;
    return target;
}
int canvasScale(const DisplaySettings& d,int width,int height){const int fit=std::max(1,std::min(width/800,height/500));return d.integerScale>0?std::min(fit,d.integerScale):fit;}
bool requiresDisplayConfirmation(const DisplaySettings& a,const DisplaySettings& b){return a.mode!=b.mode||a.width!=b.width||a.height!=b.height||a.monitor!=b.monitor||a.refreshRate!=b.refreshRate;}
DisplayTrial::DisplayTrial(DisplaySettings value):applied(value),previous(value){}
bool DisplayTrial::apply(DisplaySettings value,bool preview){
    Settings sane;sane.display=value;sanitizeSettings(sane);value=sane.display;if(value==applied)return false;
    if(!awaiting)previous=applied;applied=value;
    awaiting=preview&&requiresDisplayConfirmation(previous,applied);remaining=awaiting?15.0f:0;if(!awaiting)previous=applied;return true;
}
bool DisplayTrial::confirm(){if(!awaiting)return false;awaiting=false;remaining=0;previous=applied;return true;}
bool DisplayTrial::revert(){if(!awaiting)return false;applied=previous;awaiting=false;remaining=0;return true;}
bool DisplayTrial::advance(float seconds){if(!awaiting||!std::isfinite(seconds)||seconds<=0)return false;remaining=std::max(0.0f,remaining-seconds);return remaining<=0?revert():false;}
void DisplayTrial::recordWindowedSize(int width,int height){if(awaiting||applied.mode!=WindowMode::Windowed)return;Settings sane;sane.display=applied;sane.display.width=width;sane.display.height=height;sanitizeSettings(sane);applied=sane.display;previous=applied;}
}
