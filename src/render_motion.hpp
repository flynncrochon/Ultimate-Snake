#pragma once

#include "core.hpp"
#include <array>
#include <cstddef>
#include <memory>

namespace snake::render {

template<std::size_t N> struct SnakePose {
    std::array<Vec,N> previous{}, current{};
    std::size_t count = 0;
    float alpha = 1;
    Vec direction{1,0};
    // A rigid cosmetic translation, quantized once for the whole snake.
    Vec offset;
};
struct GreyPose {
    Vec previous, current, direction;
    std::size_t count = 0;
    float alpha = 1;
    bool alive = false;
};

// Keep simulation waypoints untouched. Rendering samples one fixed update
// behind, interpolating the movement phase across cell boundaries and turns.
// Storage is allocated once; entity lookup never allocates or scans the swarm.
class MotionHistory {
public:
    MotionHistory();
    ~MotionHistory();
    MotionHistory(const MotionHistory&) = delete;
    MotionHistory& operator=(const MotionHistory&) = delete;
    void clear();
    void beginStep(const Game& game);
    SnakePose<128> samplePlayer(const Player& player,float frameAlpha,bool active=false) const;
    SnakePose<9> sampleEnemy(const Enemy& enemy,float frameAlpha,bool active=false) const;
    SnakePose<9> sampleDeath(const SnakeDeath& death,float frameAlpha,bool active=false) const;
    SnakePose<5> sampleMini(const MiniSnake& mini,float frameAlpha,bool active=false) const;
    GreyPose sampleGrey(const GreySnake& snake,float frameAlpha,bool active=false) const;
private:
    struct Storage;
    std::unique_ptr<Storage> storage;
};

}
