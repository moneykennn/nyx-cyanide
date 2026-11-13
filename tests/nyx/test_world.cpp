#include "WorldReader.hpp"
#include <cassert>
#include <cstring>
#include <unordered_map>

struct Memory {
    std::unordered_map<std::uint64_t,std::uint8_t> data;
    std::uint64_t table{};
    unsigned tableReads{};
    bool mutate{};
    void put(std::uint64_t address,const void* p,std::size_t size) {
        const auto* bytes=static_cast<const std::uint8_t*>(p);
        for (std::size_t i=0;i<size;++i) data[address+i]=bytes[i];
    }
    bool read(std::uint64_t address,void* p,std::size_t size) {
        auto* bytes=static_cast<std::uint8_t*>(p);
        if (address==table && size==32 && ++tableReads==2 && mutate) ++data[table];
        for (std::size_t i=0;i<size;++i) {
            const auto it=data.find(address+i);
            if (it==data.end()) return false;
            bytes[i]=it->second;
        }
        return true;
    }
};
static Memory fixture() {
    Memory m;
    const std::uint64_t base=0x100000000ULL,slot=0x140000000ULL,world=0x150000000ULL;
    const auto encoded=base+nyx::WorldLayout::encodedWorldRva;
    const std::array<std::uint32_t,8> offsets{0,7,15,23,31,39,47,55};
    m.table=encoded+0x80;
    m.put(m.table,offsets.data(),sizeof(offsets));
    for (unsigned i=0;i<8;++i) {
        const auto value=std::uint8_t(slot>>(8*i));
        m.put(encoded+offsets[i],&value,1);
    }
    m.put(slot,&world,sizeof(world));
    return m;
}
int main() {
    using namespace nyx;
    constexpr auto base=0x100000000ULL;
    auto m=fixture();const auto result=readWorld(m,base);
    assert(result.status==WorldStatus::ready);
    assert(result.slot==0x140000000ULL && result.world==0x150000000ULL);
    // Confirms that slot is dereferenced: it must never be returned as UWorld.
    assert(result.world!=result.slot);
    m=fixture();m.data.erase(0x140000000ULL);
    assert(readWorld(m,base).status==WorldStatus::unavailable);
    m=fixture();m.mutate=true;
    assert(readWorld(m,base).status==WorldStatus::changed);
    m=fixture();const std::uint32_t bad=0xffffffffU;m.put(m.table,&bad,sizeof(bad));
    assert(readWorld(m,base).status==WorldStatus::invalid);
    m=fixture();assert(readWorld(m,0).status==WorldStatus::invalid);
    const std::uint64_t zero=0;m.put(0x140000000ULL,&zero,sizeof(zero));
    assert(readWorld(m,base).status==WorldStatus::invalid);
}
