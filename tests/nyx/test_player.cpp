#include "PlayerReader.hpp"
#include <cassert>
#include <cstring>
#include <unordered_map>

struct PlayerMemory {
    std::unordered_map<std::uint64_t,std::uint8_t> data;
    std::size_t reads{};
    void put(std::uint64_t address,const void* source,std::size_t size) {
        auto p=static_cast<const std::uint8_t*>(source);
        for (std::size_t i=0;i<size;++i) data[address+i]=p[i];
    }
    template<class T> void put(std::uint64_t address,const T& value) { put(address,&value,sizeof(value)); }
    bool read(std::uint64_t address,void* dest,std::size_t size) {
        ++reads;
        auto p=static_cast<std::uint8_t*>(dest);
        for (std::size_t i=0;i<size;++i) {
            auto it=data.find(address+i);if (it==data.end()) return false;p[i]=it->second;
        }
        return true;
    }
};
static void name(PlayerMemory& m,std::uint64_t chunk,unsigned id,std::uint64_t entry,const char* text) {
    m.put(chunk+id*8,entry);const std::uint32_t flags=0;m.put(entry+8,flags);
    m.put(entry+12,text,std::strlen(text)+1);
}
int main() {
    using namespace nyx;
    constexpr std::uint64_t base=0x100000000ULL,slot=0x140000000ULL,world=0x150000000ULL;
    constexpr std::uint64_t n1=0x160000000ULL,n2=0x170000000ULL,pool=0x180000000ULL,chunk=0x190000000ULL;
    constexpr std::uint64_t local=0x1a0000000ULL,enemy=0x1b0000000ULL,mesh=0x1c0000000ULL;
    constexpr std::uint64_t asset=0x1d0000000ULL,refs=0x1e0000000ULL,bones=0x1f0000000ULL;
    PlayerMemory m;
    const auto global=base+PlayerLayout::namesGlobal;
    const std::uint32_t tag=106;const std::uint8_t initialized=1;
    m.put(global,tag);m.put(global+4,initialized);m.put(global+8,n1);
    m.put(n1,n2);m.put(n2,pool);m.put(pool,chunk);
    name(m,chunk,1,0x200000000ULL,"PlayerPawn_C");
    name(m,chunk,2,0x210000000ULL,"head");
    name(m,chunk,3,0x220000000ULL,"neck_01");
    const std::uint32_t actorName=1,head=2,neck=3;
    const std::int32_t team=2,count=2;
    m.put(enemy+0x18,actorName);m.put(enemy+0x9a8,team);m.put(enemy+0x510,mesh);
    m.put(mesh+0xa20,asset);m.put(asset+0x198,refs);m.put(asset+0x1a0,count);
    m.put(refs,head);m.put(refs+16,neck);
    m.put(mesh+0xce0,bones);m.put(mesh+0xce8,count);m.put(mesh+0xcec,count);
    Transform component;component.w=1;component.sx=component.sy=component.sz=1;component.tx=100;
    m.put(mesh+0x210,component);
    Transform b1;b1.tz=20;Transform b2;b2.tz=10;m.put(bones,b1);m.put(bones+48,b2);
    const auto encoded=base+WorldLayout::encodedWorldRva;
    std::array<std::uint32_t,8> offsets{0,7,15,23,31,39,47,55};m.put(encoded+0x80,offsets);
    for (unsigned i=0;i<8;++i) { const auto byte=std::uint8_t(slot>>(8*i));m.put(encoded+offsets[i],byte); }
    m.put(slot,world);
    assert(readNamesPool(m,base)==pool);
    assert(readName(m,pool,1)=="PlayerPawn_C");
    assert(playerName(readName(m,pool,1)) && !playerName("PlayerController"));
    assert(jointSlot("B_HEAD")==0 && jointSlot("neck_01")==1 && jointSlot("unknown")==-1);
    Transform rotated;rotated.w=0.70710678f;rotated.z=0.70710678f;
    rotated.sx=rotated.sy=rotated.sz=1;Transform bone;bone.tx=10;
    auto p=boneWorld(bone,rotated);assert(p && std::abs(p->x)<0.001 && std::abs(p->y-10)<0.001);
    RawCapture raw;raw.status=CaptureStatus::ready;raw.world=world;raw.localActor=local;
    raw.actorAddresses={local,enemy};
    auto snapshot=buildPlayerSnapshot(m,base,raw,{1000,500},1000);
    assert(snapshot.complete && snapshot.actors.size()==1 && snapshot.actors[0].skeleton.size()==1);
    auto frame=buildFrame(snapshot,{1000,500},1100);
    assert(frame.status==FrameStatus::ready && frame.lines.size()==1 && frame.lines[0].actor==enemy);
    PlayerReaderCache cache;const auto before=m.reads;
    auto first=buildPlayerSnapshot(m,base,raw,{1000,500},1000,&cache);
    const auto firstCost=m.reads-before;
    const auto mid=m.reads;
    auto second=buildPlayerSnapshot(m,base,raw,{1000,500},1001,&cache);
    const auto secondCost=m.reads-mid;
    assert(first.complete && second.complete && second.actors.size()==1 && secondCost<firstCost);
    m.data.erase(bones+48);
    snapshot=buildPlayerSnapshot(m,base,raw,{1000,500},1000);
    assert(snapshot.complete && snapshot.actors.empty());
    m.data[global+4]=0;
    snapshot=buildPlayerSnapshot(m,base,raw,{1000,500},1000);
    assert(!snapshot.complete && snapshot.actors.empty());
}
