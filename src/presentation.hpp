#pragma once

#include "core.hpp"
#include "audio_design.hpp"
#include "chest_lottery.hpp"
#include "render_motion.hpp"
#include "snake_geometry.hpp"
#include "settings.hpp"
#include "display_settings.hpp"
#include "pixel_art.hpp"
#include "world_raster.hpp"
#include "effect_art.hpp"
#include "death_art.hpp"
#include "flood_art.hpp"
#include "profile.hpp"
#include <raylib.h>
#include <string>
#include <functional>

namespace snake {
// Prepare bounded circle spans before entering any render/update path.
void prepareWorldEffects();
constexpr int CanvasWidth = 800;
constexpr int CanvasHeight = 500;
enum class Screen { Menu, Countdown, Game };

class Presentation {
public:
    explicit Presentation(const Catalog& catalog, const Settings& initialSettings = defaultSettings(),
        const std::string& profilePath = {},bool persistentProfile = false);
    ~Presentation();
    void update(float dt, Vec mouse, bool mousePressed);
    // A nonnegative preview time samples the chest animation without advancing it.
    void draw(float chestPreviewTime = -1,float deathPreviewTime = -1);
    void drawWorldOnly(float deathPreviewTime = -1);
    void queueAudioEvents();
    void updateAudio(float frameDt);
    void resetAudio();
    void toggleSound();
    bool simulationBlocked() const;
    const ChestReward* pendingChestReward() const;
    bool acknowledgeChestReward();
    float chestRevealSeconds() const;
    void beginSimulationStep();
    void setRenderFrame(float alpha, bool interpolate = true);
    // Desktop/capture defaults remain 800x500. The browser can reveal more
    // world at the same integer pixel zoom without reallocating render assets.
    void setViewport(int width,int height,int pixelScale=1);
    int viewportWidth() const { return viewWidth; }
    int viewportHeight() const { return viewHeight; }
    void startRun(std::uint32_t seed = 0);
    void finishRun();
    void captureScene(const std::string& scene);
    void openPermanentUpgrades();
    void closePermanentUpgrades();
    bool purchaseUpgrade(PermanentUpgrade upgrade);
    bool refundUpgrades();
    void retryProfileSave();
    const Profile& currentProfile() const { return profile; }
    ProfileSaveState profileSaveState() const {
        const auto state=profileStorage.state();
        return state==ProfileSaveState::Saved && profileSavePending?ProfileSaveState::Saving:state;
    }
    bool permanentUpgradesOpen() const { return permanentOpen; }
    void openSettings();
    void closeSettings();
    void updateSettings(float dt);
    void drawSettings();
    int settingsTabIndex() const { return settingsTab; }
    std::string controlName(Action action) const;
    Game game;
    Settings settings, settingsDraft;
    bool settingsOpen = false;
    std::function<bool(const Settings&,std::string&)> onApplySettings;
    std::function<void()> onConfirmSettings, onRevertSettings;
    std::function<bool()> displayConfirmationPending;
    std::function<float()> displayConfirmationSeconds;
    DisplayManager* displayOptions = nullptr;
    void setSettingsFeedback(const std::string& feedback) { settingsMessage=feedback; }
    Screen screen = Screen::Menu;
    int selectedCharacter = 0;
    bool codex = false, quit = false, sound = true, debug = false;
private:
    pixel::Art art;
    render::EffectArt effectArt;
    render::WorldRaster worldRaster;
    render::DeathArt deathArt;
    render::FloodArt floodArt;
    ProfileStorage profileStorage;
    Profile profile;
    bool permanentOpen=false,profileChanged=false,profileSavePending=false,runStarted=false;
    int permanentFocus=0;
    std::int64_t bankedGold=0;
    std::string permanentMessage;
    Vec mouse;
    bool clicked = false, recorded = false;
    int focus = 0, hoveredOffer = 0;
    float visualTime = 0;
    float deathSeconds = 0;
    float countdown = 0;
    std::uint64_t acknowledgedChest = 0, revealingChest = 0;
    float rewardTime = 0;
    loot::ReelTicker reelTicker;
    bool reelTickPending = false;
    render::MotionHistory motionHistory;
    geometry::EnemyGeometryCache enemyGeometry;
    float renderFraction = 1;
    bool interpolateMotion = false;
    int viewWidth=CanvasWidth,viewHeight=CanvasHeight,pixelScale=1;
    int settingsTab = 0, settingsFocus = 0, rebindingAction = -1;
    bool settingsWasConfirming = false;
    std::string settingsMessage;
    bool audioReady = false;
    audio::Director audioDirector;
    std::array<std::array<Sound, audio::VariationCount>, audio::EffectCount> sounds{};
    std::array<Sound, audio::VoiceCount> audioVoices{};
    Sound reelTickSound{};
    void saveProfile();
    void updatePermanentUpgrades(bool mouseMoved);
    void drawPermanentUpgrades();
    void world(bool backdrop = false,float deathPreviewTime = -1);
    void hud();
    void menu();
    void levelUp();
    void pause();
    void result();
    void evolutionCodex();
    void chestReward(float seconds);
    void centered(const std::string& value, int x, int y, Color color, int scale = 1);
    int wrappedCentered(const std::string& value, int x, int y, int width, Color color, int maxLines = 20, int lineHeight = 10);
    bool button(const std::string& label, int x, int y, int w, int h, bool selected = false);
    void panel(int x, int y, int w, int h, Color border);
    void outline(int x,int y,int width,int height,Color color);
    void label(const std::string& value, int x, int y, Color color, int scale = 1);
    void wrapped(const std::string& value, int x, int y, int width, Color color);
    void bar(int x, int y, int w, int h, float ratio, Color color);
    int iconFor(int definition) const;
};
}
