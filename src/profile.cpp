#include "profile.hpp"

#include <algorithm>
#include <atomic>
#include <filesystem>
#include <fstream>
#include <limits>
#include <utility>
#ifdef __EMSCRIPTEN__
#include "web_platform.hpp"
#include <emscripten/eventloop.h>
#else
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <thread>
#endif
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif

namespace snake {
namespace {
bool valid(const Profile& profile){
    if(profile.bestKills<0 || profile.runs<0 || profile.bestSeconds<0 || profile.victories<0 ||
       profile.gold<0 || profile.investedGold<0 || profile.gold>MaxProfileGold-profile.investedGold)return false;
    for(std::size_t i=0;i<PermanentUpgradeCount;++i)
        if(profile.upgrades.ranks[i]<0 || profile.upgrades.ranks[i]>permanentDefinitions()[i].maxRank)return false;
    return (profile.upgrades.totalRanks()==0)==(profile.investedGold==0);
}
LoadedProfile loadFile(const std::filesystem::path& path){
    std::error_code error;
    if(!std::filesystem::exists(path,error))return {{},error?ProfileLoadState::Invalid:ProfileLoadState::Missing};
    const auto size=std::filesystem::file_size(path,error);
    if(error || size>4096)return {{},ProfileLoadState::Invalid};
    std::ifstream file(path,std::ios::binary);if(!file)return {{},ProfileLoadState::Invalid};
    return readProfile(file);
}
bool commit(const std::filesystem::path& path,const std::filesystem::path& temporary,const Profile& profile) noexcept {
    try{
        std::ofstream file(temporary,std::ios::binary|std::ios::trunc);
        if(!file)return false;writeProfile(file,profile);file.flush();
        if(!file)return false;file.close();if(!file)return false;
#ifdef _WIN32
        return MoveFileExW(temporary.c_str(),path.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)!=0;
#else
        std::error_code error;std::filesystem::rename(temporary,path,error);return !error;
#endif
    }catch(...){return false;}
}
}
LoadedProfile readProfile(std::istream& input){
    Profile profile;std::string version;
    if(!(input>>version))return {{},ProfileLoadState::Invalid};
    if(version!="SS1" && version!="SS2")return {{},ProfileLoadState::Unsupported};
    if(!(input>>profile.bestKills>>profile.runs>>profile.bestSeconds>>profile.victories>>profile.sound))return {{},ProfileLoadState::Invalid};
    if(version=="SS2"){
        if(!(input>>profile.gold>>profile.investedGold))return {{},ProfileLoadState::Invalid};
        for(int& rank:profile.upgrades.ranks)if(!(input>>rank))return {{},ProfileLoadState::Invalid};
    }
    input>>std::ws;
    if(!input.eof() || !valid(profile))return {{},ProfileLoadState::Invalid};
    return {profile,version=="SS1"?ProfileLoadState::Migrated:ProfileLoadState::Loaded};
}
void writeProfile(std::ostream& output,const Profile& profile){
    if(!valid(profile)){output.setstate(std::ios::failbit);return;}
    output<<"SS2 "<<profile.bestKills<<' '<<profile.runs<<' '<<profile.bestSeconds<<' '
        <<profile.victories<<' '<<profile.sound<<' '<<profile.gold<<' '<<profile.investedGold;
    for(int rank:profile.upgrades.ranks)output<<' '<<rank;
    output<<'\n';
}
bool purchasePermanent(Profile& profile,PermanentUpgrade upgrade){
    if(!valid(profile))return false;
    const int price=permanentPrice(profile.upgrades,upgrade);
    if(price<=0 || profile.gold<price)return false;
    profile.gold-=price;profile.investedGold+=price;++profile.upgrades.ranks[std::size_t(upgrade)];return true;
}
std::int64_t refundPermanent(Profile& profile){
    const auto refunded=profile.investedGold;profile.gold+=refunded;profile.investedGold=0;profile.upgrades={};return refunded;
}
std::int64_t bankGold(Profile& profile,int earnedGold){
    const auto earned=std::min<std::int64_t>(std::max(0,earnedGold),MaxProfileGold-profile.gold-profile.investedGold);
    profile.gold+=earned;return earned;
}

struct ProfileStorage::Impl {
    struct Snapshot {Profile profile;std::uint64_t revision=0;};
    std::filesystem::path path,temporary;
    LoadedProfile initial;
    bool persistent=false,blocked=false,closed=false;
    std::uint64_t revision=0;
    std::atomic<std::uint64_t> saved{0};
    std::atomic<bool> failed{false};
#ifdef __EMSCRIPTEN__
    Snapshot pending;
    int timer=0;
    static void webSave(void* pointer){
        auto& self=*static_cast<Impl*>(pointer);self.timer=0;self.save(self.pending);
    }
#else
    static constexpr unsigned Capacity=8;
    std::array<Snapshot,Capacity> mailbox{};
    std::atomic<unsigned> produced{0},consumed{0};
    std::atomic<bool> stopping{false};
    Snapshot final;
    bool hasFinal=false;
    std::mutex waitMutex;
    std::condition_variable wake;
    std::thread worker;
#endif
    Impl(std::string filename,bool enabled):path(std::move(filename)){
        if(path.empty())return;
        temporary=path;temporary+=".tmp";
        initial=loadFile(path);
        blocked=initial.state==ProfileLoadState::Invalid || initial.state==ProfileLoadState::Unsupported;
        persistent=enabled && !blocked;
#ifndef __EMSCRIPTEN__
        if(persistent)worker=std::thread([this]{run();});
#endif
    }
    void save(const Snapshot& snapshot){
        const bool success=commit(path,temporary,snapshot.profile);
        failed.store(!success,std::memory_order_release);
        if(success){saved.store(snapshot.revision,std::memory_order_release);
#ifdef __EMSCRIPTEN__
            web::flushSaves();
#endif
        }
    }
    bool submit(const Profile& profile){
        if(closed || blocked || !valid(profile))return false;
        if(!persistent)return true;
#ifdef __EMSCRIPTEN__
        pending={profile,++revision};
        if(!timer)timer=emscripten_set_timeout(&Impl::webSave,0,this);
#else
        const auto write=produced.load(std::memory_order_relaxed),read=consumed.load(std::memory_order_acquire);
        if(write-read>=Capacity)return false;
        mailbox[write%Capacity]={profile,++revision};
        produced.store(write+1,std::memory_order_release);wake.notify_one();
#endif
        return true;
    }
#ifndef __EMSCRIPTEN__
    void run(){
        for(;;){
            if(stopping.load(std::memory_order_acquire)){
                if(hasFinal)save(final);
                else {
                    const auto write=produced.load(std::memory_order_acquire),read=consumed.load(std::memory_order_relaxed);
                    if(write!=read)save(mailbox[(write-1)%Capacity]);
                }
                return;
            }
            Snapshot latest;bool pending=false;
            auto read=consumed.load(std::memory_order_relaxed);
            for(unsigned count=0;count<Capacity;++count){
                if(read==produced.load(std::memory_order_acquire))break;
                latest=mailbox[read%Capacity];pending=true;
                consumed.store(++read,std::memory_order_release);
            }
            if(pending){save(latest);continue;}
            // Only the worker takes this mutex. The timeout closes the usual
            // lock-free producer/condition-variable lost-notification window.
            std::unique_lock<std::mutex> lock(waitMutex);
            wake.wait_for(lock,std::chrono::milliseconds(50),[this]{return stopping.load(std::memory_order_acquire) || produced.load(std::memory_order_acquire)!=consumed.load(std::memory_order_relaxed);});
        }
    }
#endif
    void close(const Profile* snapshot){
        if(closed)return;closed=true;if(!persistent)return;
#ifdef __EMSCRIPTEN__
        if(timer){emscripten_clear_timeout(timer);timer=0;}
        if(snapshot && valid(*snapshot))save({*snapshot,++revision});
        else if(pending.revision>saved.load(std::memory_order_acquire))save(pending);
#else
        if(snapshot && valid(*snapshot)){final={*snapshot,++revision};hasFinal=true;}
        stopping.store(true,std::memory_order_release);wake.notify_one();
        if(worker.joinable())worker.join();
#endif
    }
};
ProfileStorage::ProfileStorage(std::string path,bool persistent):impl(std::make_unique<Impl>(std::move(path),persistent)){}
ProfileStorage::~ProfileStorage(){close();}
const LoadedProfile& ProfileStorage::loaded()const{return impl->initial;}
bool ProfileStorage::submit(const Profile& profile){return impl->submit(profile);}
ProfileSaveState ProfileStorage::state()const{
    if(impl->blocked)return ProfileSaveState::Blocked;
    if(!impl->persistent)return ProfileSaveState::Temporary;
    if(impl->failed.load(std::memory_order_acquire))return ProfileSaveState::Failed;
    return impl->saved.load(std::memory_order_acquire)<impl->revision?ProfileSaveState::Saving:ProfileSaveState::Saved;
}
void ProfileStorage::close(const Profile* snapshot){impl->close(snapshot);}
}
