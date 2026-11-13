#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>

namespace nyx {
// Reader::read(address, destination, size) must report full reads, not zero-fill
// failed reads. This module has no process-access or exploit implementation.
enum class WorldStatus { ready, unavailable, invalid, changed };
struct WorldResult {
    WorldStatus status{WorldStatus::unavailable};
    std::uint64_t slot{}, world{};
};
struct WorldLayout {
    // Confirmed in the supplied 4.7 binary, not in an installed App Store build.
    static constexpr std::uint64_t encodedWorldRva=0xB0F3AA0;
    static constexpr std::uint64_t permutationOffset=0x80;
    static constexpr std::uint32_t maxByteOffset=0x10000;
};
inline bool readablePointer(std::uint64_t value) {
    return value>=0x100000000ULL && value<0x3000000000ULL && (value&7U)==0;
}
inline bool addAddress(std::uint64_t a, std::uint64_t b, std::uint64_t& result) {
    if (a>std::numeric_limits<std::uint64_t>::max()-b) return false;
    result=a+b;
    return true;
}
template<class Reader>
WorldResult readWorld(Reader& reader,std::uint64_t imageBase) {
    if (!readablePointer(imageBase)) return {WorldStatus::invalid,0,0};
    std::uint64_t encoded{},table{};
    if (!addAddress(imageBase,WorldLayout::encodedWorldRva,encoded)
        || !addAddress(encoded,WorldLayout::permutationOffset,table))
        return {WorldStatus::invalid,0,0};
    std::array<std::uint32_t,8> offsets{},after{};
    if (!reader.read(table,offsets.data(),sizeof(offsets))) return {};
    std::array<std::uint8_t,8> bytes{},verified{};
    for (std::size_t i=0;i<offsets.size();++i) {
        if (offsets[i]>WorldLayout::maxByteOffset) return {WorldStatus::invalid,0,0};
        std::uint64_t address{};
        if (!addAddress(encoded,offsets[i],address)) return {WorldStatus::invalid,0,0};
        if (!reader.read(address,&bytes[i],1)) return {};
    }
    std::uint64_t slot{};
    for (std::size_t i=0;i<bytes.size();++i) slot|=std::uint64_t(bytes[i])<<(8*i);
    if (!readablePointer(slot)) return {WorldStatus::invalid,0,0};
    std::uint64_t world{};
    if (!reader.read(slot,&world,sizeof(world))) return {};
    if (!readablePointer(world)) return {WorldStatus::invalid,0,0};
    // Reject an observed concurrent update; repeated reads are not an atomic
    // snapshot and cannot detect every possible ABA change.
    if (!reader.read(table,after.data(),sizeof(after))) return {};
    if (offsets!=after) return {WorldStatus::changed,0,0};
    for (std::size_t i=0;i<offsets.size();++i)
        if (!reader.read(encoded+offsets[i],&verified[i],1)) return {};
    if (bytes!=verified) return {WorldStatus::changed,0,0};
    std::uint64_t worldAfter{};
    if (!reader.read(slot,&worldAfter,sizeof(worldAfter))) return {};
    if (worldAfter!=world) return {WorldStatus::changed,0,0};
    return {WorldStatus::ready,slot,world};
}
} // namespace nyx
