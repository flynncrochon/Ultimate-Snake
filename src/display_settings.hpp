#pragma once
#include "settings.hpp"
#include <memory>
#include <string>
#include <vector>

namespace snake {
struct MonitorInfo {int index=0;std::string name;int width=0,height=0,refreshRate=60;};
struct DisplayMode {int width=0,height=0,refreshRate=0;};

// Construct after InitWindow. Disabled mode supports deterministic hidden hosts
// without changing their monitor, window, VSync, or frame limiter.
class DisplayManager {
public:
    explicit DisplayManager(bool enabled=true);
    ~DisplayManager();
    DisplayManager(const DisplayManager&)=delete;
    DisplayManager& operator=(const DisplayManager&)=delete;
    bool initialize(const DisplaySettings&,std::string* error=nullptr);
    bool apply(const DisplaySettings&,bool preview=true,std::string* error=nullptr);
    bool confirm();
    bool revert();
    // True when a timed-out preview rolled back. Does not write files.
    bool update(float seconds,bool focused=true,bool minimized=false);
    void updateFrameLimit(bool focused=true,bool minimized=false);
    void observeWindowResize();
    bool confirmationPending() const;
    float confirmationSeconds() const;
    const DisplaySettings& current() const;
    const std::vector<MonitorInfo>& monitors() const;
    std::vector<DisplayMode> videoModes(int monitor) const;
    std::vector<DisplayMode> windowSizes(int monitor) const;
    std::vector<int> refreshRates(int monitor,int width=0,int height=0) const;
private:
    struct Impl;
    std::unique_ptr<Impl> impl;
};
}
