#include "presentation.hpp"
#include "grid.hpp"
#include "chest_lottery.hpp"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#ifndef SNAKE_TEST_DATA_DIR
#define SNAKE_TEST_DATA_DIR "data"
#endif
#ifndef SNAKE_TEST_ARTIFACT_DIR
#define SNAKE_TEST_ARTIFACT_DIR "artifacts/gametest"
#endif

namespace {
using namespace snake;
void require(bool condition,const char* message){if(!condition)throw std::runtime_error(message);}
void isolate(Game& game){
    game.enemies.clear();game.projectiles.clear();game.zones.clear();game.pickups.clear();
    game.nestEggs.clear();game.miniSnakes.clear();game.beams.clear();game.cobraPits.clear();
    game.cobras.clear();game.grapples.clear();game.particles.clear();game.damageNumbers.clear();
    game.owned.clear();game.spawnClock=-1000;game.bossClock=10;
    game.player.invulnerable=1000;game.player.moveClock=-1000;
}
void prepare(Presentation& app,const Catalog& catalog,bool weapon=true){
    app.startRun(0xB055u);app.screen=Screen::Game;isolate(app.game);
    if(weapon)app.game.owned.push_back({catalog.find("venom_shot"),1,false,1000});
    app.setRenderFrame(1,false);
}
void step(Presentation& app){
    app.beginSimulationStep();app.game.update(FixedStep,{});app.queueAudioEvents();app.setRenderFrame(1,false);
}
void pressKey(Presentation& app,int key,Vec cursor){
    // raylib 5.5's automation event types are private to rcore.c: key up=1, down=2.
    PlayAutomationEvent({0,2,{key,0,0,0}});
    app.update(FixedStep,cursor,false);
    PlayAutomationEvent({0,1,{key,0,0,0}});
}
void drawFrame(Presentation& app,RenderTexture2D target){
    BeginTextureMode(target);ClearBackground(BLACK);app.draw();EndTextureMode();
}
Vec defeatBoss(Presentation& app){
    auto& game=app.game;Enemy boss;boss.id=0xB055u;boss.kind=EnemyKind::Boss;
    boss.position={game.player.position.x+64,game.player.position.y};boss.hp=boss.maxHp=1;
    boss.radius=7;boss.speed=0;boss.trailSize=8;boss.trail.fill(boss.position);game.enemies.push_back(boss);
    Projectile finisher;finisher.position=boss.position;finisher.damage=10000;finisher.life=1;
    game.projectiles.push_back(finisher);step(app);
    require(game.bossesKilled==1&&game.enemies.empty(),"Actual boss combat did not produce a defeat");
    const auto chest=std::find_if(game.pickups.begin(),game.pickups.end(),[](const Pickup& p){return p.kind==PickupKind::Chest;});
    require(chest!=game.pickups.end(),"Boss death did not drop a chest pickup");
    return chest->position;
}
void collectAt(Presentation& app,Vec position){
    auto& player=app.game.player;player.position=player.previous=position;
    std::fill(player.segments.begin(),player.segments.end(),position);player.previousSegments=player.segments;
    step(app);
}
std::vector<Color> capture(Presentation& app,RenderTexture2D target,const char* name,bool worldOnly=false,float chestPreviewTime=-1){
    BeginTextureMode(target);ClearBackground(BLACK);if(worldOnly)app.drawWorldOnly();else app.draw(chestPreviewTime);EndTextureMode();
    Image image=LoadImageFromTexture(target.texture);require(image.data!=nullptr,"Native framebuffer readback failed");
    ImageFlipVertical(&image);const auto path=std::filesystem::path(SNAKE_TEST_ARTIFACT_DIR)/name;
    std::filesystem::create_directories(path.parent_path());
    const bool written=ExportImage(image,path.string().c_str());
    Color* colors=LoadImageColors(image);if(!colors){UnloadImage(image);throw std::runtime_error("Native pixel readback failed");}
    std::vector<Color> pixels(colors,colors+std::size_t(image.width)*std::size_t(image.height));
    UnloadImageColors(colors);UnloadImage(image);require(written,"Native reward capture could not be written");
    return pixels;
}
}

