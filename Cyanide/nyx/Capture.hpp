#pragma once
#include "Projection.hpp"
#include "WorldReader.hpp"
#include <array>
#include <cstring>
#include <utility>
#include <vector>

namespace nyx {
// All offsets are from the supplied 4.7 build and need live validation.
struct CaptureLayout {
    static constexpr std::uint64_t gameInstance=0x38,localPlayer=0x78,controller=0x30;
    static constexpr std::uint64_t cameraManager=0x548,localPawn=0x528,level=0x30;
    static constexpr std::uint64_t directActors=0xa0,team=0x9a8;
    static constexpr std::uint64_t location=0x530,rotation=0x548,fov=0x554;
    static constexpr std::size_t maxActors=15000;
    static constexpr std::array<std::uint8_t,16> uuid{
        0x09,0xe7,0xa8,0x76,0xbd,0x40,0x3e,0x4b,
        0xaa,0xea,0xa1,0xfd,0xe4,0x51,0x56,0x9d};
};
enum class CaptureStatus { ready, unavailable, invalid, unsupportedBuild, changed };
struct RawCapture {
    CaptureStatus status{CaptureStatus::unavailable};
    std::uint64_t world{},localActor{};
    int localTeam{};
    Camera camera{};
    std::vector<std::uint64_t> actorAddresses;
};
inline std::uint32_t u32le(const std::uint8_t* b) {
    return std::uint32_t(b[0]) | (std::uint32_t(b[1])<<8) |
           (std::uint32_t(b[2])<<16) | (std::uint32_t(b[3])<<24);
}
template<class Reader> CaptureStatus verifyBuild(Reader& reader,std::uint64_t base) {
    std::array<std::uint8_t,32> header{};
    if (!readablePointer(base)) return CaptureStatus::invalid;
    if (!reader.read(base,header.data(),header.size())) return CaptureStatus::unavailable;
    if (u32le(header.data())!=0xfeedfacfU) return CaptureStatus::unsupportedBuild;
    const auto count=u32le(header.data()+16),size=u32le(header.data()+20);
    if (count==0 || count>1024 || size<8 || size>65536) return CaptureStatus::invalid;
    std::uint64_t address{};
    if (!addAddress(base,32,address)) return CaptureStatus::invalid;
    std::vector<std::uint8_t> commands(size);
    if (!reader.read(address,commands.data(),commands.size())) return CaptureStatus::unavailable;
    std::size_t pos=0;
    bool seen=false;
    for (std::uint32_t i=0;i<count;++i) {
        if (size-pos<8) return CaptureStatus::invalid;
        const auto command=u32le(commands.data()+pos),length=u32le(commands.data()+pos+4);
        if (length<8 || length>size-pos || (length&7U)) return CaptureStatus::invalid;
        if (command==0x1b) {
            if (seen || length!=24) return CaptureStatus::invalid;
            seen=true;
            if (!std::equal(CaptureLayout::uuid.begin(),CaptureLayout::uuid.end(),commands.begin()+pos+8))
                return CaptureStatus::unsupportedBuild;
        }
        pos+=length;
    }
    if (pos!=size) return CaptureStatus::invalid;
    return seen?CaptureStatus::ready:CaptureStatus::unsupportedBuild;
}
template<class Reader,class T> bool readAt(Reader& r,std::uint64_t base,std::uint64_t offset,T& value) {
    std::uint64_t address{};
    return readablePointer(base) && addAddress(base,offset,address) && r.read(address,&value,sizeof(value));
}
template<class Reader> RawCapture capture(Reader& reader,std::uint64_t base,Viewport viewport) {
    RawCapture result;
    result.status=verifyBuild(reader,base);
    if (result.status!=CaptureStatus::ready) return result;
    const auto w=readWorld(reader,base);
    if (w.status!=WorldStatus::ready) {
        result.status=w.status==WorldStatus::changed?CaptureStatus::changed:
            w.status==WorldStatus::invalid?CaptureStatus::invalid:CaptureStatus::unavailable;
        return result;
    }
    std::uint64_t instance{},player{},controller{},camera{},local{},level{};
    if (!readAt(reader,w.world,CaptureLayout::gameInstance,instance)
        || !readAt(reader,instance,CaptureLayout::localPlayer,player)
        || !readAt(reader,player,CaptureLayout::controller,controller)
        || !readAt(reader,controller,CaptureLayout::cameraManager,camera)
        || !readAt(reader,controller,CaptureLayout::localPawn,local)
        || !readAt(reader,w.world,CaptureLayout::level,level)) {
        result.status=CaptureStatus::unavailable; return result;
    }
    if (!readablePointer(local) || !readablePointer(level) || !readablePointer(camera)) {
        result.status=CaptureStatus::invalid; return result;
    }
    std::array<float,3> location{},rotation{};float fov{};
    if (!readAt(reader,camera,CaptureLayout::location,location)
        || !readAt(reader,camera,CaptureLayout::rotation,rotation)
        || !readAt(reader,camera,CaptureLayout::fov,fov)) {
        result.status=CaptureStatus::unavailable; return result;
    }
    Camera c{{location[0],location[1],location[2]},rotation[0],rotation[1],rotation[2],fov};
    if (!Projector::create(c,viewport)) { result.status=CaptureStatus::invalid; return result; }
    struct ArrayHeader { std::uint64_t data; std::int32_t count,capacity; } actors{};
    static_assert(sizeof(ArrayHeader)==16,"Unexpected TArray layout");
    if (!readAt(reader,level,CaptureLayout::directActors,actors)) {
        result.status=CaptureStatus::unavailable; return result;
    }
    if (actors.count<0 || actors.capacity<actors.count || actors.capacity>1000000
        || std::size_t(actors.count)>CaptureLayout::maxActors
        || (actors.count>0 && !readablePointer(actors.data))) {
        result.status=CaptureStatus::invalid; return result;
    }
    std::vector<std::uint64_t> list(static_cast<std::size_t>(actors.count));
    if (!list.empty() && !reader.read(actors.data,list.data(),list.size()*sizeof(list[0]))) {
        result.status=CaptureStatus::unavailable; return result;
    }
    // Unknown local team remains zero; it must not become a guessed team.
    std::int32_t team{};
    if (!readAt(reader,local,CaptureLayout::team,team) || team<1 || team>255) team=0;
    const auto recheck=readWorld(reader,base);
    if (recheck.status!=WorldStatus::ready || recheck.world!=w.world) {
        result.status=CaptureStatus::changed; return result;
    }
    result.status=CaptureStatus::ready;result.world=w.world;result.localActor=local;
    result.localTeam=team;result.camera=c;result.actorAddresses=std::move(list);
    return result;
}
} // namespace nyx
