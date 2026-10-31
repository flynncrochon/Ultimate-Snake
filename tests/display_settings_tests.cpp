#include "display_settings.hpp"
#include <raylib.h>
#include <rlgl.h>
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <algorithm>
#include <functional>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
struct Workarea {int x=0,y=0,width=0,height=0;};
Workarea workarea(int monitor=0){
    int count=0;auto** all=glfwGetMonitors(&count);require(monitor<count,"Requested monitor is unavailable");
    Workarea area;glfwGetMonitorWorkarea(all[monitor],&area.x,&area.y,&area.width,&area.height);return area;
}
void requireFrameInsideWorkarea(int monitor=0){
    const auto area=workarea(monitor);int left=0,top=0,right=0,bottom=0;
    glfwGetWindowFrameSize(glfwGetCurrentContext(),&left,&top,&right,&bottom);
    const auto position=GetWindowPosition();
    std::cout<<"  client "<<int(position.x)<<','<<int(position.y)<<' '<<GetScreenWidth()<<'x'<<GetScreenHeight()
        <<", frame "<<left<<','<<top<<','<<right<<','<<bottom
        <<", workarea "<<area.x<<','<<area.y<<' '<<area.width<<'x'<<area.height<<'\n';
    require(int(position.x)-left>=area.x,"Window's left border is off-screen");
    require(int(position.y)-top>=area.y,"Window's title bar is off-screen");
    require(int(position.x)+GetScreenWidth()+right<=area.x+area.width,"Window's right border or close button is off-screen");
    require(int(position.y)+GetScreenHeight()+bottom<=area.y+area.height,"Window's bottom border overlaps the taskbar");
}
void requirePresentedPixels(const snake::DisplaySettings& settings,int monitor,const char* label="startup-display"){
    int nativeWidth=0,nativeHeight=0;glfwGetFramebufferSize(glfwGetCurrentContext(),&nativeWidth,&nativeHeight);
    std::cout<<"  screen "<<GetScreenWidth()<<'x'<<GetScreenHeight()<<", render "<<GetRenderWidth()<<'x'<<GetRenderHeight()
        <<", framebuffer "<<nativeWidth<<'x'<<nativeHeight<<'\n';
    const Color fill{37,73,109,255},stripe{181,127,53,255};
    auto canvas=LoadRenderTexture(800,500);require(canvas.id!=0,"Presentation test framebuffer unavailable");
    SetTextureFilter(canvas.texture,TEXTURE_FILTER_POINT);
    BeginTextureMode(canvas);ClearBackground(fill);DrawRectangle(123,0,1,500,stripe);EndTextureMode();
    const int scale=snake::canvasScale(settings,GetScreenWidth(),GetScreenHeight());
    const int x=(GetScreenWidth()-800*scale)/2,y=(GetScreenHeight()-500*scale)/2;
    BeginDrawing();ClearBackground(BLACK);
    DrawTexturePro(canvas.texture,{0,0,800,-500},{float(x),float(y),float(800*scale),float(500*scale)},{0,0},0,WHITE);
    // raylib's LoadImageFromScreen multiplies by desktop DPI even when the
    // window does not request HighDPI. Read the actual framebuffer dimensions.
    rlDrawRenderBatchActive();Image image{rlReadScreenPixels(nativeWidth,nativeHeight),nativeWidth,nativeHeight,1,PIXELFORMAT_UNCOMPRESSED_R8G8B8A8};
    EndDrawing();UnloadRenderTexture(canvas);
    require(image.data!=nullptr,"Presented window readback failed");
    const auto path=std::filesystem::path(SNAKE_TEST_ARTIFACT_DIR)/(std::string(label)+"-"+std::to_string(monitor)+".png");
    std::filesystem::create_directories(path.parent_path());const bool written=ExportImage(image,path.string().c_str());
    const auto matches=[&](int px,int py,Color expected){const auto actual=GetImageColor(image,px,py);return actual.r==expected.r&&actual.g==expected.g&&actual.b==expected.b&&actual.a==expected.a;};
    bool pixels=image.width==800*scale&&image.height==500*scale;
    if(pixels){
        pixels=matches(0,0,fill)&&matches(image.width-1,0,fill)&&matches(0,image.height-1,fill)&&matches(image.width-1,image.height-1,fill);
        for(int px=122*scale;px<125*scale;++px)pixels=pixels&&matches(px,image.height/2,px>=123*scale&&px<124*scale?stripe:fill);
    }
    UnloadImage(image);require(written,"Startup display capture could not be saved");
    require(pixels,"Oversized startup left unnecessary borders or changed the integer source pixels");
}
}
int main(){
    using namespace snake;int passed=0,failed=0;
    auto test=[&](const char* name,const std::function<void()>& body){try{body();++passed;std::cout<<"PASS "<<name<<'\n';}catch(const std::exception& ex){++failed;std::cerr<<"FAIL "<<name<<": "<<ex.what()<<'\n';}};
    SetTraceLogLevel(LOG_WARNING);SetConfigFlags(FLAG_WINDOW_HIDDEN|FLAG_WINDOW_RESIZABLE);InitWindow(800,500,"Display settings regression");SetTargetFPS(0);
    if(!IsWindowReady()||!IsWindowHidden()){std::cerr<<"Hidden OpenGL window unavailable\n";return 1;}
    test("hidden deterministic adapter exposes display metadata without changing the window",[]{
        const int width=GetScreenWidth(),height=GetScreenHeight();DisplayManager manager(false);DisplaySettings settings;settings.mode=WindowMode::Fullscreen;
        require(manager.initialize(settings),"Disabled adapter could not initialize metadata");require(!manager.monitors().empty(),"No monitor metadata was published");
        require(GetScreenWidth()==width&&GetScreenHeight()==height&&!IsWindowFullscreen()&&IsWindowHidden(),"A deterministic host changed its display mode");
        const auto& monitor=manager.monitors().front();require(monitor.width>0&&monitor.height>0&&monitor.refreshRate>0,"Monitor metadata is invalid");
        const auto sizes=manager.windowSizes(0);require(!sizes.empty()&&std::any_of(sizes.begin(),sizes.end(),[](const auto& mode){return mode.width==800&&mode.height==500;}),"Minimum pixel-perfect window size is unavailable");
        require(!manager.refreshRates(0).empty()&&manager.refreshRates(0).front()==0,"Desktop refresh option is missing");
        auto next=settings;next.mode=WindowMode::Windowed;next.width=960;next.height=600;require(manager.apply(next)&&manager.confirmationPending(),"Mode preview did not await confirmation");
        require(manager.update(15)&&!manager.confirmationPending()&&manager.current().mode==WindowMode::Fullscreen,"Disabled adapter timeout failed to revert pure state");
        require(GetScreenWidth()==width&&GetScreenHeight()==height&&!IsWindowFullscreen()&&IsWindowHidden(),"Preview controls changed a hidden host's window");
    });
    test("windowed size preview applies and cancel restores the original hidden client",[]{
        DisplayManager manager;DisplaySettings initial;initial.width=800;initial.height=500;initial.vsync=false;
        require(manager.initialize(initial),"Native display adapter initialization failed");
        auto next=initial;next.width=960;next.height=600;require(manager.apply(next)&&manager.confirmationPending(),"Window size preview did not start");
        require(GetScreenWidth()==960&&GetScreenHeight()==600,"Native client size was not applied");
        require(manager.revert()&&!manager.confirmationPending()&&GetScreenWidth()==800&&GetScreenHeight()==500,"Cancel failed to restore native client dimensions");
        require(IsWindowHidden()&&!IsAudioDeviceReady(),"Display preview exposed the test window or opened audio");
    });
    test("oversized startup fits the complete decorated window on each monitor",[]{
        int count=0;glfwGetMonitors(&count);
        for(int monitor=0;monitor<count;++monitor){
            const auto area=workarea(monitor);SetWindowPosition(area.x,area.y);
            DisplayManager manager;DisplaySettings initial;initial.monitor=monitor;initial.width=7680;initial.height=4320;initial.vsync=false;
            require(manager.initialize(initial),"Oversized window initialization failed");requireFrameInsideWorkarea(monitor);
            int left=0,top=0,right=0,bottom=0;glfwGetWindowFrameSize(glfwGetCurrentContext(),&left,&top,&right,&bottom);
            const int scale=std::max(1,std::min((area.width-left-right)/800,(area.height-top-bottom)/500));
            requirePresentedPixels(manager.current(),monitor);
            require(GetScreenWidth()==800*scale&&GetScreenHeight()==500*scale,"Window fitting used frame dimensions from the wrong monitor DPI");
            require(manager.current().width==GetScreenWidth()&&manager.current().height==GetScreenHeight(),"Startup settings do not match the fitted client size");
            require(IsWindowHidden(),"Fitting the window exposed the hidden test host");
        }
    });
    test("resizing at the desktop edges keeps the title bar and close button visible",[]{
        DisplayManager manager;DisplaySettings initial;initial.width=800;initial.height=500;initial.vsync=false;
        require(manager.initialize(initial),"Windowed display initialization failed");
        requireFrameInsideWorkarea();
        const auto area=workarea();SetWindowPosition(area.x+area.width-800,area.y+area.height-500);
        auto next=initial;next.width=7680;next.height=4320;
        require(manager.apply(next)&&manager.confirmationPending(),"Oversized window preview did not start");requireFrameInsideWorkarea();
        require(manager.current().width==GetScreenWidth()&&manager.current().height==GetScreenHeight(),"Preview settings do not match the fitted client size");
        require(manager.revert(),"Oversized window preview could not be reverted");requireFrameInsideWorkarea();
        SetWindowPosition(area.x-100,area.y-100);next=manager.current();next.width=960;next.height=600;
        require(manager.apply(next)&&manager.confirmationPending(),"Off-screen window resize did not start");requireFrameInsideWorkarea();
        require(manager.revert()&&IsWindowHidden(),"Off-screen resize did not restore a hidden window");
    });
    test("VSync follows focus and does not conflict with a software frame cap",[]{
        DisplayManager manager;DisplaySettings initial;initial.width=800;initial.height=500;initial.vsync=true;initial.targetFps=90;
        require(manager.initialize(initial)&&IsWindowState(FLAG_VSYNC_HINT),"VSync was not enabled");
        manager.updateFrameLimit(false,false);require(!IsWindowState(FLAG_VSYNC_HINT),"Background frame limiting retained competing VSync");
        manager.updateFrameLimit(true,false);require(IsWindowState(FLAG_VSYNC_HINT),"Foreground focus did not restore VSync");
        auto uncapped=initial;uncapped.vsync=false;uncapped.targetFps=0;uncapped.limitToDisplay=false;
        require(manager.apply(uncapped)&&!manager.confirmationPending()&&!IsWindowState(FLAG_VSYNC_HINT),"Frame pacing changes failed or unnecessarily requested mode confirmation");
        require(IsWindowHidden(),"Frame pacing changed the window visibility");
    });
    test("hidden borderless preview restores the original windowed client on revert",[]{
        DisplayManager manager;DisplaySettings initial;initial.width=960;initial.height=600;initial.vsync=false;
        require(manager.initialize(initial),"Windowed display initialization failed");
        const int width=GetScreenWidth(),height=GetScreenHeight();
        const auto before=manager.current();auto borderless=before;borderless.mode=WindowMode::Borderless;
        require(manager.apply(borderless)&&manager.confirmationPending(),"Borderless preview did not await confirmation");
        require(IsWindowState(FLAG_BORDERLESS_WINDOWED_MODE)&&!IsWindowFullscreen(),"Borderless preview entered the wrong native mode");
        require(IsWindowHidden(),"Borderless preview exposed the hidden test window");
        require(manager.current().width==before.width&&manager.current().height==before.height,"Borderless preview discarded the remembered windowed size");
        require(manager.revert()&&!manager.confirmationPending()&&manager.current()==before,"Borderless rollback lost committed display settings");
        require(!IsWindowState(FLAG_BORDERLESS_WINDOWED_MODE)&&!IsWindowFullscreen()&&IsWindowHidden(),"Rollback did not restore hidden windowed flags");
        require(GetScreenWidth()==width&&GetScreenHeight()==height,"Borderless rollback failed to restore the original windowed dimensions");
        requireFrameInsideWorkarea();
    });
    test("fullscreen rollback restores the complete decorated window inside the desktop",[]{
        DisplayManager manager;DisplaySettings initial;initial.width=7680;initial.height=4320;initial.vsync=false;
        require(manager.initialize(initial),"Windowed display initialization failed");requireFrameInsideWorkarea();
        const auto before=manager.current();auto fullscreen=before;fullscreen.mode=WindowMode::Fullscreen;
        require(manager.apply(fullscreen)&&manager.confirmationPending()&&IsWindowFullscreen(),"Fullscreen preview did not enter the requested mode");
        require(manager.revert()&&!IsWindowFullscreen()&&!manager.confirmationPending(),"Fullscreen rollback failed");
        require(manager.current()==before&&GetScreenWidth()==before.width&&GetScreenHeight()==before.height,"Fullscreen rollback lost the fitted windowed size");
        requireFrameInsideWorkarea();require(IsWindowHidden(),"Fullscreen rollback exposed the test host");
    });
    test("fresh default and saved windows present fitted pixels on their first frame",[]{
        const int count=GetMonitorCount();
        for(int monitor=0;monitor<count;++monitor){
            for(const auto size:{DisplayMode{1600,1000,0},DisplayMode{3840,2019,0}}){
                CloseWindow();SetConfigFlags(FLAG_WINDOW_HIDDEN|FLAG_WINDOW_RESIZABLE);
                InitWindow(size.width,size.height,"Fresh startup display regression");SetTargetFPS(0);
                require(IsWindowReady()&&IsWindowHidden(),"Fresh startup exposed a test window or failed to initialize");
                DisplayManager manager;DisplaySettings initial;initial.monitor=monitor;initial.width=size.width;initial.height=size.height;initial.vsync=false;
                require(manager.initialize(initial),"Fresh display initialization failed");requireFrameInsideWorkarea(monitor);
                requirePresentedPixels(manager.current(),monitor,size.width==1600?"startup-default":"startup-saved");
                require(IsWindowHidden()&&!IsAudioDeviceReady(),"Startup capture exposed the test host or opened audio");
            }
        }
    });
    if(IsWindowReady()){ClearWindowState(FLAG_VSYNC_HINT);SetTargetFPS(0);CloseWindow();}
    std::cout<<passed<<" passed, "<<failed<<" failed.\n";return failed?1:0;
}
