#include "core.hpp"
#include "grid.hpp"

#include <algorithm>
#include <cmath>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {
void require(bool value,const std::string& message) {if(!value)throw std::runtime_error(message);}
bool near(float a,float b) {return std::abs(a-b)<.001f;}
bool onGrid(snake::Vec p) {return near(std::remainder(p.x,float(snake::Tile)),0) && near(std::remainder(p.y,float(snake::Tile)),0) && snake::inGridBounds(p);}
void step(snake::Game& game,int frames) {for(int i=0;i<frames;++i)game.update(snake::FixedStep,{});}
void prepare(snake::Game& game,const snake::Catalog& catalog,bool nest=true,bool evolved=false,int rank=1) {
    game.owned.clear();game.enemies.clear();game.pickups.clear();game.projectiles.clear();game.zones.clear();game.nestEggs.clear();game.miniSnakes.clear();
    game.player.moveClock=-1000;game.player.invulnerable=1000;game.spawnClock=-1000;game.bossClock=100;
    if(nest)game.owned.push_back({catalog.find("snake_nest"),rank,evolved,0});
    snake::Enemy target;target.id=10000;target.position={game.player.position.x+64,game.player.position.y};
    target.previous=target.position;target.trail.fill(target.position);target.previousTrail=target.trail;
    target.hp=target.maxHp=100000;target.speed=0;game.enemies.push_back(target);
}
snake::MiniSnake miniAt(snake::Vec position) {
    snake::MiniSnake mini;mini.position=mini.previous=position;
    mini.segments={{position,{position.x-snake::Tile,position.y},{position.x-snake::Tile*2,position.y}}};
    mini.previousSegments=mini.segments;mini.life=100;mini.damage=17;return mini;
}
}

