#pragma once
#include "Capture.hpp"
#include <cctype>
#include <string>
#include <unordered_map>

namespace nyx {
struct PlayerLayout {
    static constexpr std::uint64_t namesGlobal=0xAC9DBC0,actorName=0x18,mesh=0x510;
    static constexpr std::uint64_t asset=0xA20,referenceArray=0x198,referenceCount=0x1A0;
    static constexpr std::uint64_t componentTransform=0x210,boneArray=0xCE0;
};
// Pure reads of already-initialized state. The engine's lazy initializer is
// deliberately not invoked; absence of initialized data is not a valid name.
template<class Reader> std::uint64_t readNamesPool(Reader& r,std::uint64_t base) {
    std::uint64_t object{};
    if (!readablePointer(base) || !addAddress(base,PlayerLayout::namesGlobal,object)) return 0;
    std::uint32_t tag{};std::uint8_t initialized{};std::uint64_t pointer{};
    if (!readAt(r,object,0,tag) || !readAt(r,object,4,initialized)
        || !(initialized&1) || !readAt(r,object,8,pointer)) return 0;
    if (tag<103 || (tag-100)%3!=0) return 0;
    const auto depth=(tag-100)/3;
    if (!depth || depth>16 || !readablePointer(pointer)) return 0;
    for (std::uint32_t i=1;i<depth;++i) {
        std::uint64_t next{};
        if (!readAt(r,pointer,0,next) || !readablePointer(next)) return 0;
        pointer=next;
    }
    std::uint64_t pool{};
    if (!readAt(r,pointer,0,pool) || !readablePointer(pool)) return 0;
    return pool;
}
template<class Reader> std::string readName(Reader& r,std::uint64_t pool,std::uint32_t id) {
    if (!readablePointer(pool) || !id || id>2000000) return {};
    std::uint64_t chunk{},entry{};
    if (!readAt(r,pool,std::uint64_t(id>>14)*8,chunk) || !readablePointer(chunk)
        || !readAt(r,chunk,std::uint64_t(id&0x3fff)*8,entry) || !readablePointer(entry)) return {};
    std::uint32_t flags{};
    if (!readAt(r,entry,8,flags)) return {};
    std::uint64_t textAddress{};
    if (!addAddress(entry,12,textAddress)) return {};
    std::array<std::uint8_t,126> buffer{};
    const std::size_t length=(flags&1)?126:63;
    const bool batched=r.read(textAddress,buffer.data(),length);
    std::string result;
    // Bound remote reads and require explicit termination, including for UTF-16.
    for (unsigned i=0;i<63;++i) {
        std::uint16_t ch{};
        std::uint64_t address{};
        if (!addAddress(entry,12+std::uint64_t(i)*((flags&1)?2:1),address)) return {};
        if (batched) {
            ch=(flags&1)?static_cast<std::uint16_t>(buffer[2*i]|(std::uint16_t(buffer[2*i+1])<<8)):buffer[i];
        } else if (flags&1) { if (!r.read(address,&ch,2)) return {}; }
        else { std::uint8_t c{};if (!r.read(address,&c,1)) return {};ch=c; }
        if (!ch) return result;
        if (ch<32 || ch>126) return {};
        result.push_back(static_cast<char>(ch));
    }
    return {};
}
inline bool playerName(std::string name) {
    for (auto& c:name) c=static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    const auto has=[&](const char* s){ return name.find(s)!=std::string::npos; };
    return (has("player")||has("character")||has("pawn")) &&
        !(has("vehicle")||has("projectile")||has("bullet")||has("decal")||
          has("controller")||has("state")||has("component")||has("dead")||has("box"));
}
inline int jointSlot(std::string n) {
    for (auto& c:n) c=static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    static constexpr const char* aliases[15][2]={
        {"head","b_head"},{"neck_01","neck"},{"pelvis","hips"},
        {"upperarm_l","l_upperarm"},{"lowerarm_l","l_lowerarm"},{"hand_l","l_hand"},
        {"upperarm_r","r_upperarm"},{"lowerarm_r","r_lowerarm"},{"hand_r","r_hand"},
        {"thigh_l","l_thigh"},{"calf_l","l_calf"},{"foot_l","l_foot"},
        {"thigh_r","r_thigh"},{"calf_r","r_calf"},{"foot_r","r_foot"}};
    for (int i=0;i<15;++i) if (n==aliases[i][0] || n==aliases[i][1]) return i;
    return -1;
}
struct Transform {
    float x{},y{},z{},w{},tx{},ty{},tz{},pad{},sx{},sy{},sz{},pad2{};
};
static_assert(sizeof(Transform)==48,"Unexpected Unreal transform layout");
inline std::optional<Vec3> boneWorld(const Transform& bone,const Transform& component) {
    const Vec3 local{bone.tx,bone.ty,bone.tz},origin{component.tx,component.ty,component.tz};
    if (!finite(local)||!finite(origin)) return std::nullopt;
    const double x=component.x,y=component.y,z=component.z,w=component.w;
    const double q=x*x+y*y+z*z+w*w;
    if (!std::isfinite(q) || q<0.5 || q>1.5 || !std::isfinite(component.sx)
        || !std::isfinite(component.sy) || !std::isfinite(component.sz)
        || std::abs(component.sx)>100 || std::abs(component.sy)>100 || std::abs(component.sz)>100)
        return std::nullopt;
    const double a=local.x*component.sx,b=local.y*component.sy,c=local.z*component.sz;
    Vec3 out{origin.x+a*(1-2*(y*y+z*z))+b*2*(x*y-w*z)+c*2*(x*z+w*y),
             origin.y+a*2*(x*y+w*z)+b*(1-2*(x*x+z*z))+c*2*(y*z-w*x),
             origin.z+a*2*(x*z-w*y)+b*2*(y*z+w*x)+c*(1-2*(x*x+y*y))};
    if (!finite(out) || std::hypot(std::hypot(out.x-origin.x,out.y-origin.y),out.z-origin.z)>300)
        return std::nullopt;
    return out;
}
template<class Reader> std::array<int,15> readRig(Reader& r,std::uint64_t mesh,std::uint64_t pool) {
    std::array<int,15> rig;rig.fill(-1);
    std::uint64_t asset{},names{};std::int32_t count{};
    if (!readAt(r,mesh,PlayerLayout::asset,asset) || !readablePointer(asset)
        || !readAt(r,asset,PlayerLayout::referenceArray,names) || !readablePointer(names)
        || !readAt(r,asset,PlayerLayout::referenceCount,count) || count<=0 || count>1024) return rig;
    for (int i=0;i<count;++i) {
        std::uint32_t id{};
        if (!readAt(r,names,std::uint64_t(i)*16,id)) continue;
        const int slot=jointSlot(readName(r,pool,id));
        if (slot>=0 && rig[static_cast<std::size_t>(slot)]<0) rig[static_cast<std::size_t>(slot)]=i;
    }
    return rig;
}
class PlayerReaderCache {
    struct Rig {
        std::uint64_t names{};int count{},next{};std::uint64_t scannedFrame{};
        std::array<int,15> indices;
        Rig() { indices.fill(-1); }
    };
    std::uint64_t world_{},pool_{},frame_{};
    std::unordered_map<std::uint32_t,std::string> names_;
    std::unordered_map<std::uint64_t,Rig> rigs_;
public:
    void beginFrame(std::uint64_t world,std::uint64_t pool) {
        if (world_!=world || pool_!=pool) { names_.clear();rigs_.clear();world_=world;pool_=pool; }
        if (++frame_==0) { rigs_.clear();frame_=1; }
    }
    template<class Reader> std::string name(Reader& r,std::uint64_t pool,std::uint32_t id) {
        const auto it=names_.find(id);
        if (it!=names_.end()) return it->second;
        auto value=readName(r,pool,id);
        if (!value.empty()) {
            if (names_.size()>=4096) names_.clear();
            names_.emplace(id,value);
        }
        return value;
    }
    template<class Reader> std::array<int,15> rig(Reader& r,std::uint64_t mesh,std::uint64_t pool) {
        std::array<int,15> empty;empty.fill(-1);
        std::uint64_t asset{},array{};std::int32_t count{};
        if (!readAt(r,mesh,PlayerLayout::asset,asset) || !readablePointer(asset)
            || !readAt(r,asset,PlayerLayout::referenceArray,array) || !readablePointer(array)
            || !readAt(r,asset,PlayerLayout::referenceCount,count) || count<=0 || count>1024) return empty;
        auto it=rigs_.find(asset);
        if (it==rigs_.end()) {
            if (rigs_.size()>=128) rigs_.clear();
            it=rigs_.emplace(asset,Rig{}).first;
        }
        auto& value=it->second;
        if (value.names!=array || value.count!=count) { value=Rig{};value.names=array;value.count=count; }
        if (value.scannedFrame==frame_) return value.indices;
        value.scannedFrame=frame_;
        const int end=std::min(count,value.next+24);
        while (value.next<end) {
            std::uint32_t id{};
            if (!readAt(r,array,std::uint64_t(value.next)*16,id)) break;
            if (id) {
                const auto text=name(r,pool,id);
                if (text.empty()) break;
                const auto slot=jointSlot(text);
                if (slot>=0 && value.indices[static_cast<std::size_t>(slot)]<0)
                    value.indices[static_cast<std::size_t>(slot)]=value.next;
            }
            ++value.next;
        }
        return value.indices;
    }
};
template<class Reader> Actor readPlayer(Reader& r,std::uint64_t actor,std::uint64_t pool,
                                      PlayerReaderCache* cache=nullptr) {
    Actor out;out.id=actor;
    std::int32_t team{};
    if (readAt(r,actor,CaptureLayout::team,team) && team>0 && team<=255) out.team=team;
    std::uint64_t mesh{},bones{};std::int32_t count{},capacity{};Transform component{};
    if (!readAt(r,actor,PlayerLayout::mesh,mesh) || !readablePointer(mesh)
        || !readAt(r,mesh,PlayerLayout::boneArray,bones) || !readablePointer(bones)
        || !readAt(r,mesh,PlayerLayout::boneArray+8,count)
        || !readAt(r,mesh,PlayerLayout::boneArray+12,capacity)
        || count<=0 || count>1024 || capacity<count || capacity>4096
        || !readAt(r,mesh,PlayerLayout::componentTransform,component)) return out;
    const auto rig=cache?cache->rig(r,mesh,pool):readRig(r,mesh,pool);
    std::array<std::optional<Vec3>,15> points;
    for (std::size_t i=0;i<rig.size();++i) {
        if (rig[i]<0 || rig[i]>=count) continue;
        Transform bone{};
        if (readAt(r,bones,std::uint64_t(rig[i])*sizeof(Transform),bone))
            points[i]=boneWorld(bone,component);
    }
    static constexpr int edges[][2]={{0,1},{1,2},{1,3},{3,4},{4,5},{1,6},{6,7},{7,8},
                                     {2,9},{9,10},{10,11},{2,12},{12,13},{13,14}};
    for (const auto& edge:edges)
        if (points[edge[0]] && points[edge[1]]) out.skeleton.push_back({*points[edge[0]],*points[edge[1]]});
    return out;
}
template<class Reader> Snapshot buildPlayerSnapshot(Reader& r,std::uint64_t base,const RawCapture& raw,
                                                     Viewport viewport,std::uint64_t timestampMs,
                                                     PlayerReaderCache* cache=nullptr) {
    Snapshot s;s.timestampMs=timestampMs;s.viewport=viewport;
    if (raw.status!=CaptureStatus::ready || !readablePointer(raw.localActor)
        || !readablePointer(raw.world) || raw.actorAddresses.size()>1024
        || !Projector::create(raw.camera,viewport)) return s;
    const auto pool=readNamesPool(r,base);
    if (!pool) return s;
    if (cache) cache->beginFrame(raw.world,pool);
    s.camera=raw.camera;s.localActor=raw.localActor;s.localTeam=raw.localTeam;
    for (auto actor:raw.actorAddresses) {
        if (!readablePointer(actor) || actor==raw.localActor) continue;
        std::uint32_t id{};
        if (!readAt(r,actor,PlayerLayout::actorName,id)
            || !playerName(cache?cache->name(r,pool,id):readName(r,pool,id))) continue;
        auto entry=readPlayer(r,actor,pool,cache);
        if (!entry.skeleton.empty()) s.actors.push_back(std::move(entry));
    }
    const auto check=readWorld(r,base);
    if (check.status!=WorldStatus::ready || check.world!=raw.world) {
        s.actors.clear();return s;
    }
    // These are bounded successive reads, not an atomic game-engine snapshot.
    s.complete=true;
    return s;
}
} // namespace nyx
