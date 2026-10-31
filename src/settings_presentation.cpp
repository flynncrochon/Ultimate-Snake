#include "presentation.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

namespace snake {
namespace {
constexpr int PanelX=56,PanelY=30,PanelW=688,PanelH=440;
constexpr int RowX=90,RowY=119,RowPitch=23,ControlX=365,ControlW=340,RowH=20;
constexpr Color White{245,245,245,255},Muted{184,188,196,255},Disabled{140,145,154,255};
constexpr Color Dim{68,68,68,255},Accent{255,200,95,255};
enum class Kind { Choice,Toggle,Slider,Key };
struct Row {std::string label,value;Kind kind=Kind::Choice;bool disabled=false;float slider=0;};
bool within(Vec point,int x,int y,int width,int height){return point.x>=x&&point.y>=y&&point.x<x+width&&point.y<y+height;}
bool pending(const Presentation& app){return app.displayConfirmationPending&&app.displayConfirmationPending();}
int monitorCount(){return std::max(1,GetMonitorCount());}
int monitorIndex(int index){return std::clamp(index,0,monitorCount()-1);}
const MonitorInfo* monitorInfo(const Presentation& app,int monitor){
    if(!app.displayOptions)return nullptr;const auto& options=app.displayOptions->monitors();
    return monitor>=0&&monitor<int(options.size())?&options[std::size_t(monitor)]:nullptr;
}
int refresh(const Presentation& app,int monitor){const auto* info=monitorInfo(app,monitor);const int hz=info?info->refreshRate:GetMonitorRefreshRate(monitorIndex(monitor));return hz>0?hz:60;}
std::string keyName(int key){
    if(key==KEY_SPACE)return "Space";
    if(key>=KEY_A&&key<=KEY_Z)return std::string(1,char(key));
    if(key>=KEY_ZERO&&key<=KEY_NINE)return std::string(1,char(key));
    if(key>=KEY_F1&&key<=KEY_F12)return "F"+std::to_string(key-KEY_F1+1);
    switch(key){case KEY_ESCAPE:return "Escape";case KEY_ENTER:return "Enter";case KEY_TAB:return "Tab";
    case KEY_BACKSPACE:return "Backspace";case KEY_DELETE:return "Delete";case KEY_INSERT:return "Insert";
    case KEY_UP:return "Up";case KEY_DOWN:return "Down";case KEY_LEFT:return "Left";case KEY_RIGHT:return "Right";
    case KEY_HOME:return "Home";case KEY_END:return "End";case KEY_PAGE_UP:return "Page Up";case KEY_PAGE_DOWN:return "Page Down";
    case KEY_LEFT_SHIFT:return "Left Shift";case KEY_RIGHT_SHIFT:return "Right Shift";
    case KEY_LEFT_CONTROL:return "Left Ctrl";case KEY_RIGHT_CONTROL:return "Right Ctrl";
    case KEY_LEFT_ALT:return "Left Alt";case KEY_RIGHT_ALT:return "Right Alt";default:break;}
    if(key>=32&&key<127)return std::string(1,char(key));
    return "Key "+std::to_string(key);
}
template<class T>void cycle(T& value,const std::vector<T>& choices,int direction){
    if(choices.empty())return;
    const auto found=std::find(choices.begin(),choices.end(),value);
    const int index=found==choices.end()?0:int(found-choices.begin());
    value=choices[std::size_t((index+direction+int(choices.size()))%int(choices.size()))];
}
std::vector<DisplayMode> sizes(const Presentation& app){
    const auto& d=app.settingsDraft.display;std::vector<DisplayMode> result;
    if(app.displayOptions)result=d.mode==WindowMode::Fullscreen?app.displayOptions->videoModes(d.monitor):app.displayOptions->windowSizes(d.monitor);
    if(result.empty())result={{800,500,0},{1280,800,0},{1600,1000,0},{1920,1080,0},{2560,1440,0},{3840,2160,0}};
    result.erase(std::remove_if(result.begin(),result.end(),[](const DisplayMode& mode){return mode.width<800||mode.height<500;}),result.end());
    std::sort(result.begin(),result.end(),[](const auto& a,const auto& b){return a.width!=b.width?a.width<b.width:a.height<b.height;});
    result.erase(std::unique(result.begin(),result.end(),[](const auto& a,const auto& b){return a.width==b.width&&a.height==b.height;}),result.end());
    if(std::none_of(result.begin(),result.end(),[&](const auto& mode){return mode.width==d.width&&mode.height==d.height;}))result.push_back({d.width,d.height,0});
    return result;
}
std::vector<int> rates(const Presentation& app){
    const auto& d=app.settingsDraft.display;
    auto result=app.displayOptions?app.displayOptions->refreshRates(d.monitor):std::vector<int>{0,refresh(app,d.monitor)};
    if(std::find(result.begin(),result.end(),0)==result.end())result.insert(result.begin(),0);
    return result;
}
std::string monitorName(const Presentation& app,int index){
    std::string name;
    if(app.displayOptions){const auto& options=app.displayOptions->monitors();if(index>=0&&index<int(options.size()))name=options[std::size_t(index)].name;}
    if(name.empty()){const char* current=GetMonitorName(monitorIndex(index));name=current?current:"Display";}
    if(name.size()>30)name=name.substr(0,27)+"...";
    return std::to_string(index+1)+": "+name;
}
int maximumScale(const Presentation& app){
    const auto& settings=app.settingsDraft.display;const auto* info=monitorInfo(app,settings.monitor);
    const int width=settings.mode!=WindowMode::Windowed?(info?info->width:GetMonitorWidth(monitorIndex(settings.monitor))):settings.width;
    const int height=settings.mode!=WindowMode::Windowed?(info?info->height:GetMonitorHeight(monitorIndex(settings.monitor))):settings.height;
    return std::clamp(std::min(width/CanvasWidth,height/CanvasHeight),1,8);
}
std::vector<Row> rows(const Presentation& app){
    const auto& s=app.settingsDraft;std::vector<Row> result;
    auto toggle=[&](const char* name,bool value,bool disabled=false){result.push_back({name,value?"Enabled":"Disabled",Kind::Toggle,disabled,0});};
    auto slider=[&](const char* name,float value){result.push_back({name,std::to_string(int(std::lround(value*100)))+"%",Kind::Slider,false,value});};
    if(app.settingsTabIndex()==0){
        result.push_back({"Window mode",windowModeName(s.display.mode)});
        result.push_back({"Display",monitorName(app,s.display.monitor)});
        result.push_back({"Window size",std::to_string(s.display.width)+" x "+std::to_string(s.display.height),Kind::Choice,s.display.mode!=WindowMode::Windowed});
        result.push_back({"Refresh rate",s.display.mode!=WindowMode::Fullscreen?std::to_string(refresh(app,s.display.monitor))+" Hz (desktop)":s.display.refreshRate==0?"Automatic":std::to_string(s.display.refreshRate)+" Hz",Kind::Choice,s.display.mode!=WindowMode::Fullscreen});
        result.push_back({"Frame rate limit",s.display.vsync?"Display refresh":s.display.targetFps==0?"Uncapped":std::to_string(s.display.targetFps)+" FPS",Kind::Choice,s.display.vsync});
        toggle("VSync",s.display.vsync);
        toggle("Limit to display refresh",s.display.limitToDisplay,s.display.vsync);
        result.push_back({"Pixel scale",s.display.integerScale==0?"Auto ("+std::to_string(maximumScale(app))+"x)":std::to_string(s.display.integerScale)+"x"});
        result.push_back({"Background frame rate",std::to_string(s.display.backgroundFps)+" FPS"});
        toggle("Pause when unfocused",s.display.pauseUnfocused);
#ifdef __EMSCRIPTEN__
        result[0].value="Browser";
        result[1].value="Browser canvas";
        result[3].value="Browser controlled";
        result[4].value="Browser controlled";
        result[8].value="Browser controlled";
        for(const int index:{0,1,2,3,4,5,6,8})result[std::size_t(index)].disabled=true;
#endif
    }else if(app.settingsTabIndex()==1){
        constexpr const char* quality[]{"Low","Medium","High"};result.push_back({"Effect quality",quality[std::clamp(s.render.quality,0,2)]});
        toggle("Particles",s.render.particles);toggle("Damage numbers",s.render.damageNumbers);toggle("Weapon effects",s.render.weaponEffects);
        toggle("FPS counter",s.render.showFps);toggle("Minimap",s.render.minimap);
    }else if(app.settingsTabIndex()==2){
        toggle("Sound",s.audio.enabled);slider("Master volume",s.audio.master);slider("Game effects",s.audio.effects);slider("Combat effects",s.audio.combat);slider("Interface / rewards",s.audio.interfaceVolume);
    }else{
        for(std::size_t i=0;i<ActionCount;++i)result.push_back({actionName(Action(i)),keyName(s.controls.keys[i])+(Action(i)==Action::Pause&&s.controls.keys[i]==KEY_ESCAPE?" / P":Action(i)==Action::Codex&&s.controls.keys[i]==KEY_TAB?" / F2":""),Kind::Key});
        toggle("Arrow key alternatives",s.controls.alternateArrows);
    }
    return result;
}
std::string bindingConflict(const ControlSettings& controls){
    for(std::size_t a=0;a<ActionCount;++a)for(std::size_t b=a+1;b<ActionCount;++b)if(controls.keys[a]==controls.keys[b])
        return keyName(controls.keys[a])+" is assigned to both "+actionName(Action(a))+" and "+actionName(Action(b))+".";
    for(std::size_t i=0;i<ActionCount;++i){
        if(Action(i)!=Action::Pause&&controls.keys[std::size_t(Action::Pause)]==KEY_ESCAPE&&controls.keys[i]==KEY_P)
            return "P is the alternate Pause key. Rebind Pause before using P.";
        if(Action(i)!=Action::Codex&&controls.keys[std::size_t(Action::Codex)]==KEY_TAB&&controls.keys[i]==KEY_F2)
            return "F2 opens the codex. Rebind Evolution Menu before using F2.";
        constexpr int arrows[]{KEY_UP,KEY_DOWN,KEY_LEFT,KEY_RIGHT};
        if(controls.alternateArrows)for(std::size_t direction=0;direction<4;++direction)if(controls.keys[i]==arrows[direction]&&i!=direction)
            return "Arrow keys are movement alternatives. Disable alternatives to rebind.";
    }
    return {};
}
}

void Presentation::openSettings(){
    settingsDraft=settings;settingsOpen=true;settingsTab=0;settingsFocus=0;rebindingAction=-1;settingsWasConfirming=false;settingsMessage.clear();resetAudio();
}
std::string Presentation::controlName(Action action) const {return keyName(settings.controls.keys[std::size_t(action)]);}
void Presentation::closeSettings(){
    if(pending(*this)&&onRevertSettings)onRevertSettings();
    settingsDraft=settings;settingsOpen=false;rebindingAction=-1;settingsWasConfirming=false;clicked=false;
}

void Presentation::updateSettings(float){
    const bool confirming=pending(*this);
    if(settingsWasConfirming&&!confirming){settingsDraft=settings;settingsMessage="Display settings reverted.";}
    settingsWasConfirming=confirming;
    if(confirming){
        const bool keep=IsKeyPressed(KEY_ENTER)||(clicked&&within(mouse,236,288,152,28));
        const bool revert=IsKeyPressed(KEY_ESCAPE)||(clicked&&within(mouse,412,288,152,28));
        if(keep){settingsMessage="Settings saved.";if(onConfirmSettings)onConfirmSettings();settingsDraft=settings;settingsWasConfirming=false;}
        else if(revert){settingsMessage="Display settings reverted.";if(onRevertSettings)onRevertSettings();settingsDraft=settings;settingsWasConfirming=false;}
        return;
    }
    if(rebindingAction>=0){
        const int key=GetKeyPressed();
        if(key==KEY_ESCAPE){rebindingAction=-1;settingsMessage.clear();}
        else if(key>0){
            if(validBindingKey(key)){settingsDraft.controls.keys[std::size_t(rebindingAction)]=key;rebindingAction=-1;settingsMessage=bindingConflict(settingsDraft.controls);}
            else settingsMessage="Choose a keyboard key.";
        }
        return;
    }
    if(IsKeyPressed(KEY_ESCAPE)||IsKeyPressed(settings.controls.keys[std::size_t(Action::Settings)])){closeSettings();return;}
    if(IsKeyPressed(KEY_TAB)){settingsTab=(settingsTab+(IsKeyDown(KEY_LEFT_SHIFT)||IsKeyDown(KEY_RIGHT_SHIFT)?3:1))%4;settingsFocus=0;settingsMessage.clear();}
    for(int tab=0;tab<4;++tab)if(clicked&&within(mouse,80+tab*160,78,150,25)){settingsTab=tab;settingsFocus=0;settingsMessage.clear();clicked=false;}
    auto options=rows(*this);const int count=int(options.size());
    if(IsKeyPressed(KEY_DOWN))settingsFocus=(settingsFocus+1)%(count+3);
    if(IsKeyPressed(KEY_UP))settingsFocus=(settingsFocus+count+2)%(count+3);
    auto change=[&](int index,int direction,bool directSlider){
        if(index<0||index>=count||options[std::size_t(index)].disabled)return;
        auto& s=settingsDraft;settingsMessage.clear();
        if(settingsTab==0){switch(index){
        case 0:{int mode=int(s.display.mode);cycle(mode,{0,1,2},direction);s.display.mode=WindowMode(mode);break;}
        case 1:s.display.monitor=(s.display.monitor+direction+monitorCount())%monitorCount();s.display.refreshRate=0;break;
        case 2:{const auto choices=sizes(*this);int selected=0;for(std::size_t i=0;i<choices.size();++i)if(choices[i].width==s.display.width&&choices[i].height==s.display.height)selected=int(i);selected=(selected+direction+int(choices.size()))%int(choices.size());s.display.width=choices[std::size_t(selected)].width;s.display.height=choices[std::size_t(selected)].height;s.display.refreshRate=0;break;}
        case 3:cycle(s.display.refreshRate,rates(*this),direction);break;
        case 4:cycle(s.display.targetFps,{0,30,60,75,90,120,144,160,165,180,240,360,480},direction);break;
        case 5:s.display.vsync=!s.display.vsync;break;case 6:s.display.limitToDisplay=!s.display.limitToDisplay;break;
        case 7:s.display.integerScale=(s.display.integerScale+direction+maximumScale(*this)+1)%(maximumScale(*this)+1);break;
        case 8:cycle(s.display.backgroundFps,{5,15,30,60,120},direction);break;case 9:s.display.pauseUnfocused=!s.display.pauseUnfocused;break;}}
        else if(settingsTab==1){switch(index){case 0:s.render.quality=(s.render.quality+direction+3)%3;break;case 1:s.render.particles=!s.render.particles;break;case 2:s.render.damageNumbers=!s.render.damageNumbers;break;case 3:s.render.weaponEffects=!s.render.weaponEffects;break;case 4:s.render.showFps=!s.render.showFps;break;case 5:s.render.minimap=!s.render.minimap;break;}}
        else if(settingsTab==2){
            if(index==0)s.audio.enabled=!s.audio.enabled;
            else{float* sliders[]{&s.audio.master,&s.audio.effects,&s.audio.combat,&s.audio.interfaceVolume};float& value=*sliders[index-1];value=directSlider?std::clamp((mouse.x-float(ControlX+24))/float(ControlW-100),0.0f,1.0f):std::clamp(value+float(direction)*.05f,0.0f,1.0f);value=std::round(value*100)/100;}
        }else if(index<int(ActionCount)){rebindingAction=index;settingsMessage="Press a key for "+std::string(actionName(Action(index)))+". Escape cancels.";}
        else s.controls.alternateArrows=!s.controls.alternateArrows;
    };
    int direction=IsKeyPressed(KEY_LEFT)?-1:IsKeyPressed(KEY_RIGHT)?1:0;
    if(direction&&settingsFocus<count)change(settingsFocus,direction,false);
    if(IsKeyPressed(KEY_ENTER)&&!IsKeyDown(KEY_LEFT_CONTROL)&&settingsFocus<count)change(settingsFocus,1,false);
    for(int i=0;i<count;++i)if(clicked&&within(mouse,ControlX,RowY+i*RowPitch,ControlW,RowH)){settingsFocus=i;change(i,mouse.x<ControlX+25?-1:1,options[std::size_t(i)].kind==Kind::Slider);clicked=false;}
    auto apply=[&]{
        const std::string conflict=bindingConflict(settingsDraft.controls);if(!conflict.empty()){settingsMessage=conflict;return;}
        std::string feedback;
        if(onApplySettings){if(!onApplySettings(settingsDraft,feedback)){settingsMessage=feedback.empty()?"Settings could not be applied.":feedback;return;}}
        else settings=settingsDraft;
        settingsDraft=settings;sound=settings.audio.enabled;if(!sound)resetAudio();
        settingsWasConfirming=pending(*this);settingsMessage=settingsWasConfirming?"Keep the new display settings?":feedback.empty()?"Settings saved.":feedback;
    };
    const bool enter=IsKeyPressed(KEY_ENTER);
    if((enter&&settingsFocus==count)||(IsKeyDown(KEY_LEFT_CONTROL)&&enter)||(clicked&&within(mouse,106,434,176,26)))apply();
    else if((enter&&settingsFocus==count+1)||(clicked&&within(mouse,312,434,176,26)))closeSettings();
    else if((enter&&settingsFocus==count+2)||(clicked&&within(mouse,518,434,176,26))){settingsDraft=defaultSettings();settingsMessage="Defaults selected. Apply to save.";}
}

void Presentation::drawSettings(){
    DrawRectangle(-(viewWidth-CanvasWidth)/2,-(viewHeight-CanvasHeight)/2,viewWidth,viewHeight,{0,0,0,225});DrawRectangle(PanelX,PanelY,PanelW,PanelH,{12,12,12,255});outline(PanelX,PanelY,PanelW,PanelH,Dim);
    centered("SETTINGS",CanvasWidth/2,48,White,2);
    constexpr const char* tabs[]{"DISPLAY","GRAPHICS","AUDIO","CONTROLS"};
    for(int tab=0;tab<4;++tab){const int x=80+tab*160;const bool active=tab==settingsTab;outline(x,78,150,25,active?Accent:Dim);centered(tabs[tab],x+75,86,active?Accent:Muted);}
    const auto options=rows(*this);
    for(std::size_t i=0;i<options.size();++i){
        const auto& row=options[i];const int y=RowY+int(i)*RowPitch;const bool selected=int(i)==settingsFocus;
        const Color text=row.disabled?Disabled:selected?White:Muted;
        if(selected)DrawRectangle(RowX-7,y-2,ControlX+ControlW-RowX+13,RowH+4,{25,25,25,255});
        label(row.label,RowX,y+6,text);outline(ControlX,y,ControlW,RowH,row.disabled?Color{35,35,35,255}:selected?Accent:Dim);
        if(row.kind==Kind::Slider){
            const int width=ControlW-100;DrawRectangle(ControlX+24,y+9,width,3,{50,50,50,255});DrawRectangle(ControlX+24,y+9,int(std::lround(row.slider*width)),3,Accent);DrawRectangle(ControlX+22+int(std::lround(row.slider*width)),y+4,4,13,White);centered(row.value,ControlX+ControlW-38,y+6,text);
        }else{label("<",ControlX+8,y+6,row.disabled?Disabled:Accent);centered(rebindingAction==int(i)&&settingsTab==3?"Press a key...":row.value,ControlX+ControlW/2,y+6,text);label(">",ControlX+ControlW-14,y+6,row.disabled?Disabled:Accent);}
    }
    std::string message=settingsMessage;
    if(message.empty()&&settingsTab==3)message="Enter to bind. Esc cancels binding.";
    wrappedCentered(message,CanvasWidth/2,403,PanelW-50,Accent,2,10);
    constexpr const char* buttons[]{"APPLY","CANCEL","DEFAULTS"};const int count=int(options.size());
    for(int i=0;i<3;++i){const int x=106+i*206;const bool selected=settingsFocus==count+i;const bool hover=within(mouse,x,434,176,26);outline(x,434,176,26,selected||hover?Accent:Dim);centered(buttons[i],x+88,443,selected||hover?White:Muted);}
    centered("Tab Page  Up/Down Select  Left/Right Change  Enter Confirm  Esc Cancel",CanvasWidth/2,480,Muted);
    if(pending(*this)){
        DrawRectangle(-(viewWidth-CanvasWidth)/2,-(viewHeight-CanvasHeight)/2,viewWidth,viewHeight,{0,0,0,210});DrawRectangle(175,178,450,156,{16,16,16,255});outline(175,178,450,156,Accent);
        centered("KEEP DISPLAY SETTINGS?",CanvasWidth/2,196,White,2);
        const float seconds=displayConfirmationSeconds?displayConfirmationSeconds():15;
        centered("Reverting in "+std::to_string(std::max(0,int(std::ceil(seconds))))+" seconds",CanvasWidth/2,237,Muted);
        outline(236,288,152,28,Accent);centered("ENTER - KEEP",312,298,White);
        outline(412,288,152,28,Dim);centered("ESC - REVERT",488,298,Muted);
    }
}
}
