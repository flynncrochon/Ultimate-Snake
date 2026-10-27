#include "audio_design.hpp"
#include "chest_lottery.hpp"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {
using namespace snake;
void require(bool condition, const char* message) {if (!condition) throw std::runtime_error(message);}
void little(std::ofstream& file, std::uint32_t value, int bytes) {for (int i=0;i<bytes;++i) file.put(char((value >> (i*8)) & 255));}
void wave(const std::filesystem::path& path, const std::vector<std::int16_t>& samples) {
    std::ofstream file(path, std::ios::binary);
    if (!file) throw std::runtime_error("Cannot write audio preview");
    file.write("RIFF",4);little(file,36+std::uint32_t(samples.size()*2),4);file.write("WAVEfmt ",8);
    little(file,16,4);little(file,1,2);little(file,1,2);little(file,audio::SampleRate,4);
    little(file,audio::SampleRate*2,4);little(file,2,2);little(file,16,2);file.write("data",4);
    little(file,std::uint32_t(samples.size()*2),4);
    for(const auto sample:samples)little(file,std::uint16_t(sample),2);
    if (!file) throw std::runtime_error("Audio preview write failed");
}
std::vector<std::int16_t> demo() {
    constexpr int frames=660,perFrame=audio::SampleRate/60;
    std::vector<std::int16_t> samples(std::size_t(frames*perFrame));
    struct Playing {std::vector<std::int16_t> samples;std::size_t at=0;float volume=0;};
    std::array<Playing,audio::VoiceCount> playing;
    audio::Director director;
    for(int frame=0;frame<frames;++frame){
        if(frame<480){
            for(int event=0;event<30;++event){director.queue(Event::Shot);director.queue(Event::Hit);director.queue(Event::Kill);}
            if(frame%7==0)director.queue(Event::Pickup);
        }
        if(frame==140)director.queue(Event::Level);
        if(frame==240)director.queue(Event::Hurt);
        if(frame==300)director.queue(Event::Chest);
        if(frame==400){director.queue(Event::Chest);director.queue(Event::Evolve);}
        if(frame==510)director.queue(Event::Win);
        const auto next=director.advance(1.0f/60);
        for(std::size_t i=0;i<next.count;++i){const auto cue=next.cues[i];playing[cue.voice]={audio::synthesize(cue.effect,cue.variation),0,audio::definition(cue.effect).volume};}
        for(int i=0;i<perFrame;++i){double value=0;
            for(auto& voice:playing)if(voice.at<voice.samples.size())value+=voice.samples[voice.at++]*voice.volume;
            samples[std::size_t(frame*perFrame+i)]=std::int16_t(std::lround(std::clamp(value,-32767.0,32767.0)));
        }
    }
    return samples;
}
std::array<std::size_t,loot::WinningCell> reelCrossingSamples() {
    std::array<std::size_t,loot::WinningCell> result{};
    std::size_t count=0;
    loot::ReelTicker ticker;
    const auto end=std::size_t(std::ceil(double(loot::RevealSeconds)*audio::SampleRate));
    for(std::size_t sample=0;sample<=end;++sample){
        if(ticker.advance(float(sample)/float(audio::SampleRate))){
            require(count<result.size(),"Reel export emitted more clicks than passing squares");
            result[count++]=sample;
        }
    }
    require(count==result.size(),"Reel export lost a square crossing");
    return result;
}
std::vector<std::int16_t> reelDemo() {
    // Offline export only: the runtime prepares and reuses one short Sound.
    const auto tick=audio::synthesizeReelTick();
    const auto evolution=audio::synthesize(Event::Evolve,0);
    const auto crossings=reelCrossingSamples();
    std::vector<double> mix(std::size_t(std::ceil((double(loot::RevealSeconds)+.95)*audio::SampleRate)));
    for(std::size_t event=0;event<crossings.size();++event){
        const auto at=crossings[event];
        const auto end=event+1<crossings.size()?crossings[event+1]:mix.size();
        // Match the single click voice: a later tick replaces its tiny tail.
        for(std::size_t i=0;i<tick.size()&&at+i<end&&at+i<mix.size();++i)mix[at+i]+=tick[i]*audio::ReelTickVolume;
    }
    const auto landing=std::size_t(std::lround(double(loot::RevealSeconds)*audio::SampleRate));
    for(std::size_t i=0;i<evolution.size()&&landing+i<mix.size();++i)mix[landing+i]+=evolution[i]*audio::definition(Event::Evolve).volume;
    std::vector<std::int16_t> result(mix.size());
    for(std::size_t i=0;i<mix.size();++i)result[i]=std::int16_t(std::lround(std::clamp(mix[i],-32767.0,32767.0)));
    return result;
}
float timeAtScrollPixel(int pixel) {
    float before=0,after=loot::RevealSeconds;
    for(int iteration=0;iteration<64;++iteration){
        const float middle=(before+after)*.5f;
        if(loot::reelScrollPixels(middle)<pixel)before=middle;
        else after=middle;
    }
    require(loot::reelScrollPixels(after)==pixel,"Unable to locate the requested native reel pixel");
    return after;
}
std::size_t squaresPastMarker(float seconds) {
    constexpr int marker=400;
    std::size_t crossed=0;
    const int scroll=loot::reelScrollPixels(seconds);
    // Independent screen geometry: count each incoming square whose native
    // center has actually reached/passed the fixed marker at x=400.
    for(int cell=1;cell<=loot::WinningCell;++cell){
        const int center=marker+cell*loot::CellPitch-scroll;
        if(center<=marker)++crossed;
    }
    return crossed;
}
}

