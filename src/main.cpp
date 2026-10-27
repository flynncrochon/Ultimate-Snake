#include "presentation.hpp"
#include "chest_lottery.hpp"
#ifndef __EMSCRIPTEN__
#include "game_test_api.hpp"
#else
#include "web_platform.hpp"
#include <emscripten.h>
#endif
#include <memory>
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {
struct Options {
    bool smoke = false;
    bool testApi = false;
    int frames = 0;
    std::string scene, capture, data, settings;
};
Options parse(int argc, char** argv) {
    Options o;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        auto value = [&]() -> std::string { if (++i >= argc) throw std::runtime_error("Missing value after " + arg); return argv[i]; };
        if (arg == "--smoke-test") { o.smoke = true; o.frames = 300; o.scene = "gameplay"; }
        else if(arg=="--test-api")o.testApi=true;
        else if (arg == "--frames") o.frames = std::stoi(value());
        else if (arg == "--scene") o.scene = value();
        else if (arg == "--capture") o.capture = value();
        else if (arg == "--data") o.data = value();
        else if (arg == "--settings") o.settings = value();
        else if (arg == "--help") {
            std::cout << "Snake Survivors\n  --test-api         Serve deterministic JSONL test commands, with real native captures\n  --smoke-test       Run 300 deterministic native frames and exit\n  --scene NAME       menu, gameplay, settings, upgrades, evolutions, pause, results, nest-flight, nest-hatch, nest-brood\n  --capture PATH     Save a PNG of the rendered scene and exit\n  --frames N         Exit after N frames\n  --data DIRECTORY   Override the CSV catalog directory\n  --settings FILE    Override the persistent settings JSON file\n";
            std::exit(0);
        } else throw std::runtime_error("Unknown argument: " + arg);
    }
    if (!o.capture.empty() && o.frames == 0) o.frames = 2;
    return o;
}
std::string dataDirectory(const Options& o, const char* executable) {
    namespace fs = std::filesystem;
    if (!o.data.empty()) return o.data;
#ifdef __EMSCRIPTEN__
    (void)executable;
    return "/data";
#else
    const fs::path beside = fs::absolute(executable).parent_path() / "data";
    if (fs::exists(beside / "upgrades.csv")) return beside.string();
    if (fs::exists("data/upgrades.csv")) return "data";
    throw std::runtime_error("Could not find data/upgrades.csv. Keep the data folder beside the executable or pass --data DIRECTORY.");
#endif
}
}

namespace {
// One frame implementation serves both the native loop and browser callbacks.
// All callback state lives on the heap; no browser callback captures main's stack.
struct Application {
    Options options;
    snake::Catalog catalog;
    bool automated;
    std::filesystem::path executableDirectory;
    std::string settingsPath;
    snake::Settings lastSaved;
    snake::DisplayManager display;
    RenderTexture2D canvas{};
    std::unique_ptr<snake::Presentation> presentation;
    double accumulator=0;
    bool hasMotionHistory=false,previousBackgroundPause=false;
    int frame=0;

