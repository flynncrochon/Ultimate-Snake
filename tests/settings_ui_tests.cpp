#include "presentation.hpp"
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <string>

#ifndef SNAKE_TEST_DATA_DIR
#define SNAKE_TEST_DATA_DIR "data"
#endif
#ifndef SNAKE_TEST_ARTIFACT_DIR
#define SNAKE_TEST_ARTIFACT_DIR "artifacts/gametest"
#endif

namespace {
using namespace snake;
void require(bool condition,const char* message){if(!condition)throw std::runtime_error(message);}
void click(Presentation& app,float x,float y){app.update(.016f,{x,y},true);}
void capture(Presentation& app,RenderTexture2D target,const char* name){
    BeginTextureMode(target);ClearBackground(BLACK);app.draw();EndTextureMode();
    Image image=LoadImageFromTexture(target.texture);require(image.data!=nullptr,"Native settings framebuffer readback failed");ImageFlipVertical(&image);
    const auto path=std::filesystem::path(SNAKE_TEST_ARTIFACT_DIR)/name;std::filesystem::create_directories(path.parent_path());
    const bool written=ExportImage(image,path.string().c_str());UnloadImage(image);require(written,"Settings capture could not be written");
}
}

int main(){
    int passed=0,failed=0;RenderTexture2D target{};
    auto test=[&](const char* name,const std::function<void()>& body){try{body();++passed;std::cout<<"[PASS] "<<name<<'\n';}catch(const std::exception& error){++failed;std::cerr<<"[FAIL] "<<name<<": "<<error.what()<<'\n';}};
    try{
        SetTraceLogLevel(LOG_WARNING);SetConfigFlags(FLAG_WINDOW_HIDDEN);InitWindow(CanvasWidth,CanvasHeight,"Snake settings UI tests");
        require(IsWindowReady()&&IsWindowHidden(),"Hidden native window is unavailable");
        target=LoadRenderTexture(CanvasWidth,CanvasHeight);require(target.id!=0,"Native settings framebuffer creation failed");SetTextureFilter(target.texture,TEXTURE_FILTER_POINT);
        const auto catalog=Catalog::load(SNAKE_TEST_DATA_DIR);
        test("main menu settings opens without starting the game or leaking clicks",[&]{
            Presentation app(catalog);app.screen=Screen::Menu;
            click(app,720,476);app.draw();
            require(app.settingsOpen&&app.simulationBlocked()&&app.screen==Screen::Menu,"Main menu settings button did not open a blocking menu");
            const auto original=app.selectedCharacter;click(app,220,200);capture(app,target,"settings-display.png");
            require(app.settingsOpen&&app.screen==Screen::Menu&&app.selectedCharacter==original,"A settings click activated a character card behind the menu");
            require(!IsAudioDeviceReady(),"Settings initialized an audio device in a hidden test host");
        });
        test("display edits remain drafts until apply and cancel discards them",[&]{
            Presentation app(catalog);app.openSettings();
            const bool original=app.settings.display.vsync;
            click(app,535,244);require(app.settingsDraft.display.vsync!=original,"VSync row did not react to its actual mouse control");
            require(app.settings.display.vsync==original,"An unapplied VSync edit changed live settings");
            click(app,400,447);require(!app.settingsOpen&&app.settings.display.vsync==original,"Cancel committed a draft");
            app.openSettings();require(app.settingsDraft==app.settings,"Reopening restored a discarded draft");
        });
        test("apply preview supports revert keep and timeout without unpausing combat",[&]{
            Presentation app(catalog);app.screen=Screen::Game;app.game.phase=Phase::Paused;
            DisplayTrial trial(app.settings.display);int applied=0,confirmed=0,reverted=0;
            app.displayConfirmationPending=[&]{return trial.pending();};app.displayConfirmationSeconds=[&]{return trial.secondsRemaining();};
            app.onApplySettings=[&](const Settings& candidate,std::string&){trial.apply(candidate.display,true);app.settings=candidate;++applied;return true;};
            app.onRevertSettings=[&]{trial.revert();app.settings.display=trial.current();++reverted;};
            app.onConfirmSettings=[&]{trial.confirm();++confirmed;};
            app.openSettings();click(app,535,129);click(app,194,447);
            require(applied==1&&trial.pending()&&app.settingsOpen&&app.simulationBlocked(),"Window mode apply did not enter confirmation");
            capture(app,target,"settings-display-confirmation.png");
            click(app,488,302);require(reverted==1&&!trial.pending()&&app.settings.display.mode==WindowMode::Windowed,"Revert did not restore the old window mode");
            click(app,535,129);click(app,194,447);click(app,312,302);
            require(confirmed==1&&!trial.pending()&&app.settings.display.mode==WindowMode::Borderless,"Keep did not accept the preview");
            click(app,535,129);click(app,194,447);require(trial.pending(),"Second display change did not require confirmation");
            require(trial.advance(16),"Timed display preview did not expire");app.settings.display=trial.current();app.update(.01f,{},false);
            require(app.settingsDraft.display.mode==WindowMode::Borderless&&!trial.pending(),"Expired preview left the invalid draft selected");
            app.closeSettings();require(app.game.phase==Phase::Paused,"Closing settings resumed an already paused run");
        });
        test("audio sliders apply actual values and mute while graphics stay independent",[&]{
            Presentation app(catalog);app.openSettings();click(app,475,90);
            require(app.settingsTabIndex()==2,"Audio tab click did not select the audio page");
            click(app,509,152);require(std::abs(app.settingsDraft.audio.master-.5f)<.01f,"Master volume slider did not select 50 percent");
            click(app,535,129);require(!app.settingsDraft.audio.enabled&&app.sound,"Draft mute changed live audio before apply");
            click(app,194,447);require(!app.settings.audio.enabled&&!app.sound&&std::abs(app.settings.audio.master-.5f)<.01f,"Apply did not commit sound settings");
            capture(app,target,"settings-audio.png");
            click(app,315,90);click(app,535,129);require(app.settingsDraft.render.quality==0,"Graphics quality row did not cycle to low");
            click(app,194,447);require(app.settings.render.quality==0&&!app.settings.audio.enabled,"Graphics apply unexpectedly restored audio settings");
            capture(app,target,"settings-graphics.png");
        });
        test("mute remains immediate when saving settings fails",[&]{
            Presentation app(catalog);const Settings original=app.settings;int attempts=0;
            app.onApplySettings=[&](const Settings&,std::string& error){++attempts;app.settings=original;app.sound=original.audio.enabled;error="Settings file is read-only.";return false;};
            app.toggleSound();require(!app.sound&&!app.settings.audio.enabled,"A failed save prevented mute or restored the old sound flag");
            app.toggleSound();require(app.sound&&app.settings.audio.enabled&&attempts==2,"Sound could not be re-enabled after a failed save");
        });
        test("controls reject conflicting apply and defaults remain an explicit draft",[&]{
            Presentation app(catalog);app.openSettings();click(app,635,90);require(app.settingsTabIndex()==3,"Controls tab did not open");
            int applied=0;app.onApplySettings=[&](const Settings& candidate,std::string&){app.settings=candidate;++applied;return true;};
            app.settingsDraft.controls.keys[std::size_t(Action::Pause)]=app.settingsDraft.controls.keys[std::size_t(Action::Up)];
            click(app,194,447);require(applied==0&&app.settings.controls.keys[std::size_t(Action::Pause)]==KEY_ESCAPE,"Conflicting keys were applied");
            click(app,606,447);require(app.settingsDraft==defaultSettings()&&applied==0,"Defaults were not selected as an unapplied draft");
            click(app,194,447);require(applied==1,"Valid default controls could not apply");
            capture(app,target,"settings-controls.png");
        });
    }catch(const std::exception& error){++failed;std::cerr<<"[FAIL] setup: "<<error.what()<<'\n';}
    if(target.id)UnloadRenderTexture(target);if(IsWindowReady())CloseWindow();
    std::cout<<passed<<" passed, "<<failed<<" failed.\n";return failed?1:0;
}