int main() {
    int passed=0,failed=0;
    auto test=[&](const char* name,const std::function<void()>& body){try{body();++passed;std::cout<<"PASS "<<name<<'\n';}catch(const std::exception& error){++failed;std::cerr<<"FAIL "<<name<<": "<<error.what()<<'\n';}};
    snake::Catalog catalog;
    try{catalog=snake::Catalog::load(SNAKE_TEST_DATA_DIR);}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}

    test("one base egg lands before hatching a persistent three-snake brood",[&]{
        snake::Game game(catalog,901);prepare(game,catalog);step(game,1);
        require(game.nestEggs.size()==1,"Base Snake Nest must lob one egg, not one projectile for each baby snake.");
        require(game.projectiles.empty(),"Eggs must not be ordinary homing/damaging projectiles.");
        game.owned.clear();const float hp=game.enemies.front().hp;
        step(game,55);
        require(game.miniSnakes.empty(),"Brood appeared before flight and hatching completed.");
        require(near(game.enemies.front().hp,hp),"The airborne or grounded egg dealt contact damage.");
        step(game,20);
        require(game.nestEggs.empty() && game.miniSnakes.size()==3,"A landed base egg did not hatch exactly three snakes.");
        require(near(game.enemies.front().hp,hp),"Brood bit during the hatch frame.");
        step(game,5);
        require(game.enemies.front().hp<hp,"Hatched snakes failed to hunt and bite nearby enemies.");
        require(game.miniSnakes.size()==3,"Snakes disappeared on impact like disposable projectiles.");
    });
    test("egg targets are ground snapshots and prefer a nearby enemy cluster",[&]{
        snake::Game game(catalog,902);prepare(game,catalog);
        snake::Enemy clustered=game.enemies.front();clustered.id=10001;clustered.position.x+=8;clustered.trail.fill(clustered.position);game.enemies.push_back(clustered);
        snake::Enemy lone=clustered;lone.id=10002;lone.position={game.player.position.x-24,game.player.position.y};lone.trail.fill(lone.position);game.enemies.push_back(lone);
        step(game,1);require(game.nestEggs.size()==1,"Missing cast.");
        const auto landing=game.nestEggs.front().target;
        require(landing.x>game.player.position.x,"Nest chose a lone enemy instead of the denser cluster.");
        game.owned.clear();for(auto& enemy:game.enemies)enemy.position.y+=32;
        step(game,20);
        require(snake::sameCell(landing,game.nestEggs.front().target),"Egg homed after launch rather than keeping its landing cell.");
        require(onGrid(landing),"Egg landing did not snap to the world grid.");
    });
    test("mini snakes move one cardinal grid cell and retain real body history",[&]{
        snake::Game game(catalog,903);prepare(game,catalog,false);
        const auto original=miniAt(game.player.position);game.miniSnakes.push_back(original);step(game,5);
        require(game.miniSnakes.size()==1,"Mini snake vanished.");const auto& mini=game.miniSnakes.front();
        require(near(std::abs(mini.position.x-original.position.x)+std::abs(mini.position.y-original.position.y),float(snake::Tile)),"Mini snake did not advance exactly one cell.");
        require(mini.position.x==original.position.x || mini.position.y==original.position.y,"Mini snake moved diagonally.");
        require(snake::sameCell(mini.segments[1],original.segments[0]) && snake::sameCell(mini.segments[2],original.segments[1]),"Mini body did not follow prior grid cells.");
        for(const auto segment:mini.segments)require(onGrid(segment),"Mini body left the grid.");
        require(near(game.enemies.front().hp,100000),"Mini snake damaged a distant enemy.");
    });
    test("mini snake contact bites once per movement tick without despawning",[&]{
        snake::Game game(catalog,904);prepare(game,catalog,false);
        game.enemies.front().position.x=game.player.position.x+8;game.enemies.front().trail.fill(game.enemies.front().position);
        game.miniSnakes.push_back(miniAt(game.player.position));step(game,5);
        const float hp=game.enemies.front().hp;
        require(near(hp,100000-17),"Contact bite did not apply one configured damage amount.");
        step(game,1);require(near(game.enemies.front().hp,hp),"Damage repeated on every render/fixed frame rather than the next grid tick.");
        require(game.miniSnakes.size()==1,"Contact consumed a hunting snake.");
    });
    test("duration and evolution bonuses are snapshotted when the nest is cast",[&]{
        auto content=catalog;const int definition=content.find("snake_nest");
        snake::Game game(content,905);prepare(game,content,true,true);
        game.owned.push_back({content.find("chronofield"),2,false,0});
        // Retain the source lifetime and indirect-damage exclusion while using
        // the explicitly authored native evolution damage multiplier.
        const float expectedLife=5.6f*game.stats().duration;
        const auto evolution=std::find_if(content.evolutions.begin(),content.evolutions.end(),[](const snake::EvolutionDef& recipe){return recipe.weapon=="snake_nest";});
        require(evolution!=content.evolutions.end(),"Missing Hydra recipe.");
        const float expectedDamage=std::round(200*evolution->damage);
        step(game,1);require(game.nestEggs.size()==1,"Hydra should still cast a nest egg.");
        require(game.nestEggs.front().evolved && game.nestEggs.front().count==6,"Hydra did not double the initial brood.");
        require(near(game.nestEggs.front().snakeLife,expectedLife) && near(game.nestEggs.front().damage,expectedDamage),"Cast ignored duration/damage bonuses.");
        game.owned.clear();step(game,75);
        require(game.miniSnakes.size()==6,"Evolved egg hatched the wrong brood after equipment changed.");
        for(const auto& mini:game.miniSnakes)require(mini.evolved && near(mini.damage,expectedDamage) && mini.life>expectedLife-.1f,"Hatched brood lost its cast-time stats.");
    });
    test("Coiled Volley adds eggs instead of replacing the brood count",[&]{
        snake::Game game(catalog,906);prepare(game,catalog);game.owned.push_back({catalog.find("coiled_volley"),2,false,0});step(game,1);
        require(game.nestEggs.size()==3,"Two extra projectiles must produce three eggs.");
        for(const auto& egg:game.nestEggs)require(egg.count==3,"Volley incorrectly modified the number of snakes per egg.");
        game.owned.clear();step(game,90);require(game.miniSnakes.size()==9,"Three eggs did not hatch three broods exactly once.");
    });
    test("nest and brood storage stays bounded without evicting active summons",[&]{
        snake::Game game(catalog,907);prepare(game,catalog);
        game.nestEggs.resize(25);step(game,1);require(game.nestEggs.size()==25,"Casts exceeded or replaced the full nest pool.");
        game.nestEggs.clear();game.owned.clear();
        for(int i=0;i<30;++i)game.miniSnakes.push_back(miniAt({game.player.position.x-64,game.player.position.y}));
        snake::NestEgg egg;egg.target=game.player.position;egg.age=1.25f;egg.count=12;game.nestEggs.push_back(egg);
        step(game,1);require(game.miniSnakes.size()==30 && game.nestEggs.empty(),"A full base brood pool evicted snakes or repeated hatching.");
        game.miniSnakes.resize(50,miniAt(game.player.position));egg.evolved=true;egg.age=1.25f;game.nestEggs.push_back(egg);step(game,1);
        require(game.miniSnakes.size()==60,"Hydra brood pool was not capped at sixty snakes.");
    });
    test("pause freezes nest lifecycle and reset removes all remaining summons",[&]{
        snake::Game game(catalog,908);prepare(game,catalog);step(game,1);game.owned.clear();
        const float age=game.nestEggs.front().age;game.togglePause();step(game,100);
        require(near(game.nestEggs.front().age,age) && game.miniSnakes.empty(),"Nest timers advanced while paused.");
        game.togglePause();step(game,75);require(!game.miniSnakes.empty(),"Resumed nest failed to hatch.");
        const float life=game.miniSnakes.front().life;game.togglePause();step(game,30);require(near(game.miniSnakes.front().life,life),"Summon lifetime advanced while paused.");
        game.reset();require(game.nestEggs.empty() && game.miniSnakes.empty(),"A new run retained old nests or summons.");
    });
    test("expired summons disappear and cannot deal a final post-expiry bite",[&]{
        snake::Game game(catalog,909);prepare(game,catalog,false);game.enemies.front().position=game.player.position;game.enemies.front().trail.fill(game.player.position);
        auto mini=miniAt(game.player.position);mini.life=.001f;mini.moveClock=.08f;game.miniSnakes.push_back(mini);step(game,1);
        require(game.miniSnakes.empty() && near(game.enemies.front().hp,100000),"An expired snake remained active or damaged an enemy.");
    });
    std::cout<<passed<<" passed, "<<failed<<" failed.\n";return failed?1:0;
}
