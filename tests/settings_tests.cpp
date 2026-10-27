#include "settings.hpp"
#include <cmath>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>

namespace {
void require(bool condition,const char* message){if(!condition)throw std::runtime_error(message);}
struct Fixture {
    std::filesystem::path directory=std::filesystem::temp_directory_path()/("snake-settings-regression-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    Fixture(){std::filesystem::create_directories(directory);}
    ~Fixture(){std::error_code ec;std::filesystem::remove(directory/"settings.json",ec);std::filesystem::remove(directory/"settings.json.tmp",ec);std::filesystem::remove(directory,ec);}
    std::string path()const{return (directory/"settings.json").string();}
    void write(const std::string& value){std::ofstream(path(),std::ios::binary|std::ios::trunc)<<value;}
};
}
int main(){
    using namespace snake;int passed=0,failed=0;
    auto test=[&](const char* name,const std::function<void()>& run){try{run();++passed;std::cout<<"PASS "<<name<<'\n';}catch(const std::exception& e){++failed;std::cerr<<"FAIL "<<name<<": "<<e.what()<<'\n';}};
    test("display audio effects and bindings survive an atomic settings round trip",[]{
        Fixture f;Settings original;original.display.mode=WindowMode::Borderless;original.display.monitor=2;original.display.targetFps=165;original.display.vsync=false;original.display.integerScale=2;
        original.audio.master=.37f;original.audio.combat=.22f;original.render.damageNumbers=false;original.render.quality=1;
        require(bindKey(original.controls,Action::Pause,340),"A valid new control was rejected");std::string error;
        require(saveSettings(f.path(),original,&error),error.c_str());Settings loaded;require(loadSettings(f.path(),loaded,&error),error.c_str());require(original==loaded,"Saved settings did not round trip");
        original.display.width=1280;require(saveSettings(f.path(),original,&error),"Replacing an existing settings file failed");require(loadSettings(f.path(),loaded)&&loaded==original,"Replacement did not preserve the latest settings");
        require(!std::filesystem::exists(f.directory/"settings.json.tmp"),"A successful save retained its temporary file");
        const auto unicode=f.directory/std::filesystem::u8path(u8"\u8a2d\u5b9a.json");
        require(saveSettings(unicode.u8string(),original)&&loadSettings(unicode.u8string(),loaded)&&loaded==original,"Unicode settings filenames did not round trip");
        std::filesystem::remove(unicode);
    });
    test("missing malformed oversized and future configs return safe defaults",[]{
        Fixture f;Settings s;s.display.width=9;std::string error;std::filesystem::remove(f.path());require(!loadSettings(f.path(),s,&error)&&s==defaultSettings(),"Missing settings did not use defaults");
        for(const std::string input:{std::string("{bad"),std::string("[]"),std::string("{\"version\":99}"),std::string(70000,' ')}){
            f.write(input);s.display.width=1;require(!loadSettings(f.path(),s,&error)&&s==defaultSettings()&&!error.empty(),"Invalid configuration did not fail safely");
        }
    });
    test("legacy controls retain every surviving binding when the dash slot is removed",[]{
        Fixture f;
        for(const std::string version:{std::string("\"version\":1,"),std::string()}){
            f.write("{"+version+R"("display":{"target_fps":165,"vsync":false},"audio":{"master":0.37},"render":{"particles":false},"controls":{"keys":[73,75,74,76,80,80,300,301,78,84,89],"alternate_arrows":false}})");
            Settings migrated;require(loadSettings(f.path(),migrated),"Legacy settings were rejected");
            ControlSettings expected;expected.keys={{73,75,74,76,80,300,301,78,84,89}};expected.alternateArrows=false;
            require(migrated.controls==expected,"Removing the legacy action shifted or reset surviving bindings");
            require(migrated.display.targetFps==165&&!migrated.display.vsync&&std::abs(migrated.audio.master-.37f)<.001f&&!migrated.render.particles,"Control migration changed unrelated preferences");
            require(saveSettings(f.path(),migrated),"Migrated settings could not be saved");Settings reloaded;
            require(loadSettings(f.path(),reloaded)&&reloaded==migrated,"Migrated settings changed on their next load");
        }
    });
    test("partial typed settings preserve valid fields and clamp numeric ranges",[]{
        Fixture f;f.write(R"({"display":{"width":200,"height":999999,"target_fps":-50,"monitor":-4,"vsync":"false"},"audio":{"master":9,"combat":-2},"render":{"quality":9,"particles":false},"controls":{"keys":[87,87,65,68,32,256,299,258,77,82,66]}})");
        Settings s;require(loadSettings(f.path(),s),"A partial supported configuration was rejected");
        require(s.display.width==800&&s.display.height==4320&&s.display.targetFps==30&&s.display.monitor==0&&s.display.vsync,"Display validation accepted invalid fields");
        require(s.audio.master==1&&s.audio.combat==0&&s.render.quality==2&&!s.render.particles,"Audio/effect validation lost valid values");
        require(s.controls.keys==ControlSettings{}.keys,"Duplicate key bindings were not restored safely");
        s.audio.effects=std::numeric_limits<float>::quiet_NaN();sanitizeSettings(s);require(std::isfinite(s.audio.effects)&&s.audio.effects==1,"Non-finite volume escaped validation");
    });
    test("VSync display limits uncapped and background pacing do not compete",[]{
        DisplaySettings d;d.targetFps=120;require(effectiveFrameLimit(d,144)==0,"VSync competed with a software limiter");
        d.vsync=false;d.limitToDisplay=false;d.targetFps=0;require(effectiveFrameLimit(d,144)==0,"Uncapped selection was limited");
        d.limitToDisplay=true;require(effectiveFrameLimit(d,144)==144,"Display limit did not cap uncapped rendering");
        d.targetFps=360;require(effectiveFrameLimit(d,144)==144,"Display limit allowed invisible excess frames");
        d.targetFps=90;require(effectiveFrameLimit(d,144)==90,"A slower explicit cap was ignored");
        d.vsync=true;d.backgroundFps=30;require(effectiveFrameLimit(d,144,false)==30&&effectiveFrameLimit(d,144,true,true)==30,"Background pacing ignored its cap");
        require(effectiveFrameLimit(d,0)==0,"Missing monitor metadata did not preserve VSync ownership");
    });
    test("integer canvas scale always fits while preserving square pixels",[]{
        DisplaySettings d;require(canvasScale(d,1920,1080)==2&&canvasScale(d,800,500)==1,"Auto pixel scaling did not fit the canvas");
        d.integerScale=3;require(canvasScale(d,1600,1000)==2,"Manual scale overflowed the window");d.integerScale=1;require(canvasScale(d,3840,2160)==1,"A requested 1x scale was ignored");
        require(canvasScale(d,100,100)==1,"A minimized/small client produced zero scale");
    });
    test("conflicting bindings fail without losing either action",[]{
        ControlSettings controls;const auto before=controls;std::string error;
        require(!bindKey(controls,Action::Pause,controls.keys[std::size_t(Action::Up)],&error)&&controls==before&&!error.empty(),"Conflicting rebind mutated controls");
        require(!bindKey(controls,Action::Pause,0)&&controls==before,"A non-key binding was accepted");
        require(bindKey(controls,Action::Pause,340)&&controls.keys[std::size_t(Action::Pause)]==340,"A valid binding failed");
        require(bindKey(controls,Action::Pause,32),"Space was not available for rebinding");
    });
    test("dangerous display changes roll back after fifteen real seconds",[]{
        DisplaySettings initial;DisplayTrial trial(initial);auto next=initial;next.mode=WindowMode::Fullscreen;next.monitor=1;
        require(trial.apply(next)&&trial.pending()&&trial.secondsRemaining()==15,"Fullscreen preview did not require confirmation");
        require(!trial.advance(14.5f)&&trial.pending(),"Preview reverted early");require(trial.advance(.5f)&&!trial.pending()&&trial.current()==initial,"Timeout did not restore the exact previous display");
        require(!trial.advance(20),"Completed rollback repeated");
    });
    test("confirmation cancellation and non-display edits preserve committed state",[]{
        DisplaySettings initial;DisplayTrial trial(initial);auto next=initial;next.width=1280;next.height=800;
        trial.apply(next);require(trial.confirm()&&!trial.pending()&&!trial.advance(20)&&trial.current()==next,"Confirmed display settings reverted");
        auto fullscreen=next;fullscreen.mode=WindowMode::Fullscreen;trial.apply(fullscreen);require(trial.revert()&&trial.current()==next,"Cancel did not restore the latest committed window size");
        auto pacing=next;pacing.vsync=false;pacing.targetFps=240;trial.apply(pacing);require(!trial.pending()&&trial.current()==pacing,"A frame-rate edit unnecessarily requested display confirmation");
        trial.recordWindowedSize(960,600);require(trial.current().width==960&&trial.current().height==600,"Manual window resize was not remembered");
        auto pending=trial.current();pending.mode=WindowMode::Borderless;trial.apply(pending);trial.recordWindowedSize(1920,1080);require(trial.revert()&&trial.current().width==960,"A preview resize replaced the safe rollback size");
    });
    std::cout<<passed<<" passed, "<<failed<<" failed.\n";return failed?1:0;
}
