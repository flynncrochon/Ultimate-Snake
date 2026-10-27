#include "core.hpp"
#include "snake_geometry.hpp"

#include <algorithm>
#include <cmath>
#include <functional>
#include <iostream>
#include <stdexcept>

namespace {
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
void isolate(snake::Game& game){
    game.owned.clear();game.enemies.clear();game.pickups.clear();game.spawnClock=-1000;game.bossClock=1000;
    game.player.moveClock=-1000;game.player.invulnerable=1000;
}
snake::Enemy corpse(unsigned id,snake::EnemyKind kind=snake::EnemyKind::Red){
    snake::Enemy enemy;enemy.id=id;enemy.alive=false;enemy.kind=kind;enemy.position=enemy.previous={864,800};
    enemy.trailSize=kind==snake::EnemyKind::Boss?8:3;
    for(std::size_t i=0;i<enemy.trail.size();++i)enemy.trail[i]={864-float((i+1)*snake::Tile),800};
    enemy.previousTrail=enemy.trail;return enemy;
}
}

int main(){
    int passed=0,failed=0;
    auto test=[&](const char* name,const std::function<void()>& run){try{run();++passed;std::cout<<"PASS "<<name<<'\n';}catch(const std::exception& e){++failed;std::cerr<<"FAIL "<<name<<": "<<e.what()<<'\n';}};
    const auto catalog=snake::Catalog::load(SNAKE_TEST_DATA_DIR);
    test("all enemy palettes retain their final pose through compaction",[&]{
        snake::Game game(catalog);isolate(game);
        for(int kind=0;kind<4;++kind)game.enemies.push_back(corpse(100+unsigned(kind),snake::EnemyKind(kind)));
        game.update(snake::FixedStep,{});
        require(game.snakeDeaths.size()==4,"Compaction dropped a death pose");
        for(const auto& death:game.snakeDeaths){
            require(death.id==100+unsigned(death.kind),"Compaction associated the wrong palette/handle");
            require(death.current[0].x==864 && death.current[1].x==856,"Final body path was lost");
            require(death.age==0,"New deaths must start at the impact pose");
        }
    });
    test("death storage is bounded and reused through simultaneous waves",[&]{
        snake::Game game(catalog);isolate(game);const auto* allocation=game.snakeDeaths.data();
        for(int wave=0;wave<3;++wave){
            for(unsigned i=0;i<1800;++i)game.enemies.push_back(corpse(10000+unsigned(wave)*1800+i));
            game.update(snake::FixedStep,{});
            require(game.snakeDeaths.size()<=snake::SnakeDeathCapacity,"Death capacity exceeded");
            require(game.snakeDeaths.data()==allocation,"Death wave allocated storage during gameplay");
            require(std::any_of(game.snakeDeaths.begin(),game.snakeDeaths.end(),[&](const auto& d){return d.id==10000+unsigned(wave)*1800+1799;}),"Latest death missing at saturation");
        }
        for(int i=0;i<50;++i)game.update(snake::FixedStep,{});
        require(game.snakeDeaths.empty(),"Completed death poses leaked");
        game.reset();require(game.snakeDeaths.empty() && !game.playerDied,"Reset leaked death state");
    });
    test("cosmetic clocks follow fixed updates and freeze on pause",[&]{
        snake::Game game(catalog);isolate(game);game.enemies.push_back(corpse(100));game.update(snake::FixedStep,{});
        game.update(snake::FixedStep,{});const float age=game.snakeDeaths[0].age;
        require(std::abs(age-snake::FixedStep)<.000001f,"Death clock does not use fixed steps");
        game.togglePause();for(int i=0;i<120;++i)game.update(snake::FixedStep,{});
        require(game.snakeDeaths[0].age==age,"Paused deaths moved");game.togglePause();
        for(int i=0;i<50;++i)game.update(snake::FixedStep,{});
        require(game.snakeDeaths.empty(),"Death never finished after resuming");
    });
    test("consuming weapons retain ownership of devoured victims",[&]{
        snake::Game game(catalog);isolate(game);auto enemy=corpse(100,snake::EnemyKind::Splitter);enemy.devoured=true;game.enemies.push_back(enemy);
        game.update(snake::FixedStep,{});require(game.snakeDeaths.empty() && game.enemies.empty(),"Devoured splitter left a corpse or offspring");
    });
    test("nest and Hydra expiration retain separate body palettes",[&]{
        snake::Game game(catalog);isolate(game);
        for(int evolved=0;evolved<2;++evolved){
            snake::MiniSnake mini;mini.id=unsigned(200+evolved);mini.life=.001f;mini.evolved=evolved!=0;mini.generation=1;
            mini.position=mini.previous={864,800};mini.segments.fill(mini.position);mini.previousSegments=mini.segments;
            game.miniSnakes.push_back(mini);
        }
        game.update(snake::FixedStep,{});
        require(game.miniSnakes.empty() && game.snakeDeaths.size()==2,"Summon expiration lost a death pose");
        require(game.snakeDeaths[0].kind==snake::SnakeDeathKind::Hatchling && game.snakeDeaths[1].kind==snake::SnakeDeathKind::Hydra,"Summon palette lost");
    });
    test("lethal contact records the player transition without extra simulation",[&]{
        snake::Game game(catalog);isolate(game);game.player.hp=0;game.update(snake::FixedStep,{});
        require(game.phase==snake::Phase::Dead && game.playerDied,"Player death animation was not armed");
        const float elapsed=game.elapsed;for(int i=0;i<60;++i)game.update(snake::FixedStep,{});
        require(game.elapsed==elapsed,"Player animation advanced combat");
    });
    test("splitter material belongs to its two immediately active offspring",[&]{
        snake::Game game(catalog);isolate(game);auto parent=corpse(100,snake::EnemyKind::Splitter);parent.trailSize=5;
        game.enemies.push_back(parent);game.update(snake::FixedStep,{});
        require(game.enemies.size()==2 && game.snakeDeaths[0].splitChildren==2,"Split must produce the same two live offspring immediately");
        for(std::size_t i=0;i<2;++i){const auto& child=game.enemies[i];
            require(child.kind==snake::EnemyKind::Swift && child.alive && child.hp>0,"Birth animation delayed combat activation");
            require(child.position.x==(i?872:856) && child.position.y==800,"Birth animation changed the original grid spawn");
            require(child.splitBirth.count==6 && child.splitBirth.age==0,"Offspring lost the parent material");
            require(child.splitBirth.path[0].x==108 && child.splitBirth.path[0].y==100,"Compressed parent path changed its grid pose");
            const auto& birth=child.splitBirth;
            require(birth.side*(birth.normalX?birth.normalX:birth.normalY)==(i?1:-1),"Offspring share the same body bank");
        }
        game.update(snake::FixedStep,{});const auto age=game.enemies[0].splitBirth.age;
        game.togglePause();game.update(.1f,{});require(game.enemies[0].splitBirth.age==age,"Paused split advanced");game.togglePause();
        for(int tick=0;tick<20;++tick)game.update(snake::FixedStep,{});
        require(!game.enemies[0].splitBirth.count && !game.enemies[1].splitBirth.count,"Completed split retained active material");
    });
    test("split geometry ends on the actual offspring and stays connected",[&]{
        snake::Game game(catalog);isolate(game);auto parent=corpse(100,snake::EnemyKind::Splitter);
        parent.trailSize=5;parent.direction={1,0};parent.trail={{{856,800},{848,800},{848,808},{848,816},{840,816},{840,816},{840,816},{840,816}}};parent.previousTrail=parent.trail;
        game.enemies.push_back(parent);game.update(snake::FixedStep,{});const auto& child=game.enemies[0];
        snake::geometry::EnemySnakeGeometry shape;shape.count=1;shape.centers[0]={2140,2000};shape.rectangleCount=1;shape.rectangles[0]={2135,1995,11,11};
        const auto final=snake::geometry::buildSplitBirth(child.splitBirth,shape,snake::SplitBirthDuration);
        for(int y=1980;y<2020;++y)for(int x=2110;x<2210;++x)
            require(snake::geometry::contains(final,x,y)==snake::geometry::contains(shape.rectangles[0],x,y),"Birth handoff changed a final pixel");
        for(int tick=0;tick<=60;++tick){
            const auto morph=snake::geometry::buildSplitBirth(child.splitBirth,shape,float(tick)*snake::SplitBirthDuration/60);
            require(morph.valid && morph.count>0,"Split produced invalid geometry");
            for(std::size_t i=0;i<morph.rectangleCount;++i){const auto r=morph.rectangles[i];require(r.width>0 && r.height>0,"Split produced an empty body span");if(i){const auto p=morph.rectangles[i-1];require(r.x<=p.x+p.width && p.x<=r.x+r.width && r.y<=p.y+p.height && p.y<=r.y+r.height,"Split disconnected a body at a turn");}}
        }
    });
    test("split animation honors the existing enemy-cap spawn limit",[&]{
        snake::Game game(catalog);isolate(game);
        for(unsigned i=0;i<1799;++i){auto enemy=corpse(10000+i);enemy.alive=true;enemy.hp=1e9;enemy.speed=0;game.enemies.push_back(enemy);}
        game.enemies.push_back(corpse(100,snake::EnemyKind::Splitter));game.update(snake::FixedStep,{});
        require(game.enemies.size()==1800 && game.snakeDeaths.back().splitChildren==1,"Animation changed spawn behavior at the enemy cap");
        require(std::count_if(game.enemies.begin(),game.enemies.end(),[](const auto& enemy){return enemy.splitBirth.count>0;})==1,"Animation manufactured a second offspring at capacity");
    });
    test("split offspring peel to different Y elevations in every heading",[&]{
        for(const snake::Vec direction:{snake::Vec{1,0},snake::Vec{-1,0},snake::Vec{0,1},snake::Vec{0,-1}}){
            snake::Game game(catalog);isolate(game);auto parent=corpse(100,snake::EnemyKind::Splitter);parent.direction=direction;
            game.enemies.push_back(parent);game.update(snake::FixedStep,{});
            int heights[2]{};
            for(std::size_t side=0;side<2;++side){
                snake::geometry::EnemySnakeGeometry target;target.count=1;target.centers[0]={side?2180:2140,2000};
                target.rectangleCount=1;target.rectangles[0]={target.centers[0].x-5,1995,11,11};
                const auto middle=snake::geometry::buildSplitBirth(game.enemies[side].splitBirth,target,(snake::SplitBirthFlash+snake::SplitBirthDuration)*.5f);
                heights[side]=middle.centers[0].y;
                const auto landed=snake::geometry::buildSplitBirth(game.enemies[side].splitBirth,target,snake::SplitBirthDuration);
                require(landed.centers[0].x==target.centers[0].x && landed.centers[0].y==2000,"Elevated offspring did not settle exactly");
            }
            require(heights[0]<=1980 && heights[1]>=2020,"Split stayed on the same Y plane");
        }
    });
    std::cout<<passed<<" passed, "<<failed<<" failed\n";return failed?1:0;
}
