#include "presentation.hpp"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <thread>
#include <vector>

namespace {
using namespace snake;
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
struct Files {
    std::filesystem::path folder,path,temporary;
    Files(){folder=std::filesystem::path(SNAKE_TEST_ARTIFACT_DIR)/"permanent"/
        std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
        std::filesystem::create_directories(folder);path=folder/"profile.save";temporary=path;temporary+=".tmp";}
    ~Files(){std::error_code error;std::filesystem::remove(temporary,error);std::filesystem::remove(path,error);std::filesystem::remove(folder,error);}
    void put(const Profile& profile){std::ofstream file(path);writeProfile(file,profile);require(bool(file),"Profile fixture could not be written");}
};
void key(Presentation& app,int value,Vec cursor={0,0}){
    PlayAutomationEvent({0,2,{value,0,0,0}});app.update(FixedStep,cursor,false);PlayAutomationEvent({0,1,{value,0,0,0}});
}
std::vector<Color> frame(Presentation& app,RenderTexture2D target,const char* filename=nullptr){
    BeginTextureMode(target);ClearBackground(BLACK);app.draw();EndTextureMode();
    BeginDrawing();ClearBackground(BLACK);DrawTextureRec(target.texture,{0,0,float(CanvasWidth),-float(CanvasHeight)},{0,0},WHITE);EndDrawing();
    if(!filename)return {};
    Image image=LoadImageFromTexture(target.texture);require(image.data!=nullptr,"Readback failed");ImageFlipVertical(&image);
    const auto path=std::filesystem::path(SNAKE_TEST_ARTIFACT_DIR)/"permanent"/filename;
    std::filesystem::create_directories(path.parent_path());const bool saved=ExportImage(image,path.string().c_str());
    Color* colors=LoadImageColors(image);require(colors!=nullptr,"Pixel extraction failed");
    std::vector<Color> pixels(colors,colors+CanvasWidth*CanvasHeight);UnloadImageColors(colors);UnloadImage(image);
    require(saved,"Capture write failed");return pixels;
}
void waitForSave(Presentation& app){
    const auto end=std::chrono::steady_clock::now()+std::chrono::seconds(3);
    while(app.profileSaveState()==ProfileSaveState::Saving && std::chrono::steady_clock::now()<end)std::this_thread::yield();
    require(app.profileSaveState()!=ProfileSaveState::Saving,"Background save did not complete");
}
void isolate(Presentation& app){
    auto& game=app.game;game.enemies.clear();game.pickups.clear();game.owned.clear();
    game.greySnakes.enabled=false;game.flood.enabled=false;game.spawnClock=-1000000;game.bossClock=1000000;
    game.player.moveClock=-1000000;game.player.invulnerable=1000000;app.screen=Screen::Game;
    for(int tick=0;tick<10;++tick)game.update(FixedStep,{});app.setRenderFrame(1,false);
}
}
int main(){
    int passed=0,failed=0;RenderTexture2D target{};
    const auto test=[&](const char* name,const std::function<void()>& run){try{run();++passed;std::cout<<"PASS "<<name<<'\n';}catch(const std::exception& error){++failed;std::cerr<<"FAIL "<<name<<": "<<error.what()<<'\n';}};
    try{
        SetTraceLogLevel(LOG_WARNING);SetConfigFlags(FLAG_WINDOW_HIDDEN);InitWindow(CanvasWidth,CanvasHeight,"Permanent upgrade tests");SetTargetFPS(0);
        require(IsWindowReady() && IsWindowHidden(),"Hidden native context unavailable");
        target=LoadRenderTexture(CanvasWidth,CanvasHeight);require(target.id!=0,"Framebuffer unavailable");
        const auto catalog=Catalog::load(SNAKE_TEST_DATA_DIR);
        test("keyboard and mouse purchases affect the next run and repeated draws cannot buy",[&]{
            Files files;Profile profile;profile.gold=10000;files.put(profile);
            {
                Presentation app(catalog,defaultSettings(),files.path.string(),true);
                key(app,KEY_U);require(app.permanentUpgradesOpen(),"U did not open permanent upgrades");frame(app,target);
                key(app,KEY_ENTER);frame(app,target,"purchased.png");
                require(app.currentProfile().gold==9800 && app.currentProfile().upgrades.rank(PermanentUpgrade::Might)==1,"Keyboard purchase failed");
                for(int draw=0;draw<5;++draw)frame(app,target);
                require(app.currentProfile().gold==9800,"Rendering repeated the purchase");
                app.update(FixedStep,{140,160},true);frame(app,target);
                require(app.currentProfile().upgrades.rank(PermanentUpgrade::Vitality)==1,"Mouse purchase selected wrong row");
                app.closePermanentUpgrades();app.startRun(123);
                require(app.game.player.maxHp==110 && app.game.stats().damage>1.2,"Next run did not inherit purchases");
                require(!app.purchaseUpgrade(PermanentUpgrade::Might),"Purchase changed an active run");
            }
            ProfileStorage reopened(files.path.string());require(reopened.loaded().value.upgrades.rank(PermanentUpgrade::Might)==1 && reopened.loaded().value.upgrades.rank(PermanentUpgrade::Vitality)==1,"Closing/reopening lost purchases");
        });
        test("gold is banked once for death victory and leaving a run",[&]{
            Files files;Profile profile;files.put(profile);
            {
                Presentation app(catalog,defaultSettings(),files.path.string(),true);app.startRun(1);
                app.game.gems=123;app.game.elapsed=75;app.game.phase=Phase::Dead;
                app.finishRun();app.finishRun();
                require(app.currentProfile().gold==123 && app.currentProfile().runs==1,"Death lost or duplicated earnings");
                app.screen=Screen::Game;frame(app,target,"results.png");
                app.startRun(2);app.game.gems=45;app.game.phase=Phase::Victory;app.finishRun();
                require(app.currentProfile().gold==168 && app.currentProfile().victories==1,"Victory did not bank rewards");
                app.startRun(3);app.game.gems=17;
            }
            ProfileStorage reopened(files.path.string());require(reopened.loaded().value.gold==185 && reopened.loaded().value.runs==3,"Shutdown did not bank an unfinished run exactly once");
        });
        test("full refunds persist and insufficient gold never changes ranks",[&]{
            Files files;Profile profile;profile.gold=200;files.put(profile);
            {
                Presentation app(catalog,defaultSettings(),files.path.string(),true);app.openPermanentUpgrades();
                require(app.purchaseUpgrade(PermanentUpgrade::Might) && !app.purchaseUpgrade(PermanentUpgrade::Might),"Unaffordable purchase was accepted");
                require(app.currentProfile().gold==0 && app.currentProfile().upgrades.rank(PermanentUpgrade::Might)==1,"Failed purchase changed state");
                require(app.refundUpgrades() && app.currentProfile().gold==200 && !app.refundUpgrades(),"Refund lost or duplicated gold");
                frame(app,target,"refunded.png");
            }
            ProfileStorage reopened(files.path.string());require(reopened.loaded().value.gold==200 && reopened.loaded().value.upgrades.totalRanks()==0,"Refund did not survive reopening");
        });
        test("maximum permanent upgrades keep gameplay pixels identical",[&]{
            Files files;Profile profile;profile.gold=1000000;
            for(std::size_t i=0;i<PermanentUpgradeCount;++i)for(int rank=0;rank<permanentDefinitions()[i].maxRank;++rank)require(purchasePermanent(profile,PermanentUpgrade(i)),"Maximum fixture purchase failed");
            files.put(profile);
            Presentation base(catalog),boosted(catalog,defaultSettings(),files.path.string(),false);
            boosted.openPermanentUpgrades();frame(boosted,target,"maximum.png");boosted.closePermanentUpgrades();
            base.startRun(42);boosted.startRun(42);isolate(base);isolate(boosted);
            const auto before=frame(base,target,"plain-gameplay.png"),after=frame(boosted,target,"maximum-gameplay.png");
            require(std::equal(before.begin(),before.end(),after.begin(),[](Color a,Color b){return a.r==b.r && a.g==b.g && a.b==b.b && a.a==b.a;}),"Bonuses changed player appearance, HUD geometry, or scene pixels");
        });
        test("save failures show feedback and retry preserves the purchased rank",[&]{
            Files files;Profile profile;profile.gold=500;files.put(profile);std::filesystem::create_directory(files.temporary);
            {
                Presentation app(catalog,defaultSettings(),files.path.string(),true);app.openPermanentUpgrades();require(app.purchaseUpgrade(PermanentUpgrade::Might),"Failure fixture could not buy");
                waitForSave(app);require(app.profileSaveState()==ProfileSaveState::Failed,"Write failure was hidden");frame(app,target,"save-failed.png");
                std::filesystem::remove(files.temporary);app.retryProfileSave();
                // A retry keeps the previous error visible until the worker succeeds.
                const auto end=std::chrono::steady_clock::now()+std::chrono::seconds(3);
                while(app.profileSaveState()!=ProfileSaveState::Saved && std::chrono::steady_clock::now()<end)std::this_thread::yield();
                require(app.profileSaveState()==ProfileSaveState::Saved,"Retry failed after storage recovered");
            }
            ProfileStorage reopened(files.path.string());require(reopened.loaded().value.gold==300 && reopened.loaded().value.upgrades.rank(PermanentUpgrade::Might)==1,"Retry lost the purchased rank");
        });
        UnloadRenderTexture(target);target={};CloseWindow();
    }catch(const std::exception& error){std::cerr<<error.what()<<'\n';if(target.id)UnloadRenderTexture(target);if(IsWindowReady())CloseWindow();return 1;}
    std::cout<<passed<<" passed, "<<failed<<" failed.\n";return failed?1:0;
}
