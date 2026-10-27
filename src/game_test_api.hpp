#pragma once

#include "core.hpp"
#include "../third_party/nlohmann/json.hpp"
#include <functional>
#include <iosfwd>

namespace snake {
using TestJson = nlohmann::json;
using TestCapture = std::function<TestJson(const TestJson&)>;

// Test hosts drive the same Game object as the real executable. There is no
// second simulation, wall-clock sleep, or Python dependency in this interface.
class GameTestSession {
public:
    explicit GameTestSession(Game& game, TestCapture capture = {});
    TestJson execute(const TestJson& request);
    TestJson dispatch(const TestJson& request) noexcept;
    int serve(std::istream& input, std::ostream& output);
    TestJson snapshot(bool full = true) const;
    bool stopped() const { return stop; }
    void setStepObserver(std::function<void()> observer) { beforeStep=std::move(observer); }
private:
    Game& game;
    TestCapture capture;
    std::function<void()> beforeStep;
    std::uint64_t tick = 0;
    std::uint32_t nextHandle = 1000000000u;
    bool stop = false, director = false, frozenPlayer = true;
    bool invulnerable = true, autoChoose = true;
    std::array<std::uint64_t,std::size_t(Event::Win)+1> eventCounts{};
    TestJson lastTiming = TestJson::object();
    TestJson reset(const TestJson& request);
    TestJson configure(const TestJson& request);
    TestJson setPlayer(const TestJson& request);
    TestJson equip(const TestJson& request);
    TestJson spawnEnemy(const TestJson& request);
    TestJson advance(const TestJson& request);
    TestJson validate() const;
};
}