int main(){
    int passed=0,failed=0;RenderTexture2D target{};
    auto test=[&](const char* name,const std::function<void()>& body){try{body();++passed;std::cout<<"[PASS] "<<name<<'\n';}catch(const std::exception& e){++failed;std::cerr<<"[FAIL] "<<name<<": "<<e.what()<<'\n';}};
    try{
        SetTraceLogLevel(LOG_WARNING);SetConfigFlags(FLAG_WINDOW_HIDDEN);InitWindow(CanvasWidth,CanvasHeight,"Snake presentation tests");
        require(IsWindowReady()&&IsWindowHidden(),"Hidden native OpenGL window is unavailable");
        require(!IsAudioDeviceReady(),"Hidden test process unexpectedly initialized audio");
        target=LoadRenderTexture(CanvasWidth,CanvasHeight);require(target.id!=0,"Native framebuffer creation failed");
        SetTextureFilter(target.texture,TEXTURE_FILTER_POINT);
        const auto catalog=Catalog::load(SNAKE_TEST_DATA_DIR);

        test("stationary upgrade hover preserves arrow selection and keyboard confirmation",[&]{
            for(const int direction:{KEY_LEFT,KEY_RIGHT}){
                Presentation app(catalog);prepare(app,catalog,false);app.game.grantXp(app.game.player.nextXp);
                require(app.game.phase==Phase::LevelUp&&app.game.offers[2]>=0,"Upgrade fixture did not offer three cards");
                const Vec cursor{400,200};app.update(FixedStep,cursor,false);drawFrame(app,target);
                const int expected=app.game.offers[direction==KEY_RIGHT?2:0];
                pressKey(app,direction,cursor);
                for(int frame=0;frame<3;++frame){drawFrame(app,target);app.update(FixedStep,cursor,false);}
                if(direction==KEY_RIGHT)capture(app,target,"upgrade-keyboard-stationary.png");
                pressKey(app,direction==KEY_RIGHT?KEY_ENTER:KEY_SPACE,cursor);
                require(app.game.phase==Phase::Playing&&app.game.owned.size()==1&&app.game.owned.front().definition==expected,
                    "A stationary mouse replaced the arrow-selected upgrade before keyboard confirmation");
            }
        });
        test("moving within an upgrade card restores hover and arrows take priority in the same frame",[&]{
            Presentation app(catalog);prepare(app,catalog,false);app.game.grantXp(app.game.player.nextXp);
            const Vec cursor{400,200};app.update(FixedStep,cursor,false);drawFrame(app,target);
            pressKey(app,KEY_RIGHT,cursor);drawFrame(app,target);
            // Move inside the same card, then press Right while moving once more.
            app.update(FixedStep,{401,200},false);drawFrame(app,target);
            pressKey(app,KEY_RIGHT,{402,200});drawFrame(app,target);
            const int expected=app.game.offers[2];
            pressKey(app,KEY_ENTER,{402,200});
            require(app.game.phase==Phase::Playing&&app.game.owned.size()==1&&app.game.owned.front().definition==expected,
                "Mouse movement did not restore hover or overrode keyboard input in the same frame");
        });
        test("stationary upgrade clicks still choose the pointed card after keyboard navigation",[&]{
            Presentation app(catalog);prepare(app,catalog,false);app.game.grantXp(app.game.player.nextXp);
            const Vec cursor{400,200};app.update(FixedStep,cursor,false);drawFrame(app,target);
            pressKey(app,KEY_RIGHT,cursor);drawFrame(app,target);
            const int expected=app.game.offers[1];
            app.update(FixedStep,cursor,true);drawFrame(app,target);
            require(app.game.phase==Phase::Playing&&app.game.owned.size()==1&&app.game.owned.front().definition==expected,
                "A stationary click did not choose the card under the cursor");
        });
        test("actual boss death leaves a visible collectable chest on the ground",[&]{
            Presentation app(catalog);prepare(app,catalog);const Vec chest=defeatBoss(app);
            require(app.game.chests==0&&!app.pendingChestReward()&&!app.simulationBlocked(),"Boss reward was granted before chest contact");
            require(!sameCell(chest,app.game.player.position),"Chest spawned beneath the player");
            const auto pixels=capture(app,target,"boss-loot-on-ground.png",true);
            const int x=CanvasWidth/2+int(std::lround((chest.x-app.game.player.position.x)*2.5f));
            const int y=CanvasHeight/2+int(std::lround((chest.y-app.game.player.position.y)*2.5f));
            int gold=0;
            for(int py=std::max(0,y-7);py<std::min(CanvasHeight,y+8);++py)for(int px=std::max(0,x-7);px<std::min(CanvasWidth,x+8);++px){
                const auto c=pixels[std::size_t(py*CanvasWidth+px)];if(c.r==255&&c.g==215&&c.b==0)++gold;
            }
            require(gold>=8,"Actual chest pickup has no visible gold sprite at its world position");
            require(!IsAudioDeviceReady(),"Constructing hidden presentation opened an audio device");
        });
        test("collected boss upgrade is revealed and continuing never grants it twice",[&]{
            Presentation app(catalog);prepare(app,catalog);collectAt(app,defeatBoss(app));
            const auto* reward=app.pendingChestReward();
            require(reward&&reward->kind==ChestRewardKind::Upgrade&&reward->rank==2,"Collected chest did not reveal its actual equipment upgrade");
            require(app.game.owned.front().rank==2&&app.game.chests==1&&app.simulationBlocked(),"Chest upgrade was not applied exactly once before its reveal");
            require(app.game.phase==Phase::LevelUp,"Boss XP fixture did not exercise simultaneous level and chest rewards");
            const int rank=app.game.owned.front().rank,gems=app.game.gems,chests=app.game.chests;
            const float hp=app.game.player.hp,elapsed=app.game.elapsed;
            const auto serial=reward->serial;
            app.update(.05f,{},true);require(app.pendingChestReward()&&app.pendingChestReward()->serial==serial,"Reveal consumed the click that opened it");
            app.update(.25f,{},false);capture(app,target,"boss-reward-upgrade.png");
            require(app.game.elapsed==elapsed,"Reward animation advanced the simulation");
            app.update(.001f,{},true);
            require(app.pendingChestReward()&&std::abs(app.chestRevealSeconds()-loot::RevealSeconds)<.0001f,"Skip input dismissed the reward instead of landing its reel");
            app.update(loot::ContinueDelay*.5f,{},true);
            require(app.pendingChestReward()!=nullptr,"An immediate second click dismissed the landed reward");
            app.update(loot::ContinueDelay,{},true);
            require(!app.pendingChestReward()&&!app.simulationBlocked()&&!app.acknowledgeChestReward(),"Reward acknowledgement did not clear exactly one reveal");
            require(app.game.owned.front().rank==rank&&app.game.gems==gems&&app.game.chests==chests&&app.game.player.hp==hp,"Continuing granted a second reward");
            require(app.game.phase==Phase::LevelUp,"Continuing discarded the pending level selection");
        });
        test("a real evolution chest renders its evolved weapon and survives transient event clearing",[&]{
            Presentation app(catalog);prepare(app,catalog);auto& game=app.game;
            game.owned.front().rank=8;game.owned.push_back({catalog.find("serpents_scales"),1,false,1000});
            collectAt(app,defeatBoss(app));game.events.clear();app.update(loot::RevealSeconds+loot::ContinueDelay,{},false);
            const auto* reward=app.pendingChestReward();
            require(reward&&reward->kind==ChestRewardKind::Evolution&&reward->title=="Ouroboros","Evolution reveal did not retain the real reward data");
            require(game.owned.front().evolved&&app.simulationBlocked(),"Evolution chest did not replace its base weapon and block play for the reveal");
            const auto pixels=capture(app,target,"boss-reward.png");
            const auto green=std::count_if(pixels.begin(),pixels.end(),[](Color c){return c.g>150&&c.g>c.r&&c.g>c.b;});
            require(green>80,"Evolution reward panel has no visible green heading or icon");
            require(app.acknowledgeChestReward()&&!app.acknowledgeChestReward()&&game.chests==1&&game.owned.front().evolved,"Evolution acknowledgement repeated its gameplay effect");
        });
        test("merged chest quantities produce FIFO reveals without repeating gold or healing",[&]{
            Presentation app(catalog);prepare(app,catalog,false);auto& game=app.game;game.player.hp=10;
            game.pickups.push_back({game.player.position,3,PickupKind::Chest});step(app);game.events.clear();
            require(game.chests==3&&game.gems==75&&game.player.hp==85,"Merged chest did not grant every treasure once");
            require(game.chestRewards.size()==3,"Merged chest lost its individual reward records");
            for(std::uint64_t serial=1;serial<=3;++serial){const auto* reward=app.pendingChestReward();
                require(reward&&reward->serial==serial&&reward->kind==ChestRewardKind::Treasure&&reward->gold==25,"Merged rewards were lost or revealed out of order");
                require(app.simulationBlocked()&&app.acknowledgeChestReward(),"A merged reward could not be acknowledged");
                require(game.gems==75&&game.player.hp==85&&game.chests==3,"Acknowledging a merged record granted its reward again");
            }
            require(!app.pendingChestReward()&&!app.simulationBlocked()&&!app.acknowledgeChestReward(),"Merged reward queue did not drain");
        });
        test("each merged lottery starts fresh and skip requires a separate guarded continue",[&]{
            Presentation app(catalog);prepare(app,catalog,false);auto& game=app.game;game.player.hp=10;
            game.pickups.push_back({game.player.position,3,PickupKind::Chest});step(app);
            require(game.chests==3&&game.gems==75&&game.player.hp==85,"Merged lottery fixture did not grant its real treasure");
            for(std::uint64_t serial=1;serial<=3;++serial){
                require(app.pendingChestReward()&&app.pendingChestReward()->serial==serial,"Lottery acknowledgements reordered merged rewards");
                require(app.chestRevealSeconds()==0,"A new reward inherited the previous lottery timer");
                app.update(loot::IntroSeconds*.5f,{},true);
                require(app.pendingChestReward()&&app.pendingChestReward()->serial==serial&&app.chestRevealSeconds()<loot::IntroSeconds,"The opening input skipped a new lottery");
                app.update(loot::IntroSeconds,{},true);
                require(app.pendingChestReward()&&app.pendingChestReward()->serial==serial,"Skipping consumed the reward record");
                require(std::abs(app.chestRevealSeconds()-loot::RevealSeconds)<.0001f,"Skipping did not land exactly on the reveal timestamp");
                app.update(loot::ContinueDelay*.5f,{},true);
                require(app.pendingChestReward()&&app.pendingChestReward()->serial==serial,"The reveal guard allowed an accidental double click");
                app.update(loot::ContinueDelay,{},true);
                require(game.chests==3&&game.gems==75&&game.player.hp==85,"Lottery input repeated treasure grants");
            }
            require(!app.pendingChestReward()&&!app.simulationBlocked()&&app.chestRevealSeconds()==0,"The final lottery left stale state or blocked combat");
        });
        test("settings and codex freeze the lottery without consuming clicks underneath",[&]{
            Presentation app(catalog);prepare(app,catalog,false);
            app.game.pickups.push_back({app.game.player.position,1,PickupKind::Chest});step(app);
            app.update(loot::IntroSeconds+.1f,{},false);
            const float clock=app.chestRevealSeconds();const auto serial=app.pendingChestReward()->serial;
            app.codex=true;app.update(1,{},true);
            require(app.pendingChestReward()&&app.pendingChestReward()->serial==serial&&app.chestRevealSeconds()==clock,"Codex input advanced or skipped the covered lottery");
            app.codex=false;app.openSettings();app.update(1,{-100,-100},true);
            require(app.settingsOpen&&app.pendingChestReward()&&app.pendingChestReward()->serial==serial&&app.chestRevealSeconds()==clock,"Settings input advanced or skipped the covered lottery");
            app.closeSettings();app.update(.1f,{},false);
            require(std::abs(app.chestRevealSeconds()-(clock+.1f))<.0001f,"Closing an overlay snapped or restarted lottery progress");
            require(app.simulationBlocked(),"Closing settings resumed combat through a pending reward");
        });
        test("lottery animation and preview drawing preserve gameplay and its next seeded choices",[&]{
            Presentation app(catalog);prepare(app,catalog);
            app.game.owned.push_back({catalog.find("rapid_fire"),1,false,1000});
            app.game.pickups.push_back({app.game.player.position,1,PickupKind::Chest});step(app);
            require(app.pendingChestReward()!=nullptr,"Animation isolation fixture did not grant a reward");
            Game control=app.game;
            const int frames=int(std::ceil((loot::RevealSeconds+loot::ContinueDelay+.2f)*60));
            for(int frame=0;frame<frames;++frame){
                app.update(1.0f/60.0f,{},false);
                if(frame%20==0){BeginTextureMode(target);ClearBackground(BLACK);app.draw();EndTextureMode();}
            }
            const float clock=app.chestRevealSeconds();
            capture(app,target,"boss-lottery-intro.png",false,0);
            capture(app,target,"boss-lottery-spinning.png",false,loot::IntroSeconds+loot::SpinSeconds*.4f);
            capture(app,target,"boss-lottery-landed.png",false,loot::RevealSeconds+loot::ContinueDelay);
            require(app.chestRevealSeconds()==clock&&app.pendingChestReward()&&app.simulationBlocked(),"Preview drawing changed or acknowledged the live lottery");
            require(app.game.elapsed==control.elapsed&&app.game.gems==control.gems&&app.game.chests==control.chests&&app.game.phase==control.phase,"Lottery animation changed combat time, currency, chest count, or phase");
            require(app.game.player.hp==control.player.hp&&app.game.player.xp==control.player.xp&&app.game.player.position.x==control.player.position.x&&app.game.player.position.y==control.player.position.y&&app.game.player.moveClock==control.player.moveClock,"Lottery animation advanced player state");
            require(app.game.owned.size()==control.owned.size()&&app.game.chestRewards.size()==control.chestRewards.size(),"Lottery animation changed inventory or reward storage");
            for(std::size_t i=0;i<control.owned.size();++i){const auto& actual=app.game.owned[i];const auto& expected=control.owned[i];
                require(actual.definition==expected.definition&&actual.rank==expected.rank&&actual.evolved==expected.evolved&&actual.cooldown==expected.cooldown,"Lottery animation changed weapon state");
            }
            app.game.grantXp(app.game.player.nextXp);control.grantXp(control.player.nextXp);
            require(app.game.phase==Phase::LevelUp&&app.game.offers==control.offers,"Lottery drawing consumed the simulation RNG before the next level offers");
            for(int i=0;i<3;++i){
                require(app.game.reroll()==control.reroll()&&app.game.offers==control.offers,"Lottery drawing changed subsequent seeded rerolls");
            }
        });
        test("codex pause and new runs preserve or reset reward state appropriately",[&]{
            Presentation app(catalog);prepare(app,catalog,false);auto& game=app.game;
            game.pickups.push_back({game.player.position,1,PickupKind::Chest});step(app);
            require(app.pendingChestReward()!=nullptr,"Fixture did not create its first reward");
            const auto serial=app.pendingChestReward()->serial;app.codex=true;app.update(.5f,{},true);
            require(app.simulationBlocked()&&app.pendingChestReward()&&app.pendingChestReward()->serial==serial,"Codex click dismissed the reward underneath it");
            app.codex=false;game.togglePause();const float elapsed=game.elapsed;game.update(FixedStep,{});
            require(game.phase==Phase::Paused&&game.elapsed==elapsed&&app.pendingChestReward(),"Pause lost the pending reward or advanced gameplay");
            require(app.acknowledgeChestReward()&&game.phase==Phase::Paused,"Reward acknowledgement unexpectedly resumed a paused run");
            require(!app.simulationBlocked(),"Closed codex and drained rewards still block the main loop");
            app.startRun(0xB056u);
            require(!app.pendingChestReward()&&!app.simulationBlocked()&&!app.codex&&game.chests==0&&game.chestRewards.empty(),"New run retained old reward acknowledgement or records");
            require(app.chestRevealSeconds()==0,"A new run retained the old lottery timer");
            app.screen=Screen::Game;isolate(game);game.pickups.push_back({game.player.position,1,PickupKind::Chest});step(app);
            require(app.pendingChestReward()&&app.pendingChestReward()->serial==1,"First chest of a new run was hidden by the previous acknowledgement");
        });
        require(!IsAudioDeviceReady(),"Native presentation tests initialized audio");
        UnloadRenderTexture(target);target={};CloseWindow();
    }catch(const std::exception& e){++failed;std::cerr<<"Native presentation setup failed: "<<e.what()<<'\n';if(target.id)UnloadRenderTexture(target);if(IsWindowReady())CloseWindow();}
    std::cout<<passed<<" passed, "<<failed<<" failed.\n";return failed?1:0;
}
