#pragma once

#include "permanent_upgrades.hpp"
#include <iosfwd>
#include <memory>
#include <string>

namespace snake {
struct Profile {
    int bestKills=0,runs=0,bestSeconds=0,victories=0;
    bool sound=true;
    std::int64_t gold=0,investedGold=0;
    PermanentUpgrades upgrades;
};
constexpr std::int64_t MaxProfileGold=1000000000000LL;
enum class ProfileLoadState { Missing, Loaded, Migrated, Invalid, Unsupported };
enum class ProfileSaveState { Temporary, Saved, Saving, Failed, Blocked };
struct LoadedProfile { Profile value;ProfileLoadState state=ProfileLoadState::Missing; };
LoadedProfile readProfile(std::istream& input);
void writeProfile(std::ostream& output,const Profile& profile);
bool purchasePermanent(Profile& profile,PermanentUpgrade upgrade);
std::int64_t refundPermanent(Profile& profile);
std::int64_t bankGold(Profile& profile,int earnedGold);

// An empty path is a temporary profile. Tests/captures can load an explicit
// path read-only. Native writes use a bounded SPSC mailbox and worker; frames
// never wait on file I/O or acquire the worker's mutex. close() joins only at
// shutdown and flushes the final complete snapshot before storage is destroyed.
class ProfileStorage {
public:
    explicit ProfileStorage(std::string path={},bool persistent=false);
    ~ProfileStorage();
    ProfileStorage(const ProfileStorage&)=delete;
    ProfileStorage& operator=(const ProfileStorage&)=delete;
    const LoadedProfile& loaded() const;
    bool submit(const Profile& profile);
    ProfileSaveState state() const;
    void close(const Profile* finalSnapshot=nullptr);
private:
    struct Impl;
    std::unique_ptr<Impl> impl;
};
}
