#include "game_test_api.hpp"
#include <filesystem>
#include <iostream>
#include <stdexcept>

int main(int argc,char** argv) {
    try {
        std::string data;
        for(int i=1;i<argc;++i){
            const std::string arg=argv[i];
            if(arg=="--data" && i+1<argc)data=argv[++i];
            else if(arg=="--help"){
                std::cerr<<"snake_game_test [--data DIRECTORY]\nJSONL requests on stdin, responses on stdout.\n";return 0;
            }else throw std::runtime_error("Unknown or incomplete argument: "+arg);
        }
        if(data.empty()){
            const auto beside=std::filesystem::absolute(argv[0]).parent_path()/"data";
            data=std::filesystem::exists(beside/"upgrades.csv")?beside.string():"data";
        }
        const auto catalog=snake::Catalog::load(data);snake::Game game(catalog);
        snake::GameTestSession session(game);return session.serve(std::cin,std::cout);
    }catch(const std::exception& error){std::cerr<<"GameTest host: "<<error.what()<<'\n';return 1;}
}