    Application(const Options& requested,const char* executable)
        :options(requested),catalog(snake::Catalog::load(dataDirectory(options,executable))),
         automated(options.smoke || options.testApi || options.frames>0 || !options.capture.empty()),
         executableDirectory(std::filesystem::absolute(executable).parent_path()),display(!automated) {
#ifdef __EMSCRIPTEN__
        settingsPath=snake::web::SettingsPath;
#else
        settingsPath=options.settings.empty()?(executableDirectory/"snake-settings.json").string():options.settings;
#endif
    }
    ~Application(){
        // GPU/audio owners must die while the context is still alive, including
        // partial initialization failures and an explicit end of a browser run.
        presentation.reset();
        if(canvas.id)UnloadRenderTexture(canvas);
        if(IsWindowReady())CloseWindow();
    }
    bool persistSettings(std::string& error){
        if(automated)return true;
        if(!snake::saveSettings(settingsPath,presentation->settings,&error))return false;
        lastSaved=presentation->settings;
#ifdef __EMSCRIPTEN__
        snake::web::flushSaves();
#endif
        return true;
    }
    void initialize(){
        snake::Settings initialSettings=snake::defaultSettings();std::string settingsError;
        if(!automated || !options.settings.empty()){
            if(!snake::loadSettings(settingsPath,initialSettings,&settingsError) && !std::filesystem::exists(settingsPath)){
                // Preserve the previous sound preference when upgrading an old build.
                std::ifstream legacy(executableDirectory/"snake-survivors.save");std::string version;int kills,runs,seconds,wins;bool sound;
                if(legacy>>version>>kills>>runs>>seconds>>wins>>sound && version=="SS1")initialSettings.audio.enabled=sound;
            }
        }
        SetTraceLogLevel(options.testApi?LOG_NONE:LOG_WARNING);
        unsigned flags=FLAG_WINDOW_RESIZABLE;
#ifndef __EMSCRIPTEN__
        if(!automated && initialSettings.display.vsync)flags|=FLAG_VSYNC_HINT;
        if(automated)flags|=FLAG_WINDOW_HIDDEN;
#endif
        SetConfigFlags(flags);
        InitWindow(initialSettings.display.width,initialSettings.display.height,"Snake Survivors");
        if(!IsWindowReady())throw std::runtime_error("Unable to create the raylib OpenGL window.");
        SetWindowMinSize(snake::CanvasWidth,snake::CanvasHeight);
        SetExitKey(KEY_NULL);SetTargetFPS(0);
        std::string displayError;
        if(!display.initialize(initialSettings.display,&displayError))throw std::runtime_error(displayError);
        initialSettings.display=display.current();
        canvas=LoadRenderTexture(snake::CanvasWidth,snake::CanvasHeight);
        if(!canvas.id)throw std::runtime_error("Unable to create the source framebuffer.");
        SetTextureFilter(canvas.texture,TEXTURE_FILTER_POINT);
        presentation=std::make_unique<snake::Presentation>(catalog,initialSettings);
        auto& app=*presentation;
        app.displayOptions=&display;lastSaved=initialSettings;
        app.onApplySettings=[this](const snake::Settings& requested,std::string& error){
            auto& app=*presentation;
            const auto previous=app.settings;auto next=requested;snake::sanitizeSettings(next);
            if(!display.apply(next.display,true,&error))return false;
            next.display=display.current();app.settings=next;
            if(!display.confirmationPending()&&!persistSettings(error)){
                display.apply(previous.display,false);app.settings=previous;return false;
            }
            app.sound=next.audio.enabled;
            if(!app.sound || !(previous.audio==next.audio))app.resetAudio();
            return true;
        };
        app.onConfirmSettings=[this]{
            std::string error;
            if(persistSettings(error))display.confirm();
            else {display.revert();presentation->settings.display=display.current();presentation->setSettingsFeedback(error);}
        };
        app.onRevertSettings=[this]{
            display.revert();presentation->settings.display=display.current();std::string error;
            if(!persistSettings(error))presentation->setSettingsFeedback(error);
        };
        app.displayConfirmationPending=[this]{return display.confirmationPending();};
        app.displayConfirmationSeconds=[this]{return display.confirmationSeconds();};
        if(!settingsError.empty())app.setSettingsFeedback(settingsError);
        if(!options.scene.empty())app.captureScene(options.scene);
    }
    bool running()const{
        // raylib's web WindowShouldClose sleeps for a synchronous/Asyncify
        // loop. Browser callbacks must never call it or block the event loop.
#ifndef __EMSCRIPTEN__
        if(WindowShouldClose())return false;
#endif
        return !presentation->quit && (!options.frames || frame<options.frames);
    }
    void tick(){
        auto& app=*presentation;
#ifdef __EMSCRIPTEN__
        const bool minimized=snake::web::pageHidden();
        const bool focused=!minimized && IsWindowFocused();
#else
        const bool focused=automated || IsWindowFocused(),minimized=!automated && IsWindowMinimized();
#endif
        const float dt = options.frames ? snake::FixedStep : std::min(GetFrameTime(),.25f);
        const bool reverted=display.update(dt,focused,minimized);
        app.settings.display=display.current();
        if(reverted){app.settingsDraft=app.settings;std::string error;if(!persistSettings(error))app.setSettingsFeedback(error);}
        const bool backgroundPaused=app.settings.display.pauseUnfocused&&(!focused||minimized);
        const int scale = snake::canvasScale(app.settings.display,GetScreenWidth(),GetScreenHeight());
        const int offsetX = (GetScreenWidth() - snake::CanvasWidth * scale) / 2;
        const int offsetY = (GetScreenHeight() - snake::CanvasHeight * scale) / 2;
        const Vector2 cursor = GetMousePosition();
        const snake::Vec mouse{(cursor.x - float(offsetX)) / float(scale), (cursor.y - float(offsetY)) / float(scale)};
        // UI uses real frame time. Simulation uses a fixed 60 Hz accumulator.
        const bool wasPlaying = !backgroundPaused && !previousBackgroundPause && app.screen == snake::Screen::Game && !app.simulationBlocked() && app.game.phase == snake::Phase::Playing;
        previousBackgroundPause=backgroundPaused;
        app.update(dt, mouse, IsMouseButtonPressed(MOUSE_BUTTON_LEFT));
        app.queueAudioEvents();
        if (!backgroundPaused && app.screen == snake::Screen::Game && !app.simulationBlocked() && app.game.phase == snake::Phase::Playing) {
            snake::Input input;
            const auto& keys=app.settings.controls.keys;
            const bool arrows=app.settings.controls.alternateArrows;
            if (IsKeyDown(keys[std::size_t(snake::Action::Right)]) || (arrows&&IsKeyDown(KEY_RIGHT))) input.direction = {1,0};
            else if (IsKeyDown(keys[std::size_t(snake::Action::Left)]) || (arrows&&IsKeyDown(KEY_LEFT))) input.direction = {-1,0};
            else if (IsKeyDown(keys[std::size_t(snake::Action::Up)]) || (arrows&&IsKeyDown(KEY_UP))) input.direction = {0,-1};
            else if (IsKeyDown(keys[std::size_t(snake::Action::Down)]) || (arrows&&IsKeyDown(KEY_DOWN))) input.direction = {0,1};
            if (options.smoke) {
                const int direction = (frame / 60) % 4;
                constexpr snake::Vec path[]{{1,0},{0,1},{-1,0},{0,-1}};
                input.direction = path[direction];
            }
            // Capture scenes remain still; smoke mode exercises the live simulation.
            if (options.capture.empty() || options.smoke) {
                if(wasPlaying)accumulator += dt;
                int steps = 0;
                while (accumulator >= snake::FixedStep && steps++ < 15 && app.game.phase == snake::Phase::Playing && !app.simulationBlocked()) {
                    app.beginSimulationStep();hasMotionHistory=true;
                    app.game.update(snake::FixedStep, input);
                    app.queueAudioEvents();
                    accumulator -= snake::FixedStep;
                }
                if(app.simulationBlocked() || app.game.phase!=snake::Phase::Playing)accumulator=std::min(accumulator,double(snake::FixedStep));
            }
        } else {
            // Pauses and reward reveals retain the last fractional
            // pose. Resetting it here caused a small backward snap.
            if(app.screen!=snake::Screen::Game){accumulator=0;hasMotionHistory=false;}
            if (options.smoke && app.game.phase == snake::Phase::LevelUp){app.game.choose(0);app.queueAudioEvents();}
            if (options.smoke && app.pendingChestReward())app.acknowledgeChestReward();
        }
        if(wasPlaying && (backgroundPaused || app.screen!=snake::Screen::Game || app.game.phase==snake::Phase::Paused || app.codex || app.settingsOpen))app.resetAudio();
        app.setRenderFrame(float(accumulator/snake::FixedStep),hasMotionHistory);
        app.updateAudio(dt);
        if(minimized){BeginDrawing();EndDrawing();++frame;return;}
        BeginTextureMode(canvas);
        ClearBackground(BLACK);
        app.draw();
        EndTextureMode();
        BeginDrawing();
        ClearBackground(BLACK);
        const int outputScale=snake::canvasScale(app.settings.display,GetScreenWidth(),GetScreenHeight());
        const int outputX=(GetScreenWidth()-snake::CanvasWidth*outputScale)/2,outputY=(GetScreenHeight()-snake::CanvasHeight*outputScale)/2;
        DrawTexturePro(canvas.texture, {0,0,float(snake::CanvasWidth),-float(snake::CanvasHeight)},
            {float(outputX),float(outputY),float(snake::CanvasWidth * outputScale),float(snake::CanvasHeight * outputScale)}, {0,0},0,WHITE);
        EndDrawing();
        ++frame;
    }
#ifndef __EMSCRIPTEN__
    void serveTests(){
        auto& app=*presentation;
        snake::GameTestSession session(app.game,[&](const snake::TestJson& request)->snake::TestJson{
            const std::string path=request.value("path",std::string("artifacts/gametest/capture.png"));
            const bool worldOnly=request.value("world_only",true);
            const bool interpolated=request.contains("render_alpha");
            const float renderAlpha=request.value("render_alpha",1.0f);
            if(!std::isfinite(renderAlpha) || renderAlpha<0 || renderAlpha>1)throw std::invalid_argument("render_alpha must be in [0,1]");
            if(request.contains("reward_time") && !request.at("reward_time").is_number())throw std::invalid_argument("reward_time must be a number in [0,30]");
            const float rewardSeconds=request.value("reward_time",app.chestRevealSeconds());
            if(!std::isfinite(rewardSeconds) || rewardSeconds<0 || rewardSeconds>30)throw std::invalid_argument("reward_time must be in [0,30]");
            if(request.contains("death_time") && !request.at("death_time").is_number())throw std::invalid_argument("death_time must be a number in [0,2]");
            const float deathSeconds=request.value("death_time",-1.0f);
            if(request.contains("death_time") && (!std::isfinite(deathSeconds) || deathSeconds<0 || deathSeconds>2))throw std::invalid_argument("death_time must be in [0,2]");
            const auto parent=std::filesystem::path(path).parent_path();
            if(!parent.empty())std::filesystem::create_directories(parent);
            app.screen=snake::Screen::Game;app.codex=false;
            app.setRenderFrame(renderAlpha,interpolated);
            BeginTextureMode(canvas);ClearBackground(BLACK);
            if(worldOnly)app.drawWorldOnly(deathSeconds);else app.draw(rewardSeconds,deathSeconds);
            EndTextureMode();
            Image capture=LoadImageFromTexture(canvas.texture);ImageFlipVertical(&capture);
            const bool written=ExportImage(capture,path.c_str());UnloadImage(capture);
            if(!written)throw std::runtime_error("Unable to write native capture: "+path);
            return {{"path",std::filesystem::absolute(path).string()},{"width",snake::CanvasWidth},{"height",snake::CanvasHeight},{"reward_open",app.pendingChestReward()!=nullptr},
                {"reward_time",rewardSeconds},{"reward_revealed",app.pendingChestReward()!=nullptr && rewardSeconds>=snake::loot::RevealSeconds}};
        });
        session.setStepObserver([&]{app.beginSimulationStep();});
        session.serve(std::cin,std::cout);
    }
#endif
    void finish(){
        auto& app=*presentation;
        if (!options.capture.empty()) {
            const auto parent = std::filesystem::path(options.capture).parent_path();
            if (!parent.empty()) std::filesystem::create_directories(parent);
            Image shot = LoadImageFromTexture(canvas.texture);
            ImageFlipVertical(&shot);
            if (!ExportImage(shot, options.capture.c_str())) throw std::runtime_error("Could not export screenshot: " + options.capture);
            UnloadImage(shot);
        }
        if (options.smoke) {
            if (!std::isfinite(app.game.player.position.x) || !std::isfinite(app.game.elapsed)) throw std::runtime_error("Non-finite simulation state.");
            std::cout << "Native smoke test passed: " << frame << " frames, " << app.game.enemies.size() << " enemies, " << app.game.projectiles.size() << " projectiles, " << app.game.nestEggs.size() << " eggs, " << app.game.miniSnakes.size() << " summons, " << app.game.kills << " kills.\n";
        }
        if(display.confirmationPending()){display.revert();app.settings.display=display.current();}
        if(!automated && app.settings!=lastSaved){std::string error;if(!persistSettings(error))TraceLog(LOG_WARNING,"%s",error.c_str());}
    }
};
#ifdef __EMSCRIPTEN__
void browserFrame(void* context){
    auto* app=static_cast<Application*>(context);
    try{
        if(app->running()){app->tick();return;}
        app->finish();
    }catch(const std::exception& error){
        std::cerr<<"Snake Survivors: "<<error.what()<<'\n';
        snake::web::showError(error.what());
    }
    emscripten_cancel_main_loop();
    delete app;
}
#endif
}

int main(int argc,char** argv){
    try{
        auto app=std::make_unique<Application>(parse(argc,argv),argv[0]);
        app->initialize();
#ifdef __EMSCRIPTEN__
        // Returning main is safe: the browser loop owns the heap application.
        emscripten_set_main_loop_arg(browserFrame,app.release(),0,false);
#else
        if(app->options.testApi)app->serveTests();
        else while(app->running())app->tick();
        app->finish();
#endif
        return 0;
    }catch(const std::exception& error){
        std::cerr<<"Snake Survivors: "<<error.what()<<'\n';
#ifdef __EMSCRIPTEN__
        snake::web::showError(error.what());
#endif
        return 1;
    }
}
