#include "presentation.hpp"
#include "chest_lottery.hpp"
#include "web_platform.hpp"
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <rlgl.h>

namespace snake {
namespace {
constexpr Color White{255,255,255,255}, Gray{190,190,190,255}, DarkGray{85,85,85,255};
constexpr Color Cyan{0,255,255,255}, Green{68,255,68,255}, Yellow{255,255,0,255};
constexpr Color Orange{255,170,0,255}, Border{68,68,68,255};
constexpr int UpgradeCardWidth=176,UpgradeCardHeight=244,UpgradeCardGap=14;
int upgradeY(int height){return (height-UpgradeCardHeight)/2-10;}
int upgradeStart(int count,int width){return (width-(UpgradeCardWidth*count+UpgradeCardGap*(count-1)))/2;}
constexpr const char* Names[]{"COBRA","TAIPAN","MAMBA","BASILISK"};
constexpr const char* Weapons[]{"Venom Shot","Viper Fangs","Tongue Lash","Plague Mortar"};
constexpr const char* Descriptions[]{
    "Forward venom spread","Homing fangs",
    "Slowing tongue strike","Poison bombs and pools"};
constexpr const char* Passives[]{"Venom Potency","Apex Predator","Coiling Strike","Pestilence"};
constexpr const char* PassiveDescriptions[]{
    "+15% damage","10% shorter cooldowns",
    "+15% area","+20% duration"};
std::string clockText(float seconds) {
    const int t=std::max(0,int(seconds));std::ostringstream s;s<<t/60<<':'<<std::setfill('0')<<std::setw(2)<<t%60;return s.str();
}
bool inside(Vec p,int x,int y,int w,int h) {return p.x>=x && p.y>=y && p.x<x+w && p.y<y+h;}
int evolutionIcon(Pattern p) {
    constexpr int indices[]{18,19,20,21,22,23,24,25,26};return indices[int(p)];
}
}
Presentation::Presentation(const Catalog& catalog,const Settings& initialSettings) : game(catalog),settings(initialSettings),settingsDraft(initialSettings) {
    prepareWorldEffects();
#ifdef __EMSCRIPTEN__
    savePath = web::ProfilePath;
#else
    savePath = (std::filesystem::path(GetApplicationDirectory()) / "snake-survivors.save").string();
#endif
    loadProfile();
    sound = settings.audio.enabled;
    game.debugPopulate();
    if (!IsWindowHidden()) {
        InitAudioDevice();
        audioReady = IsAudioDeviceReady();
    }
    if (audioReady) {
        for (std::size_t effect = 0; effect < sounds.size(); ++effect) {
            for (std::size_t variation = 0; variation < audio::VariationCount; ++variation) {
                auto samples = audio::synthesize(Event::Type(effect), variation);
                Wave wave{}; wave.frameCount = unsigned(samples.size()); wave.sampleRate = audio::SampleRate;
                wave.sampleSize = 16; wave.channels = 1; wave.data = samples.data();
                sounds[effect][variation] = LoadSoundFromWave(wave);
                if (IsSoundValid(sounds[effect][variation]))
                    SetSoundVolume(sounds[effect][variation], audio::definition(Event::Type(effect)).volume);
            }
        }
        auto tickSamples=audio::synthesizeReelTick();
        Wave tickWave{};tickWave.frameCount=unsigned(tickSamples.size());tickWave.sampleRate=audio::SampleRate;
        tickWave.sampleSize=16;tickWave.channels=1;tickWave.data=tickSamples.data();
        reelTickSound=LoadSoundFromWave(tickWave);
        if(IsSoundValid(reelTickSound))SetSoundVolume(reelTickSound,audio::ReelTickVolume);
    }
}
Presentation::~Presentation() {
    if (audioReady) {
        resetAudio();
        for (const auto& bank : sounds) for (const Sound sample : bank) if (IsSoundValid(sample)) UnloadSound(sample);
        if(IsSoundValid(reelTickSound))UnloadSound(reelTickSound);
        CloseAudioDevice();
    }
}
void Presentation::setViewport(int width,int height,int scale){
    viewWidth=std::clamp(width,1,16384);viewHeight=std::clamp(height,1,16384);pixelScale=std::clamp(scale,1,32);
    worldRaster.setViewport(viewWidth,viewHeight);
}
void Presentation::queueAudioEvents() {
    for (const Event& event : game.events) {
        // A chest's existing soft reward chord belongs to the reel landing.
        if((event.type==Event::Chest || event.type==Event::Evolve) && pendingChestReward())continue;
        audioDirector.queue(event.type);
    }
    game.events.clear();
}
void Presentation::updateAudio(float frameDt) {
    const bool ticking=audioReady && sound && settings.audio.enabled && !settingsOpen && !codex &&
        pendingChestReward() && chestRevealSeconds()<loot::RevealSeconds;
    if(IsSoundValid(reelTickSound)){
        if(ticking && reelTickPending){
            // One preloaded UI voice, outside the combat cooldown/ducking gate.
            StopSound(reelTickSound);
            SetSoundVolume(reelTickSound,audio::ReelTickVolume*settings.audio.master*settings.audio.interfaceVolume);
            PlaySound(reelTickSound);
        }else if(!ticking && IsSoundPlaying(reelTickSound))StopSound(reelTickSound);
    }
    reelTickPending=false;
    const auto frame = audioDirector.advance(frameDt, audioReady && sound && settings.audio.enabled);
    for (std::size_t i = 0; i < frame.count; ++i) {
        const auto cue = frame.cues[i];
        auto& previous = audioVoices[cue.voice];
        if (IsSoundValid(previous)) StopSound(previous);
        previous = sounds[std::size_t(cue.effect)][cue.variation];
        if (IsSoundValid(previous)) {
            const bool combat=cue.effect==Event::Shot || cue.effect==Event::Hit || cue.effect==Event::Kill;
            const bool feedback=cue.effect==Event::Hurt;
            const float category=combat?settings.audio.combat:feedback?settings.audio.effects:settings.audio.interfaceVolume;
            SetSoundVolume(previous,audio::definition(cue.effect).volume*settings.audio.master*category);
            PlaySound(previous);
        }
    }
}
void Presentation::resetAudio() {
    audioDirector.reset();
    reelTickPending=false;
    if(IsSoundValid(reelTickSound))StopSound(reelTickSound);
    if (audioReady) for (auto& sample : audioVoices) {if (IsSoundValid(sample)) StopSound(sample); sample = {};}
}
void Presentation::toggleSound() {
    const bool enabled=!settings.audio.enabled;
    Settings changed=settings;changed.audio.enabled=enabled;
    settings.audio.enabled=enabled;sound=enabled;
    if(!enabled)resetAudio();
    if(onApplySettings){
        std::string feedback;
        if(!onApplySettings(changed,feedback))settingsMessage=feedback.empty()?"Sound changed, but settings could not be saved.":feedback;
    }
    // Persistence may restore its previous model on failure. Muting remains
    // an immediate in-memory action even when the settings file is read-only.
    settings.audio.enabled=enabled;sound=enabled;
    if(!enabled)resetAudio();
}
void Presentation::loadProfile() {
    std::ifstream file(savePath);
    std::string version;
    Profile loaded;
    if (file >> version >> loaded.bestKills >> loaded.runs >> loaded.bestSeconds >> loaded.victories >> loaded.sound && version == "SS1") {
        if (loaded.bestKills >= 0 && loaded.runs >= 0 && loaded.bestSeconds >= 0 && loaded.victories >= 0) profile = loaded;
    }
}
void Presentation::saveProfile() {
    profile.sound = sound;
    std::ofstream file(savePath, std::ios::trunc);
    file << "SS1 " << profile.bestKills << ' ' << profile.runs << ' ' << profile.bestSeconds << ' ' << profile.victories << ' ' << profile.sound << '\n';
#ifdef __EMSCRIPTEN__
    file.close();web::flushSaves();
#endif
}

void Presentation::startRun(std::uint32_t seed) {
    resetAudio();
    settingsOpen=false;rebindingAction=-1;
    motionHistory.clear();interpolateMotion=false;renderFraction=1;
    if(seed==0)seed=std::uint32_t(GetTime()*1000.0)^0x5EEDu;
    game.reset(selectedCharacter,seed);screen=Screen::Countdown;countdown=1;codex=false;recorded=false;focus=0;
    acknowledgedChest=revealingChest=0;rewardTime=0;reelTicker.reset();deathSeconds=0;
    // Begin the first visual cell immediately when the countdown finishes.
    game.player.moveClock=game.player.moveInterval;
}
void Presentation::beginSimulationStep(){motionHistory.beginStep(game);deathSeconds=0;}
void Presentation::setRenderFrame(float alpha,bool interpolate){
    renderFraction=std::clamp(alpha,0.0f,1.0f);interpolateMotion=interpolate;
}
const ChestReward* Presentation::pendingChestReward() const {
    if(screen!=Screen::Game)return nullptr;
    for(const auto& reward:game.chestRewards)if(reward.serial>acknowledgedChest)return &reward;
    return nullptr;
}
bool Presentation::simulationBlocked() const {return settingsOpen || codex || pendingChestReward()!=nullptr;}
float Presentation::chestRevealSeconds() const {
    const auto* reward=pendingChestReward();
    return reward && revealingChest==reward->serial?rewardTime:0;
}
bool Presentation::acknowledgeChestReward() {
    const auto* reward=pendingChestReward();if(!reward)return false;
    acknowledgedChest=reward->serial;rewardTime=0;revealingChest=0;reelTicker.reset();reelTickPending=false;
    if(IsSoundValid(reelTickSound))StopSound(reelTickSound);
    return true;
}
void Presentation::finishRun() {
    if(recorded)return;
    recorded=true;++profile.runs;profile.bestKills=std::max(profile.bestKills,game.kills);
    profile.bestSeconds=std::max(profile.bestSeconds,int(game.elapsed));
    if(game.phase==Phase::Victory)++profile.victories;
    saveProfile();
}
void Presentation::captureScene(const std::string& scene) {
    if(scene=="settings"){screen=Screen::Menu;openSettings();return;}
    if(scene=="menu"){screen=Screen::Menu;return;}
    startRun(0x5EEDu);screen=Screen::Game;countdown=0;game.debugPopulate();
    if(scene=="nest-flight" || scene=="nest-hatch" || scene=="nest-brood"){
        game.reset(0,0x5EEDu);game.owned={{game.catalog.find("snake_nest"),1,false,0}};
        game.enemies.clear();game.pickups.clear();game.spawnClock=-1000;game.bossClock=10;
        game.player.moveClock=-1000;game.player.invulnerable=1000;
        for(int i=0;i<5;++i){
            Enemy enemy;enemy.id=std::uint32_t(10000+i);enemy.position={game.player.position.x+64+float((i%3)*Tile),game.player.position.y+float((i/3)*Tile)};
            enemy.hp=enemy.maxHp=10000;enemy.speed=0;enemy.trailSize=0;enemy.trail.fill(enemy.position);game.enemies.push_back(enemy);
        }
        const int frames=scene=="nest-flight"?20:scene=="nest-hatch"?55:100;
        for(int i=0;i<frames;++i)game.update(FixedStep,{});
        game.player.invulnerable=0;
        return;
    }
    if(scene=="upgrades"){game.grantXp(game.player.nextXp);game.phase=Phase::LevelUp;}
    else if(scene=="evolutions")codex=true;
    else if(scene=="pause")game.phase=Phase::Paused;
    else if(scene=="results"){game.phase=Phase::Dead;recorded=true;}
}
void Presentation::update(float dt,Vec cursor,bool pressed) {
    const bool mouseMoved=cursor.x!=mouse.x || cursor.y!=mouse.y;
    visualTime+=dt;mouse=cursor;clicked=pressed;
    if(settingsOpen){
        const Vec saved=mouse;mouse.x-=float((viewWidth-CanvasWidth)/2);mouse.y-=float((viewHeight-CanvasHeight)/2);
        updateSettings(dt);mouse=saved;return;
    }
    if(IsKeyPressed(settings.controls.keys[std::size_t(Action::Settings)])){openSettings();return;}
    if(IsKeyPressed(settings.controls.keys[std::size_t(Action::Mute)]))toggleSound();
    if(IsKeyPressed(KEY_F3) && std::find(settings.controls.keys.begin(),settings.controls.keys.end(),KEY_F3)==settings.controls.keys.end())debug=!debug;
    if(IsKeyPressed(settings.controls.keys[std::size_t(Action::Codex)]) || (settings.controls.keys[std::size_t(Action::Codex)]==KEY_TAB && IsKeyPressed(KEY_F2)))codex=!codex;
    if(codex){if(IsKeyPressed(KEY_ESCAPE))codex=false;return;}
    if(screen==Screen::Menu){
        if(IsKeyPressed(KEY_RIGHT) || IsKeyPressed(settings.controls.keys[std::size_t(Action::Right)]))selectedCharacter=(selectedCharacter+1)%4;
        if(IsKeyPressed(KEY_LEFT) || IsKeyPressed(settings.controls.keys[std::size_t(Action::Left)]))selectedCharacter=(selectedCharacter+3)%4;
        if(IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_ENTER))startRun();
#ifndef __EMSCRIPTEN__
        if(IsKeyPressed(KEY_ESCAPE))quit=true;
#endif
        return;
    }
    if(screen==Screen::Countdown){countdown-=dt;if(countdown<=0)screen=Screen::Game;return;}
    if(const auto* reward=pendingChestReward()){
        if(revealingChest!=reward->serial){revealingChest=reward->serial;rewardTime=0;reelTicker.reset();}
        const float previousTime=rewardTime;
        rewardTime=std::min(rewardTime+std::max(0.0f,dt),loot::RevealSeconds+loot::ContinueDelay+1);
        const bool activate=rewardTime>=loot::IntroSeconds && (IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_ESCAPE) || clicked);
        if(activate){
            // Skip and continue are separate presses. Neither can fall through
            // to a level-up/settings button drawn beneath the reward overlay.
            clicked=false;
            if(rewardTime<loot::RevealSeconds)rewardTime=loot::RevealSeconds;
        }
        reelTickPending=reelTicker.advance(rewardTime,activate || rewardTime>=loot::RevealSeconds);
        if(previousTime<loot::RevealSeconds && rewardTime>=loot::RevealSeconds)
            audioDirector.queue(reward->kind==ChestRewardKind::Evolution?Event::Evolve:Event::Chest);
        if(activate && previousTime>=loot::RevealSeconds && rewardTime>=loot::RevealSeconds+loot::ContinueDelay)acknowledgeChestReward();
        return;
    }
    if(game.phase==Phase::Dead || game.phase==Phase::Victory){
        if(game.phase==Phase::Dead && game.playerDied){
            deathSeconds=std::min(.8f,deathSeconds+std::max(0.0f,dt));
            if(deathSeconds<.8f)return;
        }
        finishRun();
        if(IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_ESCAPE))screen=Screen::Menu;
        return;
    }
    if(game.phase==Phase::Paused && (IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_ENTER)))game.togglePause();
    else if(IsKeyPressed(settings.controls.keys[std::size_t(Action::Pause)]) || (settings.controls.keys[std::size_t(Action::Pause)]==KEY_ESCAPE && IsKeyPressed(KEY_P)))game.togglePause();
    if(game.phase==Phase::LevelUp){
        const int offerCount=game.offers[0]<0?1:3;focus=std::clamp(focus,0,offerCount-1);
        // Hover takes focus only on movement; keyboard input below wins ties.
        if(mouseMoved){
            const int start=upgradeStart(offerCount,viewWidth);
            for(int i=0;i<offerCount;++i){
                if(inside(mouse,start+i*(UpgradeCardWidth+UpgradeCardGap),upgradeY(viewHeight),UpgradeCardWidth,UpgradeCardHeight)){
                    focus=i;break;
                }
            }
        }
        if(IsKeyPressed(KEY_RIGHT) || IsKeyPressed(settings.controls.keys[std::size_t(Action::Right)]))focus=(focus+1)%offerCount;
        if(IsKeyPressed(KEY_LEFT) || IsKeyPressed(settings.controls.keys[std::size_t(Action::Left)]))focus=(focus+offerCount-1)%offerCount;
        for(int i=0;i<3;++i)if(IsKeyPressed(KEY_ONE+i))game.choose(i);
        if(IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_ENTER))game.choose(focus);
        if(IsKeyPressed(settings.controls.keys[std::size_t(Action::Reroll)]))game.reroll();
        if(IsKeyPressed(settings.controls.keys[std::size_t(Action::Banish)]))game.banish(focus);
    }else if(game.phase==Phase::Paused && IsKeyPressed(KEY_Q))screen=Screen::Menu;
}
void Presentation::label(const std::string& text,int x,int y,Color color,int scale){pixel::text(text,x,y,color,scale);}
void Presentation::outline(int x,int y,int width,int height,Color color){
    if(pixelScale==1)DrawRectangleLines(x,y,width,height,color);else pixel::outline(x,y,width,height,color);
}
void Presentation::centered(const std::string& text,int x,int y,Color color,int scale){label(text,x-pixel::width(text,scale)/2,y,color,scale);}
void Presentation::panel(int x,int y,int w,int h,Color border){
    DrawRectangle(x,y,w,h,{24,24,24,255});outline(x,y,w,h,border);
}
void Presentation::bar(int x,int y,int w,int h,float ratio,Color color){
    DrawRectangle(x,y,w,h,{26,26,26,255});DrawRectangle(x,y,int(float(w)*std::clamp(ratio,0.0f,1.0f)),h,color);
}
int Presentation::wrappedCentered(const std::string& text,int cx,int y,int maxWidth,Color color,int maxLines,int lineHeight){
    std::istringstream input(text);std::string word,line;int lines=0;
    while(input>>word){
        const std::string next=line.empty()?word:line+" "+word;
        if(pixel::width(next)>maxWidth && !line.empty()){
            centered(line,cx,y+lines*lineHeight,color);++lines;if(lines>=maxLines)return lines;line=word;
        }else line=next;
    }
    if(!line.empty() && lines<maxLines){centered(line,cx,y+lines*lineHeight,color);++lines;}
    return lines;
}
void Presentation::wrapped(const std::string& text,int x,int y,int maxWidth,Color color){
    std::istringstream input(text);std::string word,line;
    while(input>>word){
        const std::string next=line.empty()?word:line+" "+word;
        if(pixel::width(next)>maxWidth && !line.empty()){label(line,x,y,color);y+=10;line=word;}else line=next;
    }
    if(!line.empty())label(line,x,y,color);
}
bool Presentation::button(const std::string& text,int x,int y,int w,int h,bool selected){
    const bool hover=inside(mouse,x,y,w,h);outline(x,y,w,h,selected || hover?White:DarkGray);
    centered(text,x+w/2,y+(h-8)/2,selected || hover?White:Gray);return hover && clicked;
}
int Presentation::iconFor(int definition)const{
    if(definition<0 || definition>=int(game.catalog.upgrades.size()))return 0;
    const auto& d=game.catalog.upgrades[std::size_t(definition)];
    if(d.kind==UpgradeKind::Weapon)return int(d.pattern);
    if(d.modifiers.empty())return 9;
    switch(d.modifiers.front().stat){
    case Stat::Pickup:return 9;case Stat::Cooldown:return 10;case Stat::Crit:return 11;case Stat::Growth:return 12;
    case Stat::Duration:return 13;case Stat::Amount:return 14;case Stat::Area:return 15;case Stat::Range:return 16;case Stat::Armor:return 17;default:return 12;
    }
}
void Presentation::menu(){
    ClearBackground(BLACK);
    const int size=450, oy=(viewHeight-size)/2;
    centered("SNAKE SURVIVORS",viewWidth/2,oy+29,White,2);
    centered("CHOOSE YOUR SERPENT",viewWidth/2,oy+57,Gray);
    const int cw=124,ch=250,gap=12,total=cw*4+gap*3,start=(viewWidth-total)/2,cy=oy+81;
    for(int i=0;i<4;++i){
        const int x=start+i*(cw+gap),cx=x+cw/2;const bool selected=i==selectedCharacter;
        DrawRectangle(x,cy,cw,ch,selected?Color{20,20,20,255}:Color{15,15,15,255});
        DrawRectangleLinesEx({float(x),float(cy),float(cw),float(ch)},selected?2.0f:1.0f,selected?White:Border);
        BeginScissorMode((x+2)*pixelScale,(cy+2)*pixelScale,(cw-4)*pixelScale,(ch-4)*pixelScale);
        if(selected){for(int r=0;r<4;++r)DrawRectangle(cx-3+r,cy+6+r,7-r*2,1,White);}
        const int previewY=cy+23;
        for(int j=0;j<4;++j){
            const int shade=j==3?255:52+j*35;DrawRectangle(cx-22+j*11,previewY,10,10,{static_cast<unsigned char>(shade),static_cast<unsigned char>(shade),static_cast<unsigned char>(shade),255});
        }
        const int hx=cx+11;
        if(i==0){DrawRectangle(hx+6,previewY+2,1,1,BLACK);DrawRectangle(hx+6,previewY+7,1,1,BLACK);}
        else if(i==1){DrawRectangle(hx+6,previewY+1,1,3,Orange);DrawRectangle(hx+6,previewY+6,1,3,Orange);}
        else if(i==2){DrawRectangle(hx+6,previewY+2,2,1,RED);DrawRectangle(hx+6,previewY+7,2,1,RED);pixel::line(hx+9,previewY+5,hx+13,previewY+5,RED);DrawPixel(hx+14,previewY+4,RED);DrawPixel(hx+14,previewY+6,RED);}
        else{DrawRectangle(hx+6,previewY+2,2,2,GREEN);DrawRectangle(hx+6,previewY+6,2,2,GREEN);}
        centered(Names[i],cx,cy+50,White,2);
        if(pixelScale==1)DrawLine(x+13,cy+80,x+cw-13,cy+80,Border);else DrawRectangle(x+13,cy+80,cw-26,1,Border);
        art.iconSized(i==0?0:i==1?1:i==2?8:2,cx-12,cy+92,24);
        wrappedCentered(Weapons[i],cx,cy+130,cw-12,Orange,2,11);
        wrappedCentered(Descriptions[i],cx,cy+152,cw-16,Gray,3,11);
        if(pixelScale==1)DrawLine(x+13,cy+190,x+cw-13,cy+190,Border);else DrawRectangle(x+13,cy+190,cw-26,1,Border);
        wrappedCentered(Passives[i],cx,cy+202,cw-12,Green,2,11);
        wrappedCentered(PassiveDescriptions[i],cx,cy+220,cw-12,White,2,11);
        EndScissorMode();
        if(clicked && inside(mouse,x,cy,cw,ch)){if(selected)startRun();else selectedCharacter=i;}
    }
    centered("Left / Right  Select    Enter  Start",viewWidth/2,oy+358,White);
    const bool wasd=settings.controls.keys[0]==KEY_W&&settings.controls.keys[1]==KEY_S&&settings.controls.keys[2]==KEY_A&&settings.controls.keys[3]==KEY_D;
    const std::string movement=wasd?(settings.controls.alternateArrows?"WASD / Arrows":"WASD"):controlName(Action::Up)+" / "+controlName(Action::Left)+" / "+controlName(Action::Down)+" / "+controlName(Action::Right);
    centered(movement+"  Move",viewWidth/2,oy+383,Gray);
    centered(controlName(Action::Codex)+"  Evolutions",viewWidth/2,oy+408,Gray);
    if(button("SETTINGS",viewWidth-124,viewHeight-34,110,22))openSettings();
    if(profile.bestSeconds>0)centered("Best survival: "+clockText(float(profile.bestSeconds)),viewWidth/2,oy+438,Gray);
}
void Presentation::hud(){
    constexpr Color hudBackground{12,12,12,255};
    const std::string kills="Kills: "+std::to_string(game.kills),time=clockText(game.elapsed),length="Length: "+std::to_string(game.player.length);
    DrawRectangle(0,0,std::max(96,pixel::width(kills)+12),86,hudBackground);
    const int timeWidth=pixel::width(time,2)+16;
    DrawRectangle((viewWidth-timeWidth)/2,0,timeWidth,27,hudBackground);
    DrawRectangle(viewWidth-pixel::width(length)-12,0,pixel::width(length)+12,20,hudBackground);
    label(kills,6,6,White);
    for(int i=0;i<5;++i){
        if(game.player.hp>float(i)*20)art.draw(pixel::Sprite::Heart,9+i*17,25);
        else{
            // Raster outline uses the same source pixels as the filled hearts.
            const int x=9+i*17;pixel::line(x+1,26,x+3,26,Gray);pixel::line(x+5,26,x+6,26,Gray);
            pixel::line(x,27,x,29,Gray);pixel::line(x+7,27,x+7,29,Gray);
            pixel::line(x,29,x+3,32,Gray);pixel::line(x+7,29,x+3,32,Gray);
        }
    }
    for(int row=0;row<2;++row){
        const int y=row==0?37:62;
        std::array<const OwnedUpgrade*,4> equipment{};int count=0;
        for(const auto& owned:game.owned)if((game.catalog.upgrades[std::size_t(owned.definition)].kind==UpgradeKind::Weapon)==(row==0) && count<int(equipment.size()))equipment[std::size_t(count++)]=&owned;
        for(int i=0;i<4;++i){
            const int x=6+i*21;outline(x,y,12,12,Border);
            if(i<count){
                const auto& owned=*equipment[std::size_t(i)];const auto& d=game.catalog.upgrades[std::size_t(owned.definition)];
                art.iconSized(owned.evolved?evolutionIcon(d.pattern):iconFor(owned.definition),x+1,y+1,10);
                centered(std::to_string(owned.rank),x+6,y+13,White);
            }
        }
    }
    centered(time,viewWidth/2,5,White,2);
    label(length,viewWidth-6-pixel::width(length),6,White);
    const int bw=112,bh=5,bx=(viewWidth-bw)/2,by=viewHeight-12;
    const std::string level="Lv "+std::to_string(game.player.level);const int levelX=bx-8-pixel::width(level);
    DrawRectangle(levelX-6,by-6,bx+bw-levelX+12,18,hudBackground);
    bar(bx,by,bw,bh,float(game.player.xp)/float(std::max(1,game.player.nextXp)),{0,255,136,255});
    outline(bx,by,bw,bh,{64,64,64,255});
    label(level,levelX,by-1,White);
    const int mapSize=110,mx=viewWidth-mapSize-5,my=viewHeight-mapSize-5;
    DrawRectangle(mx,my,mapSize,mapSize,Fade(BLACK,.7f));outline(mx,my,mapSize,mapSize,Border);
    auto mapPoint=[&](Vec p){return Vec{float(mx)+std::clamp(p.x/ArenaSize,0.0f,1.0f)*float(mapSize-2)+1,float(my)+std::clamp(p.y/ArenaSize,0.0f,1.0f)*float(mapSize-2)+1};};
    for(const auto& enemy:game.enemies)if(enemy.alive){
        const Vec p=mapPoint(enemy.position);const Color color=enemy.kind==EnemyKind::Boss?Color{187,68,255,255}:enemy.kind==EnemyKind::Splitter?Color{60,130,220,255}:Color{160,0,0,255};
        if(enemy.kind==EnemyKind::Boss)DrawRectangle(int(p.x)-1,int(p.y)-1,3,3,color);else DrawPixel(int(p.x),int(p.y),color);
    }
    for(const auto& pickup:game.pickups)if(pickup.kind==PickupKind::Chest){const Vec p=mapPoint(pickup.position);DrawRectangle(int(p.x)-1,int(p.y)-1,3,3,{255,215,0,255});}
    for(const auto& snake:game.greySnakes.trails){
        for(std::size_t i=0;i<snake.count;++i){const Vec p=mapPoint(snake.path[i]);DrawPixel(int(p.x),int(p.y),{160,160,160,153});}
        if(snake.alive && snake.count){const Vec p=mapPoint(snake.path[snake.count-1]);DrawRectangle(int(p.x)-1,int(p.y)-1,3,3,{220,220,220,230});}
    }
    for(const Vec segment:game.player.segments){const Vec p=mapPoint(segment);DrawPixel(int(p.x),int(p.y),White);}
    const Vec head=mapPoint(game.player.position);DrawRectangle(int(head.x)-1,int(head.y)-1,2,2,Cyan);
    // Only boss arrivals use the bottom notification banner.
    if(game.noticeTime>0 && game.notice=="AN ANCIENT SERPENT APPROACHES" && !pendingChestReward()){
        const int width=pixel::width(game.notice)+16;
        DrawRectangle((viewWidth-width)/2,viewHeight-40,width,18,hudBackground);
        centered(game.notice,viewWidth/2,viewHeight-35,Yellow);
    }
    if(debug){
        const std::string perf="FPS "+std::to_string(GetFPS())+" | Enemies "+std::to_string(game.enemies.size())+" | Shots "+std::to_string(game.projectiles.size());
        DrawRectangle(viewWidth-330,24,330,15,Fade(BLACK,.75f));label(perf,viewWidth-326,28,White);
    }
}
void Presentation::levelUp(){
    DrawRectangle(0,0,viewWidth,viewHeight,Fade(BLACK,.9f));
    centered("CHOOSE AN UPGRADE",viewWidth/2,34,White,2);
    const int cw=UpgradeCardWidth,ch=UpgradeCardHeight,gap=UpgradeCardGap,cy=upgradeY(viewHeight);
    const int count=game.offers[0]<0?1:3,start=upgradeStart(count,viewWidth);
    focus=std::clamp(focus,0,count-1);
    for(int i=0;i<count;++i){
        const int x=start+i*(cw+gap),cx=x+cw/2;
        const bool selected=focus==i;panel(x,cy,cw,ch,selected?White:DarkGray);
        if(selected)DrawRectangleLinesEx({float(x),float(cy),float(cw),float(ch)},2,White);
        const int index=game.offers[std::size_t(i)];
        if(index<0 || index>=int(game.catalog.upgrades.size())){
            if(i==0 && game.offers[0]<0){
                centered("Second Wind",cx,cy+23,White);art.draw(pixel::Sprite::Heart,cx-4,cy+63);
                wrappedCentered("+25 health, +10 gold",cx,cy+117,cw-20,Cyan);
                if(clicked && inside(mouse,x,cy,cw,ch))game.choose(0);
            }
            centered("["+std::to_string(i+1)+"]",cx,cy+ch-17,White);continue;
        }
        const auto& d=game.catalog.upgrades[std::size_t(index)];int rank=0;
        for(const auto& owned:game.owned)if(owned.definition==index)rank=owned.rank;
        if(rank==0){
            const int badgeWidth=pixel::width("NEW")+6,badgeHeight=pixel::lineHeight()+4;
            const int badgeX=x+cw-5-badgeWidth,badgeY=cy+5;
            DrawRectangle(badgeX,badgeY,badgeWidth,badgeHeight,{0,255,0,255});
            label("NEW",badgeX+3,badgeY+2,BLACK);
        }
        wrappedCentered(d.name,cx,cy+24,cw-16,White,2,11);
        const int pitch=std::min(7,(cw-20)/d.maxRank),pipWidth=std::max(1,pitch-2),pipsWidth=d.maxRank*pitch-2;
        for(int p=0;p<d.maxRank;++p)DrawRectangle(cx-pipsWidth/2+p*pitch,cy+48,pipWidth,3,p<rank?Cyan:p==rank?White:Color{51,51,51,255});
        centered("Lv "+std::to_string(rank)+" -> "+std::to_string(rank+1),cx,cy+57,Gray);
        art.iconSized(iconFor(index),cx-16,cy+73,32);
        wrappedCentered(d.description,cx,cy+117,cw-24,d.kind==UpgradeKind::Weapon?Gray:Cyan,5,11);
        if(d.kind==UpgradeKind::Weapon){
            const auto preview=game.weaponSpec({index,rank+1,false,0});
            centered("Damage "+std::to_string(int(std::round(preview.damage))),cx,cy+181,Cyan);
            centered("Cooldown "+std::to_string(int(std::round(preview.cooldown*1000)))+" ms",cx,cy+194,Cyan);
        }
        centered("["+std::to_string(i+1)+"]",cx,cy+ch-17,White);
        if(clicked && inside(mouse,x,cy,cw,ch))game.choose(i);
    }
    const std::string reroll=controlName(Action::Reroll)+" Reroll ("+std::to_string(game.rerolls)+")",banish=controlName(Action::Banish)+" Banish ("+std::to_string(game.banishes)+")";
    const int rerollWidth=std::max(126,pixel::width(reroll)+14),banishWidth=std::max(126,pixel::width(banish)+14);
    if(button(reroll,viewWidth/2-10-rerollWidth,cy+ch+18,rerollWidth,20))game.reroll();
    if(button(banish,viewWidth/2+10,cy+ch+18,banishWidth,20))game.banish(focus);
}
void Presentation::pause(){
    DrawRectangle(0,0,viewWidth,viewHeight,Fade(BLACK,.6f));
    centered("PAUSED",viewWidth/2,viewHeight/2-25,White,3);
    centered("Space  Resume    Q  Menu",viewWidth/2,viewHeight/2+10,White);
    if(button("SETTINGS",viewWidth/2-76,viewHeight/2+37,152,24))openSettings();
}
void Presentation::result(){
    const bool won=game.phase==Phase::Victory;
    panel(viewWidth/2-180,viewHeight/2-62,360,124,won?Green:Border);
    centered(won?"YOU WON!":"GAME OVER",viewWidth/2,viewHeight/2-43,won?Green:White,2);
    centered(clockText(game.elapsed)+"    "+std::to_string(game.kills)+" kills",viewWidth/2,viewHeight/2-8,White);
    centered("Enter  Menu",viewWidth/2,viewHeight/2+32,Gray);
}
void Presentation::evolutionCodex(){
    constexpr int cw=192,ch=102,gap=12,pad=20,menuW=cw*3+gap*2+pad*2,menuH=ch*3+gap*2+90;
    const int mx=(viewWidth-menuW)/2,my=(viewHeight-menuH)/2;
    DrawRectangle(mx,my,menuW,menuH,{12,12,12,255});outline(mx,my,menuW,menuH,Yellow);
    centered("EVOLUTIONS",viewWidth/2,my+15,Yellow,2);
    for(std::size_t i=0;i<game.catalog.evolutions.size() && i<9;++i){
        const auto& e=game.catalog.evolutions[i];const int x=mx+pad+int(i%3)*(cw+gap),y=my+43+int(i/3)*(ch+gap);
        const int weapon=game.catalog.find(e.weapon),passive=game.catalog.find(e.passive);int wr=0,pr=0;bool evolved=false;
        for(const auto& owned:game.owned){if(owned.definition==weapon){wr=owned.rank;evolved=owned.evolved;}if(owned.definition==passive)pr=owned.rank;}
        const int target=weapon>=0?game.catalog.upgrades[std::size_t(weapon)].maxRank:8;const bool ready=wr>=target && pr>0;
        DrawRectangle(x,y,cw,ch,evolved?Color{0,24,0,255}:Color{24,24,24,255});
        outline(x,y,cw,ch,evolved?Green:ready?Cyan:DarkGray);
        if(weapon>=0)art.iconSized(evolutionIcon(game.catalog.upgrades[std::size_t(weapon)].pattern),x+7,y+7,24);
        const std::string name=e.name;
        std::istringstream words(name);std::string word,line;int nameY=y+8;
        while(words>>word){
            if(pixel::width(line.empty()?word:line+" "+word)>cw-44 && !line.empty()){label(line,x+38,nameY,evolved?Green:Yellow);nameY+=9;line=word;}else line=line.empty()?word:line+" "+word;
        }
        if(!line.empty())label(line,x+38,nameY,evolved?Green:Yellow);
        label(evolved?"UNLOCKED":ready?"READY":"LOCKED",x+38,y+32,evolved?Green:ready?Cyan:Gray);
        for(int r=0;r<2;++r){
            const int def=r==0?weapon:passive;if(def<0)continue;
            const int rank=r==0?wr:pr,goal=r==0?target:1;const Color color=rank>=goal?Green:Color{238,170,170,255};
            art.iconSized(iconFor(def),x+7,y+51+r*19,10);
            label(game.catalog.upgrades[std::size_t(def)].name,x+23,y+52+r*19,color);
            const std::string counts=std::to_string(std::min(rank,goal))+"/"+std::to_string(goal);
            label(counts,x+cw-7-pixel::width(counts),y+61+r*19,rank>=goal?Green:Gray);
        }
    }
    centered("Max weapon + passive + chest",viewWidth/2,my+menuH-30,White);
    centered(controlName(Action::Codex)+" / Esc  Close",viewWidth/2,my+menuH-15,Gray);
}
void Presentation::chestReward(float seconds){
    const auto* reward=pendingChestReward();if(!reward)return;
    const bool landed=seconds>=loot::RevealSeconds;
    const bool evolution=reward->kind==ChestRewardKind::Evolution;
    const Color accent=landed && evolution?Green:Color{255,210,100,255};
    DrawRectangle(0,0,viewWidth,viewHeight,{0,0,0,210});
    const int w=440,h=290,x=(viewWidth-w)/2,y=(viewHeight-h)/2,cx=viewWidth/2;
    DrawRectangle(x,y,w,h,{12,12,12,255});outline(x,y,w,h,{92,78,42,255});
    centered(landed && evolution?"EVOLUTION UNLOCKED":"BOSS CHEST",cx,y+20,accent,2);
    art.chest(cx,y+95,pixel::ChestSize::Reward);

    // Nine visible cells, integer source-pixel motion, and a fixed-size clip.
    // The recorded award owns the winning cell; decoys never touch Game's RNG.
    const int reelWidth=348,reelHeight=60,rx=cx-reelWidth/2,ry=y+129;
    const int scrollPixels=loot::reelScrollPixels(seconds);
    DrawRectangle(rx-3,ry-3,reelWidth+6,reelHeight+6,{57,48,30,255});
    DrawRectangle(rx,ry,reelWidth,reelHeight,{5,5,5,255});
    const int icon=reward->definition>=0?(evolution?evolutionIcon(game.catalog.upgrades[std::size_t(reward->definition)].pattern):iconFor(reward->definition)):-1;
    BeginScissorMode(rx*pixelScale,ry*pixelScale,reelWidth*pixelScale,reelHeight*pixelScale);
    const int first=scrollPixels/loot::CellPitch-4;
    for(int cell=first;cell<=first+8;++cell){
        const int cellX=cx+cell*loot::CellPitch-scrollPixels;
        const bool winner=cell==loot::WinningCell;
        DrawRectangle(cellX-25,ry+4,50,52,winner && landed?Color{37,31,17,255}:Color{18,18,18,255});
        outline(cellX-25,ry+4,50,52,winner && landed?accent:Color{48,48,48,255});
        if(winner && icon<0)art.chest(cellX,ry+30);
        else art.iconSized(winner?icon:loot::decoyIcon(reward->serial,cell),cellX-16,ry+14,32);
    }
    EndScissorMode();
    for(int row=0;row<5;++row){
        DrawRectangle(cx-4+row,ry+reelHeight+7-row,9-row*2,1,accent);
    }
    if(landed){
        if(pixel::width(reward->title,2)<=w-30)centered(reward->title,cx,y+202,White,2);
        else wrappedCentered(reward->title,cx,y+202,w-30,White,2,11);
        wrappedCentered(reward->description,cx,y+225,w-36,Gray,3,11);
        // Short, bounded raster rays give the landing a little emphasis.
        const float flash=std::clamp(1-(seconds-loot::RevealSeconds)/.42f,0.0f,1.0f);
        if(flash>0){
            constexpr int directions[][2]{{1,0},{1,1},{0,1},{-1,1},{-1,0},{-1,-1},{0,-1},{1,-1}};
            const int radius=20+int(std::lround((1-flash)*12));
            for(const auto& direction:directions)pixel::line(cx+direction[0]*radius,y+95+direction[1]*radius,cx+direction[0]*(radius+3),y+95+direction[1]*(radius+3),Fade(accent,flash));
        }
    }
    centered(landed?"Enter  Continue":"Enter  Skip",cx,y+h-22,
        seconds>=(landed?loot::RevealSeconds+loot::ContinueDelay:loot::IntroSeconds)?accent:Gray);
}
void Presentation::draw(float chestPreviewTime,float deathPreviewTime){
    if(settingsOpen)clicked=false;
    if(screen==Screen::Menu)menu();
    else{
        world(false,deathPreviewTime);hud();
        if(screen==Screen::Countdown){
            DrawRectangle(0,0,viewWidth,viewHeight,Fade(BLACK,.6f));centered("1",viewWidth/2,viewHeight/2-20,Orange,5);
        }else if(pendingChestReward())chestReward(chestPreviewTime>=0?chestPreviewTime:chestRevealSeconds());
        else if(game.phase==Phase::LevelUp)levelUp();
        else if(game.phase==Phase::Paused)pause();
        else if(game.phase==Phase::Victory || (game.phase==Phase::Dead && (!game.playerDied || (deathPreviewTime>=0?deathPreviewTime:deathSeconds)>=.8f)))result();
        // The source fades a red warning over the final 300 ms of grace.
        if(game.player.collisionClock>=.7f && game.phase!=Phase::Dead && game.phase!=Phase::Victory)
            DrawRectangle(0,0,viewWidth,viewHeight,Fade(Color{255,0,0,255},.3f*std::clamp((game.player.collisionClock-.7f)/.3f,0.0f,1.0f)));
    }
    if(codex)evolutionCodex();
    if(settingsOpen){
        const int x=(viewWidth-CanvasWidth)/2,y=(viewHeight-CanvasHeight)/2;
        const Vec saved=mouse;mouse.x-=float(x);mouse.y-=float(y);
        rlPushMatrix();rlTranslatef(float(x),float(y),0);drawSettings();rlPopMatrix();mouse=saved;
    }
    clicked=false;
}
void Presentation::drawWorldOnly(float deathPreviewTime){world(false,deathPreviewTime);}
}
