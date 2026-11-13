#include "Capture.hpp"
#include <cassert>
#include <unordered_map>

struct FakeMemory {
    std::unordered_map<std::uint64_t,std::uint8_t> data;
    void put(std::uint64_t address,const void* source,std::size_t size) {
        auto p=static_cast<const std::uint8_t*>(source);
        for (std::size_t i=0;i<size;++i) data[address+i]=p[i];
    }
    template<class T> void put(std::uint64_t address,const T& value) { put(address,&value,sizeof(value)); }
    bool read(std::uint64_t address,void* destination,std::size_t size) {
        auto out=static_cast<std::uint8_t*>(destination);
        for (std::size_t i=0;i<size;++i) {
            auto it=data.find(address+i);
            if (it==data.end()) return false;
            out[i]=it->second;
        }
        return true;
    }
};
constexpr std::uint64_t base=0x100000000ULL,slot=0x140000000ULL,world=0x150000000ULL;
constexpr std::uint64_t instance=0x160000000ULL,player=0x170000000ULL;
constexpr std::uint64_t controller=0x180000000ULL,camera=0x190000000ULL;
constexpr std::uint64_t local=0x1a0000000ULL,level=0x1b0000000ULL,actors=0x1c0000000ULL;
static FakeMemory fixture() {
    using namespace nyx;
    FakeMemory m;
    std::array<std::uint32_t,8> header{0xfeedfacf,0,0,0,1,24,0,0};
    std::array<std::uint8_t,24> command{};
    command[0]=0x1b;command[4]=24;
    std::copy(CaptureLayout::uuid.begin(),CaptureLayout::uuid.end(),command.begin()+8);
    m.put(base,header);m.put(base+32,command);
    const auto encrypted=base+WorldLayout::encodedWorldRva;
    std::array<std::uint32_t,8> offsets{0,7,15,23,31,39,47,55};
    m.put(encrypted+0x80,offsets);
    for (unsigned i=0;i<8;++i) { auto byte=std::uint8_t(slot>>(i*8));m.put(encrypted+offsets[i],byte); }
    m.put(slot,world);m.put(world+0x38,instance);m.put(instance+0x78,player);
    m.put(player+0x30,controller);m.put(controller+0x548,camera);
    m.put(controller+0x528,local);m.put(world+0x30,level);
    std::array<float,3> pos{1,2,3},rot{4,5,6};float fov=90;
    m.put(camera+0x530,pos);m.put(camera+0x548,rot);m.put(camera+0x554,fov);
    std::array<std::uint32_t,2> sizes{2,2};
    m.put(level+0xa0,actors);m.put(level+0xa8,sizes);
    std::array<std::uint64_t,2> list{local,0x1d0000000ULL};m.put(actors,list);
    const std::int32_t team=7;m.put(local+0x9a8,team);
    return m;
}
int main() {
    using namespace nyx;
    auto m=fixture();auto r=capture(m,base,{1000,500});
    assert(r.status==CaptureStatus::ready && r.world==world && r.localActor==local);
    assert(r.localTeam==7 && r.camera.location.x==1 && r.camera.yaw==5 && r.camera.horizontalFov==90);
    assert(r.actorAddresses.size()==2 && r.actorAddresses[1]==0x1d0000000ULL);
    m=fixture();m.data[base+40]^=1;
    r=capture(m,base,{1000,500});assert(r.status==CaptureStatus::unsupportedBuild && r.actorAddresses.empty());
    m=fixture();m.data.erase(camera+0x554);
    r=capture(m,base,{1000,500});assert(r.status==CaptureStatus::unavailable && r.actorAddresses.empty());
    m=fixture();float bad=0;m.put(camera+0x554,bad);
    assert(capture(m,base,{1000,500}).status==CaptureStatus::invalid);
    m=fixture();std::array<std::uint32_t,2> sizes{2,1};m.put(level+0xa8,sizes);
    assert(capture(m,base,{1000,500}).status==CaptureStatus::invalid);
    m=fixture();m.data.erase(local+0x9a8);
    r=capture(m,base,{1000,500});assert(r.status==CaptureStatus::ready && r.localTeam==0);
}
