#include "presentation.hpp"

#include <algorithm>
#include <iomanip>
#include <sstream>

namespace snake {
namespace {
constexpr Color White{255,255,255,255},Gray{190,190,190,255},Dim{85,85,85,255};
constexpr Color Green{68,255,68,255},Yellow{255,255,0,255},Border{68,68,68,255};
constexpr int Width=600,Height=420,RowTop=88,RowPitch=26;
bool within(Vec p,int x,int y,int w,int h){return p.x>=x && p.y>=y && p.x<x+w && p.y<y+h;}
std::string effect(PermanentUpgrade upgrade,int rank){
    switch(upgrade){
    case PermanentUpgrade::Might:return "+"+std::to_string(rank*5)+"% eligible damage";
    case PermanentUpgrade::Vitality:{
        constexpr double health[]{100,110,121,133.1};std::ostringstream text;
        text<<std::fixed<<std::setprecision(1)<<health[rank]<<" starting / maximum HP";return text.str();
    }
    case PermanentUpgrade::Armor:return std::to_string(rank)+" less contact damage (minimum hit: 3)";
    case PermanentUpgrade::Recovery:{std::ostringstream text;text<<std::fixed<<std::setprecision(1)<<.1*rank<<" HP restored per simulation second";return text.str();}
    case PermanentUpgrade::Cooldown:return "-"+std::to_string(rank*25/10)+(rank%2?".5":"")+"% cooldown";
    case PermanentUpgrade::Growth:return "+"+std::to_string(rank*3)+"% experience";
    case PermanentUpgrade::Reroll:return std::to_string(3+2*rank)+" rerolls each run";
    case PermanentUpgrade::Banish:return std::to_string(3+2*rank)+" banishes each run";
    default:return {};
    }
}
}
void Presentation::openPermanentUpgrades(){
    if(screen!=Screen::Menu)return;
    permanentOpen=true;codex=false;permanentFocus=0;permanentMessage.clear();clicked=false;resetAudio();
}
void Presentation::closePermanentUpgrades(){permanentOpen=false;clicked=false;}
bool Presentation::purchaseUpgrade(PermanentUpgrade upgrade){
    if(screen!=Screen::Menu || !permanentOpen || std::size_t(upgrade)>=PermanentUpgradeCount)return false;
    if(profileStorage.state()==ProfileSaveState::Blocked){permanentMessage="Profile unreadable. Progress is temporary.";return false;}
    const auto& definition=permanentDefinitions()[std::size_t(upgrade)];
    const int price=permanentPrice(profile.upgrades,upgrade);
    if(price==0){permanentMessage="Already at maximum rank.";return false;}
    if(!purchasePermanent(profile,upgrade)){
        permanentMessage="Need "+std::to_string(std::max<std::int64_t>(0,price-profile.gold))+" more gold.";return false;
    }
    permanentMessage=std::string(definition.name)+" purchased. Applies next run.";saveProfile();return true;
}
bool Presentation::refundUpgrades(){
    if(screen!=Screen::Menu || !permanentOpen || profileStorage.state()==ProfileSaveState::Blocked)return false;
    const auto gold=refundPermanent(profile);
    if(gold==0){permanentMessage="Nothing to refund.";return false;}
    permanentMessage="All upgrades refunded: "+std::to_string(gold)+" gold.";saveProfile();return true;
}
void Presentation::retryProfileSave(){
    if(profileStorage.state()==ProfileSaveState::Blocked)return;
    saveProfile();permanentMessage="Save retry requested.";
}
void Presentation::updatePermanentUpgrades(bool mouseMoved){
    constexpr int Rows=int(PermanentUpgradeCount),Actions=3;
    const int x=(viewWidth-Width)/2,y=(viewHeight-Height)/2;
    const bool retry=profileStorage.state()==ProfileSaveState::Failed;
    if(!retry && permanentFocus==Rows+1)permanentFocus=Rows+2;
    if(IsKeyPressed(KEY_ESCAPE)){closePermanentUpgrades();return;}
    if(IsKeyPressed(KEY_DOWN)){do{permanentFocus=(permanentFocus+1)%(Rows+Actions);}while(!retry && permanentFocus==Rows+1);}
    if(IsKeyPressed(KEY_UP)){do{permanentFocus=(permanentFocus+Rows+Actions-1)%(Rows+Actions);}while(!retry && permanentFocus==Rows+1);}
    if(mouseMoved){
        for(int i=0;i<Rows;++i)if(within(mouse,x+20,y+RowTop+i*RowPitch,Width-40,24))permanentFocus=i;
        for(int i=0;i<Actions;++i)if((i!=1 || retry) && within(mouse,x+32+i*188,y+350,160,24))permanentFocus=Rows+i;
    }
    int selected=-1;
    if(clicked){
        for(int i=0;i<Rows;++i)if(within(mouse,x+20,y+RowTop+i*RowPitch,Width-40,24))selected=i;
        for(int i=0;i<Actions;++i)if((i!=1 || retry) && within(mouse,x+32+i*188,y+350,160,24))selected=Rows+i;
    }
    if(IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE))selected=permanentFocus;
    if(selected>=0){
        permanentFocus=selected;
        if(selected<Rows)purchaseUpgrade(PermanentUpgrade(selected));
        else if(selected==Rows)refundUpgrades();
        else if(selected==Rows+1)retryProfileSave();
        else closePermanentUpgrades();
    }
    clicked=false;
}
void Presentation::drawPermanentUpgrades(){
    const int x=(viewWidth-Width)/2,y=(viewHeight-Height)/2;
    DrawRectangle(0,0,viewWidth,viewHeight,{0,0,0,225});
    DrawRectangle(x,y,Width,Height,{12,12,12,255});outline(x,y,Width,Height,Border);
    centered("SHEDSKIN UPGRADES",viewWidth/2,y+16,White,2);
    centered("Gold: "+std::to_string(profile.gold),viewWidth/2,y+46,Yellow);
    label("UPGRADE",x+28,y+71,Gray);label("RANK",x+280,y+71,Gray);
    label("NEXT COST",x+Width-28-pixel::width("NEXT COST"),y+71,Gray);
    const auto& definitions=permanentDefinitions();
    for(std::size_t i=0;i<definitions.size();++i){
        const auto upgrade=PermanentUpgrade(i);const int rank=profile.upgrades.rank(upgrade),price=permanentPrice(profile.upgrades,upgrade);
        const bool selected=int(i)==permanentFocus,maxed=rank==definitions[i].maxRank;
        const int top=y+RowTop+int(i)*RowPitch;
        if(selected)DrawRectangle(x+20,top,Width-40,24,{24,24,24,255});
        outline(x+20,top,Width-40,24,selected?White:Border);
        label(definitions[i].name,x+28,top+8,selected?White:Gray);
        label(std::to_string(rank)+" / "+std::to_string(definitions[i].maxRank),x+280,top+8,maxed?Green:White);
        const std::string cost=maxed?"MAX":std::to_string(price)+" G";
        label(cost,x+Width-28-pixel::width(cost),top+8,maxed?Green:profile.gold>=price?White:Dim);
    }
    if(permanentFocus<int(PermanentUpgradeCount)){
        const auto upgrade=PermanentUpgrade(permanentFocus);const auto& definition=definitions[std::size_t(upgrade)];
        centered(definition.description,viewWidth/2,y+302,Gray);
        centered("Current: "+effect(upgrade,profile.upgrades.rank(upgrade)),viewWidth/2,y+316,White);
    }else centered("Bonuses apply to all characters when a new run begins.",viewWidth/2,y+309,Gray);
    const auto state=profileStorage.state();
    const std::string message=state==ProfileSaveState::Blocked?"Profile unreadable. Progress is temporary.":
        state==ProfileSaveState::Failed?"Save failed. Retry saving to keep this progress.":
        state==ProfileSaveState::Saving || profileSavePending?"Saving progress...":permanentMessage;
    centered(message,viewWidth/2,y+332,state==ProfileSaveState::Failed || state==ProfileSaveState::Blocked?Yellow:Gray);
    button("REFUND ALL",x+32,y+350,160,24,permanentFocus==int(PermanentUpgradeCount));
    if(state==ProfileSaveState::Failed)button("RETRY SAVE",x+220,y+350,160,24,permanentFocus==int(PermanentUpgradeCount)+1);
    button("BACK",x+408,y+350,160,24,permanentFocus==int(PermanentUpgradeCount)+2);
    centered("Up / Down  Select    Enter  Buy    Esc  Back",viewWidth/2,y+389,Gray);
    centered("Keep earned gold after death. Refund anytime.",viewWidth/2,y+404,Dim);
}
}