int main(int argc,char** argv) {
    int passed=0,failed=0;
    auto test=[&](const char* name,const std::function<void()>& body){try{body();++passed;std::cout<<"[PASS] "<<name<<'\n';}catch(const std::exception& e){++failed;std::cerr<<"[FAIL] "<<name<<": "<<e.what()<<'\n';}};
    test("all sound variants have quiet bounded waveforms and click-free endpoints",[]{
        for(std::size_t effect=0;effect<audio::EffectCount;++effect)for(std::size_t variant=0;variant<audio::VariationCount;++variant){
            const auto type=Event::Type(effect);const auto samples=audio::synthesize(type,variant);
            require(samples.size()==std::size_t(std::ceil(double(audio::definition(type).duration)*audio::SampleRate)),"Incorrect clip duration");
            require(samples.front()==0&&samples.back()==0,"Clip endpoint clicks");
            int peak=0,derivative=0;double energy=0;
            for(std::size_t i=0;i<samples.size();++i){peak=std::max(peak,std::abs(int(samples[i])));energy+=double(samples[i])*samples[i];if(i)derivative=std::max(derivative,std::abs(int(samples[i])-samples[i-1]));}
            require(peak>250&&peak<7209,"Clip is silent or too loud");
            require(derivative<900,"Waveform contains a harsh sample discontinuity");
            require(std::sqrt(energy/double(samples.size()))<1800,"Average clip energy is too high");
        }
    });
    test("variations are reproducible without identical repeated waveforms",[]{
        for(std::size_t effect=0;effect<audio::EffectCount;++effect){const auto type=Event::Type(effect);
            require(audio::synthesize(type,0)==audio::synthesize(type,0),"Synthesis is not deterministic");
            require(audio::synthesize(type,0)!=audio::synthesize(type,1),"Variations produce identical sounds");}
    });
    test("reel click is a quiet repeatable mechanical transient with no ringing tail",[]{
        const auto samples=audio::synthesizeReelTick();
        require(samples==audio::synthesizeReelTick(),"Reel click changed pitch or waveform between plays");
        require(samples.size()==std::size_t(std::ceil(double(audio::ReelTickDuration)*audio::SampleRate)),"Incorrect reel click duration");
        require(samples.size()<std::size_t(.03*audio::SampleRate),"Reel click is long enough to smear the fast cadence");
        require(samples.front()==0&&samples.back()==0,"Reel click has a discontinuous endpoint");
        int peak=0,derivative=0;double energy=0,earlyEnergy=0,tailEnergy=0,sum=0;
        for(std::size_t i=0;i<samples.size();++i){
            const double square=double(samples[i])*samples[i];energy+=square;sum+=samples[i];
            peak=std::max(peak,std::abs(int(samples[i])));
            if(i)derivative=std::max(derivative,std::abs(int(samples[i])-samples[i-1]));
            if(i<std::size_t(.008*audio::SampleRate))earlyEnergy+=square;
            if(i>=std::size_t(.020*audio::SampleRate))tailEnergy+=square;
        }
        require(peak>1000&&peak<7209,"Reel transient is silent or too loud");
        require(derivative<5000,"Reel click contains an excessive sample discontinuity");
        const double rms=std::sqrt(energy/double(samples.size()));
        require(rms>100&&rms<1600,"Reel transient has unsuitable sustained loudness");
        require(std::abs(sum/double(samples.size()))<50,"Reel click has a significant DC offset");
        require(earlyEnergy>energy*.90&&tailEnergy<energy*.001,"Reel click has an audible musical or ringing tail");
        require(peak*audio::ReelTickVolume<1600,"Default reel click playback lacks headroom");
    });
    test("reel clicks fire when each native square center reaches the marker",[]{
        loot::ReelTicker ticker;
        require(!ticker.advance(0)&&!ticker.advance(loot::IntroSeconds),"The stationary opening square produced a click");
        for(int cell=1;cell<=loot::WinningCell;++cell){
            const float right=timeAtScrollPixel(cell*loot::CellPitch-1);
            const float centered=timeAtScrollPixel(cell*loot::CellPitch);
            require(!ticker.advance(right),"A square clicked while its center remained one pixel right of the marker");
            require(ticker.consumed()==std::size_t(cell-1),"The audio count disagrees with squares past the marker");
            require(ticker.advance(centered)&&ticker.consumed()==std::size_t(cell),"A square reaching the marker did not click exactly once");
            require(squaresPastMarker(centered)==std::size_t(cell),"Crossing fixture did not reach the actual native marker");
            require(!ticker.advance(centered),"A repeated animation pose replayed a square click");
            require(!ticker.advance(right)&&!ticker.advance(centered),"A backward sample replayed an already consumed square");
        }
        require(ticker.consumed()==loot::WinningCell&&!ticker.advance(loot::RevealSeconds),"The resting winning square kept producing clicks");
    });
    test("render cadence keeps clicks attached to passing squares without catch-up bursts",[]{
        for(const int fps:{20,30,60,120,144,240}){
            loot::ReelTicker ticker;int emitted=0;std::size_t previous=0;
            for(int frame=0;frame<=int(std::ceil(double(loot::RevealSeconds+1)*fps));++frame){
                const float time=float(frame)/float(fps);
                const auto expected=squaresPastMarker(time);
                const bool click=ticker.advance(time);
                require(click==(expected>previous),"A click failed to coincide with a square crossing the native marker");
                if(click)++emitted;
                require(ticker.consumed()==expected,"The audio count diverged from native square positions");
                require(!ticker.advance(time),"Drawing the same pose twice replayed click feedback");
                previous=expected;
            }
            require(ticker.consumed()==loot::WinningCell&&emitted==loot::WinningCell,"A supported cadence lost or duplicated a passing square click");
        }
        loot::ReelTicker stalled;
        require(stalled.advance(100)&&stalled.consumed()==loot::WinningCell,"A long frame did not consume all crossed squares");
        require(!stalled.advance(100)&&!stalled.advance(101),"A long frame left a delayed click burst behind");
    });
    test("skip mute overlay pauses and reset never replay stale reel clicks",[]{
        loot::ReelTicker skipped;
        require(!skipped.advance(loot::RevealSeconds,true)&&skipped.consumed()==loot::WinningCell,"Skipping emitted overdue reel clicks");
        require(!skipped.advance(loot::RevealSeconds+1),"Continuing a skipped reel replayed its clicks");
        skipped.reset();
        require(skipped.consumed()==0&&!skipped.advance(loot::IntroSeconds-.001f),"A fresh reward inherited an old click clock");
        require(!skipped.advance(loot::IntroSeconds),"A fresh reward clicked before any square passed the marker");
        require(skipped.advance(timeAtScrollPixel(loot::CellPitch))&&skipped.consumed()==1,"A fresh reward lost its first passing-square click");
        loot::ReelTicker muted;
        require(!muted.advance(1,true)&&muted.consumed()>1,"Muted playback failed to consume its elapsed clicks");
        const auto held=muted.consumed();
        for(int frame=0;frame<1000;++frame)require(!muted.advance(1)&&muted.consumed()==held,"A covered/frozen reel advanced or repeated its audio");
        const float next=timeAtScrollPixel(int(held+1)*loot::CellPitch);
        require(muted.advance(next)&&muted.consumed()==held+1,"Unmuting did not wait for the next new square crossing");
        require(!muted.advance(next),"Unmuting replayed a consumed click");
    });
    test("thousands of repeated events aggregate once and never replay",[]{
        audio::Director director;for(int i=0;i<10000;++i)director.queue(Event::Shot);
        const auto first=director.advance(1.0f/144);require(first.count==1&&first.cues[0].effect==Event::Shot,"Burst did not aggregate");
        for(int frame=0;frame<144;++frame)require(director.advance(1.0f/144).count==0,"Consumed event replayed without a new simulation event");
    });
    test("dense combat feedback is limited by one shared cadence",[]{
        audio::Director director;int emitted=0;
        for(int frame=0;frame<864;++frame){for(auto effect:{Event::Shot,Event::Hit,Event::Kill,Event::Pickup})director.queue(effect);
            const auto next=director.advance(1.0f/144);require(next.count<=1,"Combat cues stack in one frame");emitted+=int(next.count);}
        require(emitted>=30&&emitted<=58,"Combat sound rate is missing or excessive");
    });
    test("important feedback can replace a quiet voice while three-voice budget holds",[]{
        audio::Director director;director.queue(Event::Kill);
        const auto quiet=director.advance(.01f);require(quiet.count==1,"Fixture did not occupy a quiet voice");
        director.queue(Event::Level);director.queue(Event::Hurt);
        const auto busy=director.advance(.01f);require(busy.count==2,"Fixture did not occupy all voices");
        director.queue(Event::Win);const auto win=director.advance(.01f);
        require(win.count==1&&win.cues[0].effect==Event::Win&&win.cues[0].voice==quiet.cues[0].voice,"Victory failed to replace the least important voice");
    });
    test("reward events resolve to one chord and leave space around it",[]{
        audio::Director director;for(auto effect:{Event::Level,Event::Chest,Event::Evolve,Event::Hit})director.queue(effect);
        const auto reward=director.advance(.01f);require(reward.count==1&&reward.cues[0].effect==Event::Evolve,"Evolution stacked multiple reward cues");
        director.queue(Event::Shot);director.queue(Event::Pickup);require(director.advance(.1f).count==0,"Combat sound masks the reward tail");
        director.advance(.8f);director.queue(Event::Hit);require(director.advance(.01f).count==1,"Combat did not resume after reward ducking");
    });
    test("mute and reset discard queued feedback rather than playing it late",[]{
        audio::Director director;director.queue(Event::Evolve);require(director.advance(.01f,false).count==0,"Muted director played a cue");
        require(director.advance(1).count==0,"Muted cue played after unmute");
        director.queue(Event::Chest);director.reset();require(director.advance(1).count==0,"Run reset retained a queued reward");
    });
    test("dense combat and rewards retain ample mix headroom",[]{
        const auto samples=demo();int peak=0;double energy=0;
        for(const auto sample:samples){peak=std::max(peak,std::abs(int(sample)));energy+=double(sample)*sample;}
        require(peak>500&&peak<8192,"Realistic mix has insufficient headroom or no feedback");
        require(std::sqrt(energy/double(samples.size()))<1000,"Dense mix remains too loud on average");
    });
    if(argc==3&&std::string(argv[1])=="--export"){
        const std::filesystem::path path=argv[2];std::filesystem::create_directories(path);
        for(std::size_t i=0;i<audio::EffectCount;++i){const auto type=Event::Type(i);auto clip=audio::synthesize(type);
            for(auto& sample:clip)sample=std::int16_t(std::lround(sample*audio::definition(type).volume));
            wave(path/(std::string(audio::definition(type).name)+".wav"),clip);}
        auto reelTick=audio::synthesizeReelTick();
        for(auto& sample:reelTick)sample=std::int16_t(std::lround(sample*audio::ReelTickVolume));
        wave(path/"reel_tick.wav",reelTick);
        wave(path/"reel_open_evolution.wav",reelDemo());
        std::ofstream timing(path/"reel_tick_times.json");
        require(bool(timing),"Cannot write reel crossing timings");
        timing<<std::setprecision(9)<<'[';
        const auto crossings=reelCrossingSamples();
        for(std::size_t i=0;i<crossings.size();++i){if(i)timing<<',';timing<<double(crossings[i])/audio::SampleRate;}
        timing<<"]\n";
        require(bool(timing),"Reel crossing timing export failed");
        wave(path/"combat_and_rewards.wav",demo());std::cout<<"Audio previews exported to "<<std::filesystem::absolute(path).string()<<'\n';
    }
    std::cout<<passed<<" passed, "<<failed<<" failed.\n";return failed?1:0;
}
